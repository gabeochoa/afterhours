#pragma once

// NPC line skeleton (Pharmasea G-P21): the join / advance / at-front bookkeeping
// every game with a queue re-implements, without the state machines. The game
// owns walking, ordering and serving; this owns who's in line, in what order,
// and who just reached the front.
//
//   NpcQueue<EntityID> line;
//   line.set_at_front_callback([](EntityID id) { /* send that npc to the counter */ });
//   line.join(npc);                                  // back of the line
//   line.join_at(npc, 0);                            // a specific slot
//   auto serving = line.advance();                   // front npc pops; next front fires the callback
//   Vector2Type p = npc_queue::slot_position(points, line.position_of(npc).value_or(0), 24.f);
//
// A queue member is any handle with == : EntityID, a generation-checked handle,
// a network peer id. Duplicate joins are a no-op (join returns false), so a
// re-announcement can't double-book a slot. The at-front callback fires
// synchronously after the change lands, so the queue is already consistent
// when it runs; it fires only when the front becomes a member, not on join
// behind, leave of a non-front, or clear.

#include <cmath>
#include <cstddef>
#include <functional>
#include <optional>
#include <vector>

#include "../developer.h"

namespace afterhours {

template <typename Handle> struct NpcQueue {
  using AtFrontCallback = std::function<void(Handle)>;

  void set_at_front_callback(AtFrontCallback fn) { callback_ = std::move(fn); }

  // Append at the back; false (no-op, order untouched) if already in line.
  bool join(Handle h) {
    if (position_of(h).has_value()) return false;
    const auto before = front();
    members_.push_back(h);
    notify_if_front_changed(before);
    return true;
  }

  // Insert at slot `index` (0 = front); an index past the back clamps to it.
  bool join_at(Handle h, size_t index) {
    if (position_of(h).has_value()) return false;
    if (index > members_.size()) index = members_.size();
    const auto before = front();
    members_.insert(members_.begin() + static_cast<std::ptrdiff_t>(index), h);
    notify_if_front_changed(before);
    return true;
  }

  // Remove from anywhere in the line; false if not a member.
  bool leave(Handle h) {
    const auto pos = position_of(h);
    if (!pos.has_value()) return false;
    const auto before = front();
    members_.erase(members_.begin() + static_cast<std::ptrdiff_t>(*pos));
    notify_if_front_changed(before);
    return true;
  }

  // Pop the front and return it; the next member's at-front callback fires.
  std::optional<Handle> advance() {
    if (members_.empty()) return std::nullopt;
    const auto before = front();
    Handle popped = members_.front();
    members_.erase(members_.begin());
    notify_if_front_changed(before);
    return popped;
  }

  [[nodiscard]] std::optional<Handle> front() const {
    if (members_.empty()) return std::nullopt;
    return members_.front();
  }

  [[nodiscard]] bool at_front(Handle h) const {
    return front().has_value() && *front() == h;
  }

  [[nodiscard]] std::optional<size_t> position_of(Handle h) const {
    for (size_t i = 0; i < members_.size(); i++)
      if (members_[i] == h) return i;
    return std::nullopt;
  }

  [[nodiscard]] size_t size() const { return members_.size(); }
  [[nodiscard]] bool empty() const { return members_.empty(); }
  [[nodiscard]] const std::vector<Handle> &members() const { return members_; }

  void clear() { members_.clear(); }

 private:
  void notify_if_front_changed(const std::optional<Handle> &before) {
    const auto now = front();
    if (now.has_value() && (!before.has_value() || *now != *before) && callback_)
      callback_(*now);
  }

  std::vector<Handle> members_;
  AtFrontCallback callback_;
};

namespace npc_queue {

// Where slot `index` stands along a line of points the caller supplies (the
// shopfront, the till, a curved queue): `index * spacing` units of arc length
// from the first point. Past the end of the line the slot is the last point;
// an empty line has no slots, so it answers {0, 0}.
inline Vector2Type slot_position(const std::vector<Vector2Type> &line, size_t index,
                                 float spacing) {
  if (line.empty()) return Vector2Type{0.f, 0.f};
  float remaining = static_cast<float>(index) * spacing;
  for (size_t i = 0; i + 1 < line.size(); i++) {
    const float dx = line[i + 1].x - line[i].x;
    const float dy = line[i + 1].y - line[i].y;
    const float seg = std::sqrt(dx * dx + dy * dy);
    if (remaining <= seg) {
      const float t = seg > 0.f ? remaining / seg : 0.f;
      return Vector2Type{line[i].x + dx * t, line[i].y + dy * t};
    }
    remaining -= seg;
  }
  return line.back();
}

} // namespace npc_queue

} // namespace afterhours
