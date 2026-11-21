#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace afterhours {

// Optional ordered event queue (AH-04). Systems emit with an
// application-defined phase; delivery is sorted by phase and stable within a
// phase, so replay/netcode runs agree regardless of emission order. Call
// drain() at an explicit flush point (e.g. between named system phases);
// events emitted while draining form the next batch and are drained in turn.
template <typename Event, typename Phase = int>
struct EventQueue {
    struct DrainResult {
        std::size_t delivered = 0;
        bool limit_exceeded = false;  // work left pending; drain again
    };

    // Returns the monotonic emission sequence (same-phase tie-break order).
    std::uint64_t emit(Phase phase, Event event) {
        const std::uint64_t seq = next_seq_++;
        pending_.push_back(Entry{phase, seq, std::move(event)});
        return seq;
    }

    template <typename Handler>
    DrainResult drain(Handler &&handler, std::size_t max_work = 4096) {
        DrainResult result;
        while (!pending_.empty()) {
            std::vector<Entry> batch;
            batch.swap(pending_);
            std::stable_sort(batch.begin(), batch.end(),
                             [](const Entry &a, const Entry &b) {
                                 return a.phase < b.phase;
                             });
            for (std::size_t i = 0; i < batch.size(); ++i) {
                if (result.delivered >= max_work) {
                    result.limit_exceeded = true;
                    // Keep the undelivered tail ahead of cascaded emits.
                    pending_.insert(pending_.begin(),
                                    std::make_move_iterator(batch.begin() +
                                                            (long) i),
                                    std::make_move_iterator(batch.end()));
                    return result;
                }
                ++result.delivered;
                handler(batch[i].phase, batch[i].event);
            }
        }
        return result;
    }

    [[nodiscard]] std::size_t pending() const { return pending_.size(); }
    [[nodiscard]] bool empty() const { return pending_.empty(); }
    [[nodiscard]] std::uint64_t next_sequence() const { return next_seq_; }
    void clear() { pending_.clear(); }

   private:
    struct Entry {
        Phase phase;
        std::uint64_t seq;
        Event event;
    };
    std::vector<Entry> pending_;
    std::uint64_t next_seq_ = 0;
};

}  // namespace afterhours
