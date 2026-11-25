#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace afterhours {

template <typename Event, typename Phase = int>
struct EventQueue {
    struct DrainResult {
        std::size_t delivered = 0;
        bool limit_exceeded = false;
    };

    std::uint64_t emit(Phase phase, Event event) {
        const std::uint64_t seq = next_seq_++;
        pending_events.push_back(Entry{phase, seq, std::move(event)});
        return seq;
    }

    template <typename Handler>
    DrainResult drain(Handler &&handler, std::size_t max_work = 4096) {
        DrainResult result;
        while (!pending_events.empty()) {
            std::vector<Entry> batch;
            batch.swap(pending_events);
            std::stable_sort(batch.begin(), batch.end(),
                             [](const Entry &a, const Entry &b) {
                                 return a.phase < b.phase;
                             });
            for (std::size_t i = 0; i < batch.size(); ++i) {
                if (result.delivered >= max_work) {
                    result.limit_exceeded = true;
                    pending_events.insert(pending_events.begin(),
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

    [[nodiscard]] std::size_t pending() const { return pending_events.size(); }
    [[nodiscard]] bool empty() const { return pending_events.empty(); }
    [[nodiscard]] std::uint64_t next_sequence() const { return next_seq_; }
    void clear() { pending_events.clear(); }

   private:
    struct Entry {
        Phase phase;
        std::uint64_t seq;
        Event event;
    };
    std::vector<Entry> pending_events;
    std::uint64_t next_seq_ = 0;
};

}
