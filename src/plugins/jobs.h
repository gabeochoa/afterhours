#pragma once

// Bounded background jobs. See docs/plugins.md "Background jobs" for the
// contract. Header-only; no ECS dependency. Job functions run on worker
// threads and must return owned values instead of touching entities.

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <exception>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace afterhours::jobs {

enum class Priority { Foreground, Background };

// Rejected: never queued (queue full or executor shut down).
// Pending: accepted and not finished.
// Cancelled: cancellation was requested before the job produced a value.
enum class Status { Rejected, Pending, Cancelled, Completed, Failed };

// Identifies which consumer asked and which of its requests this is, so the
// main thread can drop a completion after its view or scene was replaced.
struct Ticket {
  std::uint64_t owner = 0;
  std::uint64_t generation = 0;
  bool operator==(const Ticket &) const = default;
};

// One per consumer. issue() stamps a new request; is_current() is false for
// tickets issued before the latest issue() or invalidate().
class Generation {
public:
  explicit Generation(std::uint64_t owner) : owner_(owner) {}
  Ticket issue() { return {owner_, ++generation_}; }
  void invalidate() { ++generation_; }
  bool is_current(Ticket ticket) const {
    return ticket.owner == owner_ && ticket.generation == generation_;
  }

private:
  std::uint64_t owner_;
  std::uint64_t generation_ = 0;
};

struct SubmitOptions {
  Priority priority = Priority::Foreground;
  // Must-complete jobs ignore cancel() and run during shutdown.
  bool must_complete = false;
  Ticket ticket{};
};

namespace detail {

struct JobBase {
  std::mutex mutex;
  std::condition_variable finished;
  Status status = Status::Pending;
  bool started = false;
  std::exception_ptr error;
  std::stop_source stop;
  bool must_complete = false;
  Ticket ticket{};

  virtual ~JobBase() = default;
  virtual void run() = 0;

  void cancel() {
    if (must_complete) return;
    std::lock_guard lock(mutex);
    stop.request_stop();
    if (!started && status == Status::Pending) settle(Status::Cancelled);
  }
  // Caller holds mutex.
  void settle(Status next) {
    status = next;
    finished.notify_all();
  }
  bool begin() {
    std::lock_guard lock(mutex);
    if (status != Status::Pending) return false;
    started = true;
    return true;
  }
};

template <class T> struct Result : JobBase {
  std::optional<T> value;
};

template <class T, class Fn> struct Job final : Result<T> {
  Fn fn;
  explicit Job(Fn f) : fn(std::move(f)) {}
  using JobBase::begin, JobBase::mutex, JobBase::settle, JobBase::stop,
      JobBase::must_complete, JobBase::error;
  using Result<T>::value;

  void run() override {
    if (!begin()) return;
    try {
      T result = fn(stop.get_token());
      std::lock_guard lock(mutex);
      if (!must_complete && stop.stop_requested()) return settle(Status::Cancelled);
      value.emplace(std::move(result));
      settle(Status::Completed);
    } catch (...) {
      std::lock_guard lock(mutex);
      if (!must_complete && stop.stop_requested()) return settle(Status::Cancelled);
      error = std::current_exception();
      settle(Status::Failed);
    }
  }
};

} // namespace detail

// Move-only handle. Destroying or reassigning it cancels a cancellable job.
template <class T> class Handle {
public:
  Handle() = default;
  Handle(Handle &&) noexcept = default;
  Handle &operator=(Handle &&other) noexcept {
    if (this != &other) {
      cancel();
      state_ = std::move(other.state_);
    }
    return *this;
  }
  Handle(const Handle &) = delete;
  Handle &operator=(const Handle &) = delete;
  ~Handle() { cancel(); }

  bool valid() const { return state_ != nullptr; }
  bool accepted() const { return valid() && status() != Status::Rejected; }
  Status status() const {
    std::lock_guard lock(state_->mutex);
    return state_->status;
  }
  bool finished() const { return status() != Status::Pending; }
  Ticket ticket() const { return state_->ticket; }
  void cancel() {
    if (state_) state_->cancel();
  }
  // Blocks up to `timeout`; returns finished().
  template <class Rep, class Period>
  bool wait_for(std::chrono::duration<Rep, Period> timeout) const {
    std::unique_lock lock(state_->mutex);
    return state_->finished.wait_for(lock, timeout, [&] {
      return state_->status != Status::Pending;
    });
  }
  // Moves the value out once Completed; empty otherwise or on a second call.
  std::optional<T> take() {
    std::lock_guard lock(state_->mutex);
    if (state_->status != Status::Completed) return std::nullopt;
    return std::exchange(state_->value, std::nullopt);
  }
  // The exception a Failed job threw; null otherwise.
  std::exception_ptr error() const {
    std::lock_guard lock(state_->mutex);
    return state_->error;
  }

private:
  friend class Executor;
  explicit Handle(std::shared_ptr<detail::Result<T>> state)
      : state_(std::move(state)) {}
  std::shared_ptr<detail::Result<T>> state_;
};

struct Options {
  std::size_t threads = 4;
  // Total queued (not running) jobs. Foreground may fill it.
  std::size_t queue_capacity = 64;
  // Background admission stops here, keeping the rest for foreground work.
  std::size_t background_queue_capacity = 48;
};

class Executor {
public:
  explicit Executor(Options options = {})
      : options_(options), active_(std::max<std::size_t>(1, options.threads)),
        // One worker is reserved for foreground work when there are two or
        // more, so a backlog of background jobs cannot delay foreground ones.
        background_workers_(active_.size() > 1 ? active_.size() - 1 : 1) {
    for (std::size_t i = 0; i < active_.size(); ++i)
      workers_.emplace_back([this, i] { work(i); });
  }
  Executor(const Executor &) = delete;
  Executor &operator=(const Executor &) = delete;
  ~Executor() { shutdown(); }

  // fn: T(std::stop_token). Never blocks; a full queue or a shut-down executor
  // returns a Rejected handle and fn is destroyed without running.
  template <class Fn>
  auto submit(Fn fn, SubmitOptions submit = {})
      -> Handle<std::invoke_result_t<Fn &, std::stop_token>> {
    using T = std::invoke_result_t<Fn &, std::stop_token>;
    static_assert(!std::is_void_v<T>, "jobs return an owned value");
    auto state = std::make_shared<detail::Job<T, Fn>>(std::move(fn));
    state->must_complete = submit.must_complete;
    state->ticket = submit.ticket;
    {
      std::lock_guard lock(mutex_);
      const std::size_t queued = foreground_.size() + background_.size();
      const std::size_t limit = submit.priority == Priority::Background
                                    ? options_.background_queue_capacity
                                    : options_.queue_capacity;
      if (stopping_ || queued >= limit) {
        state->status = Status::Rejected;
        return Handle<T>(std::move(state));
      }
      (submit.priority == Priority::Foreground ? foreground_ : background_)
          .push_back(state);
    }
    ready_.notify_all();
    return Handle<T>(std::move(state));
  }

  std::size_t queued() const {
    std::lock_guard lock(mutex_);
    return foreground_.size() + background_.size();
  }

  // Stops admission, cancels running and queued cancellable jobs (queued ones
  // finish as Cancelled without running), runs queued must-complete jobs,
  // then joins. Idempotent. Call from the owning thread, never from a job.
  void shutdown() {
    {
      std::lock_guard lock(mutex_);
      stopping_ = true;
      for (auto &job : active_)
        if (job) job->cancel();
      for (auto *queue : {&foreground_, &background_})
        for (auto &job : *queue) job->cancel();
    }
    ready_.notify_all();
    for (auto &worker : workers_)
      if (worker.joinable()) worker.join();
  }

private:
  using Ptr = std::shared_ptr<detail::JobBase>;
  Options options_;
  mutable std::mutex mutex_;
  std::condition_variable ready_;
  std::deque<Ptr> foreground_, background_;
  std::vector<Ptr> active_;
  std::vector<std::thread> workers_;
  std::size_t background_workers_;
  std::size_t background_active_ = 0;
  bool stopping_ = false;

  void work(std::size_t index) {
    std::unique_lock lock(mutex_);
    for (;;) {
      auto runnable = [&] {
        return !foreground_.empty() ||
               (!background_.empty() && background_active_ < background_workers_);
      };
      ready_.wait(lock, [&] {
        return runnable() ||
               (stopping_ && foreground_.empty() && background_.empty());
      });
      if (!runnable()) return;
      const bool background = foreground_.empty();
      auto &queue = background ? background_ : foreground_;
      auto job = std::move(queue.front());
      queue.pop_front();
      active_[index] = job;
      if (background) ++background_active_;
      lock.unlock();
      job->run();
      lock.lock();
      active_[index].reset();
      if (background) --background_active_;
      ready_.notify_all();
    }
  }
};

// Shows a busy indicator only once work has been pending for `delay`, so fast
// operations never flash it. start() keeps the original start time while
// already busy, so back-to-back requests do not restart the delay.
struct BusyDelay {
  using Clock = std::chrono::steady_clock;
  Clock::duration delay = std::chrono::milliseconds(150);
  std::optional<Clock::time_point> started;

  void start(Clock::time_point now = Clock::now()) {
    if (!started) started = now;
  }
  void stop() { started.reset(); }
  bool visible(Clock::time_point now = Clock::now()) const {
    return started && now - *started >= delay;
  }
};

} // namespace afterhours::jobs
