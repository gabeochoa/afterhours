#pragma once

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#include "../frame_loop.h"
#include "../logging.h"

namespace afterhours::jobs {

enum class Priority { Foreground, Background };

using JobId = uint32_t;

class Pool {
  struct Job {
    using RunFn = std::function<std::function<void()>(std::stop_token)>;

    JobId id;
    std::stop_source stop;
    RunFn run;
  };
  struct Running {
    JobId id;
    std::stop_source stop;
  };
  struct Done {
    using Fn = std::function<void()>;

    JobId id;
    Fn deliver;
  };

  std::mutex mutex;
  std::condition_variable ready;
  std::deque<Job> foreground;
  std::deque<Job> background;
  std::deque<Done> mailbox;
  std::vector<std::optional<Running>> running;
  std::vector<std::thread> workers;
  std::thread::id owner = std::this_thread::get_id();
  size_t capacity;
  size_t background_limit;
  size_t background_active = 0;
  JobId next_id = 1;
  bool stopping = false;

  void work(size_t slot) {
    std::unique_lock lock(mutex);
    for (;;) {
      ready.wait(lock, [&] {
        return stopping || !foreground.empty() ||
               (!background.empty() && background_active < background_limit);
      });
      if (stopping) return;
      const bool is_background = foreground.empty();
      auto &queue = is_background ? background : foreground;
      Job job = std::move(queue.front());
      queue.pop_front();
      if (is_background) background_active++;
      running[slot] = Running{job.id, job.stop};
      lock.unlock();
      Done::Fn deliver = job.run(job.stop.get_token());
      lock.lock();
      running[slot].reset();
      if (is_background) background_active--;
      if (!stopping && !job.stop.stop_requested()) {
        mailbox.push_back({job.id, std::move(deliver)});
        frame_loop::request_frame();
      }
      ready.notify_all();
    }
  }

  void require_owner(const char *what) const {
    if (std::this_thread::get_id() == owner) return;
    log_error("jobs::Pool::{} called off the thread that created the pool",
              what);
    std::abort();
  }

public:
  Pool(size_t threads, size_t queue_capacity)
      : running(threads), capacity(queue_capacity),
        background_limit(threads > 1 ? threads - 1 : 1) {
    if (threads == 0 || queue_capacity == 0) {
      log_error("jobs::Pool needs at least one thread and one queue slot, "
                "got {} threads and {} slots",
                threads, queue_capacity);
      std::abort();
    }
    for (size_t slot = 0; slot < threads; slot++)
      workers.emplace_back([this, slot] { work(slot); });
  }
  Pool(const Pool &) = delete;
  Pool &operator=(const Pool &) = delete;
  ~Pool() { shutdown(); }

  template <class Work, class Deliver>
  std::optional<JobId> submit(Work work, Deliver deliver,
                              Priority priority = Priority::Foreground) {
    using T = std::invoke_result_t<Work &, std::stop_token>;
    static_assert(!std::is_void_v<T>, "a job must return its result");
    Job::RunFn run = [work = std::move(work), deliver = std::move(deliver)](
                         std::stop_token stop) mutable -> Done::Fn {
      auto value = std::make_shared<T>(work(stop));
      return [value, deliver = std::move(deliver)]() mutable {
        deliver(std::move(*value));
      };
    };
    std::lock_guard lock(mutex);
    const size_t queued = foreground.size() + background.size();
    const size_t limit = priority == Priority::Background
                             ? capacity - capacity / 4
                             : capacity;
    if (stopping || queued >= limit) return std::nullopt;
    const JobId id = next_id++;
    auto &queue = priority == Priority::Background ? background : foreground;
    queue.push_back({id, std::stop_source{}, std::move(run)});
    ready.notify_all();
    return id;
  }

  void cancel(JobId id) {
    std::lock_guard lock(mutex);
    for (auto *queue : {&foreground, &background})
      std::erase_if(*queue, [id](const Job &job) { return job.id == id; });
    for (auto &slot : running)
      if (slot && slot->id == id) slot->stop.request_stop();
    std::erase_if(mailbox, [id](const Done &done) { return done.id == id; });
  }

  size_t drain() {
    require_owner("drain");
    size_t pending;
    {
      std::lock_guard lock(mutex);
      pending = mailbox.size();
    }
    size_t delivered = 0;
    for (; delivered < pending; delivered++) {
      Done::Fn deliver;
      {
        std::lock_guard lock(mutex);
        if (mailbox.empty()) break;
        deliver = std::move(mailbox.front().deliver);
        mailbox.pop_front();
      }
      deliver();
    }
    return delivered;
  }

  void shutdown() {
    require_owner("shutdown");
    {
      std::lock_guard lock(mutex);
      stopping = true;
      foreground.clear();
      background.clear();
      mailbox.clear();
      for (auto &slot : running)
        if (slot) slot->stop.request_stop();
    }
    ready.notify_all();
    for (auto &worker : workers)
      if (worker.joinable()) worker.join();
  }
};

} // namespace afterhours::jobs
