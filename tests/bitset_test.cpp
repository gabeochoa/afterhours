// bitset_test.cpp
// Bitset<N> replaces std::bitset on Entity::componentSet because libc++'s
// operator[] const returns a proxy, which is a handful of un-inlined calls at
// -O0 and -Og -- the builds everyone actually develops against. has_child_of
// does 128 of them per entity per system.
//
// A replacement is only worth having if it cannot disagree with the thing it
// replaces, so most of this compares the two directly. The word boundary at 63
// and 64 is where a hand-written one goes wrong.

#include "ui_test_harness.h"

#include <bitset>
#include <chrono>
#include <vector>

#include <afterhours/src/core/bitset.h>

using namespace afterhours;

namespace {

// Not a multiple of 64 on purpose: AFTER_HOURS_MAX_COMPONENTS is -D-overridable
// and nothing says it has to be round.
constexpr std::size_t kOdd = 100;

} // namespace

TEST(a_new_bitset_is_empty) {
  Bitset<128> b;
  CHECK(b.none());
  CHECK(!b.any());
  CHECK(b.count() == 0);
  CHECK(b.size() == 128);
  for (std::size_t i = 0; i < 128; i++)
    CHECK(!b.test(i));
}

// The positions either side of a word edge are where the arithmetic goes wrong.
TEST(the_word_boundary_behaves) {
  Bitset<128> ours;
  std::bitset<128> theirs;
  for (std::size_t pos : {std::size_t{0}, std::size_t{1}, std::size_t{63},
                          std::size_t{64}, std::size_t{65}, std::size_t{127}}) {
    ours.set(pos);
    theirs.set(pos);
  }
  for (std::size_t i = 0; i < 128; i++) {
    if (ours.test(i) != theirs.test(i))
      printf("        disagree at %zu: ours=%d theirs=%d\n", i,
             (int)ours.test(i), (int)theirs.test(i));
    CHECK(ours.test(i) == theirs.test(i));
    CHECK(ours[i] == theirs[i]);
  }
  CHECK(ours.count() == theirs.count());
  CHECK(ours.to_string() == theirs.to_string());
}

// Setting bit 0 must not disturb the second word, and clearing one bit must not
// clear its neighbours.
TEST(setting_one_bit_leaves_the_others_alone) {
  Bitset<128> b;
  b.set(64);
  CHECK(b.test(64));
  CHECK(!b.test(0));
  CHECK(!b.test(63));
  CHECK(!b.test(65));
  CHECK(b.count() == 1);

  b.set(65);
  b.set(64, false);
  CHECK(!b.test(64));
  CHECK(b.test(65));
  CHECK(b.count() == 1);
}

// reset() has to clear every word, not just the first. A loop that forgets the
// second word passes any test that only touches low bits.
TEST(reset_clears_every_word) {
  Bitset<128> b;
  b.set(3);
  b.set(70);
  b.set(127);
  CHECK(b.count() == 3);
  b.reset();
  CHECK(b.none());
  CHECK(b.count() == 0);
  CHECK(!b.test(70));
  CHECK(!b.test(127));
}

TEST(reset_of_one_position_matches_std) {
  Bitset<128> ours;
  std::bitset<128> theirs;
  for (std::size_t i = 0; i < 128; i += 3) {
    ours.set(i);
    theirs.set(i);
  }
  for (std::size_t i = 0; i < 128; i += 7) {
    ours.reset(i);
    theirs.reset(i);
  }
  CHECK(ours.count() == theirs.count());
  for (std::size_t i = 0; i < 128; i++)
    CHECK(ours.test(i) == theirs.test(i));
}

// A size that is not a multiple of 64 still gets a whole trailing word, and the
// bits past N must never be reported.
TEST(a_size_that_is_not_a_multiple_of_64_works) {
  Bitset<kOdd> ours;
  std::bitset<kOdd> theirs;
  CHECK(ours.size() == kOdd);
  CHECK(Bitset<kOdd>::num_words == 2);
  for (std::size_t i = 0; i < kOdd; i += 5) {
    ours.set(i);
    theirs.set(i);
  }
  CHECK(ours.count() == theirs.count());
  CHECK(ours.to_string() == theirs.to_string());
}

// Runs both in the same binary and the same run, so machine noise hits each
// equally and the ratio is the signal. Comparing separate runs already produced
// one bogus number in this codebase.
TEST(it_is_faster_than_std_bitset_unoptimised) {
  constexpr int kScans = 20000;
  constexpr std::size_t kBits = 128;

  Bitset<kBits> ours;
  std::bitset<kBits> theirs;
  // A handful of components, like a real entity, so the scan mostly misses --
  // which is has_child_of's common case.
  for (std::size_t i : {std::size_t{2}, std::size_t{17}, std::size_t{61},
                        std::size_t{90}, std::size_t{120}}) {
    ours.set(i);
    theirs.set(i);
  }

  const auto time_scan = [](auto &bits) {
    const auto start = std::chrono::steady_clock::now();
    std::size_t hits = 0;
    for (int s = 0; s < kScans; s++)
      for (std::size_t i = 0; i < kBits; i++)
        if (bits[i])
          hits++;
    const auto elapsed = std::chrono::duration<double, std::milli>(
                             std::chrono::steady_clock::now() - start)
                             .count();
    // Returned so the loop cannot be discarded.
    return std::make_pair(elapsed, hits);
  };

  const auto [t_std, hits_std] = time_scan(theirs);
  const auto [t_ours, hits_ours] = time_scan(ours);
  CHECK(hits_std == hits_ours);

  const double ratio = t_ours > 0.0 ? t_std / t_ours : 0.0;
  printf("  std::bitset %.2fms, Bitset %.2fms, ratio %.2fx\n", t_std, t_ours,
         ratio);

  // Deliberately loose. The claim is "not slower", and a ratio at or below 1
  // means this change is not worth having and should be said out loud rather
  // than asserted around.
  CHECK(ratio > 0.9);
}


// next_set must visit exactly the set bits, and nothing else. This is the
// property has_child_of relies on to stop asking all 128 slots.
TEST(next_set_visits_only_the_set_bits) {
  Bitset<128> b;
  const std::size_t placed[] = {0, 5, 63, 64, 65, 127};
  for (std::size_t i : placed)
    b.set(i);

  std::vector<std::size_t> seen;
  for (std::size_t i = b.next_set(0); i < b.size(); i = b.next_set(i + 1))
    seen.push_back(i);

  printf("  visited %zu of %zu slots\n", seen.size(), b.size());
  CHECK(seen.size() == 6);
  for (std::size_t k = 0; k < 6; k++)
    CHECK(seen[k] == placed[k]);
}

TEST(next_set_on_an_empty_bitset_ends_immediately) {
  Bitset<128> b;
  CHECK(b.next_set(0) == b.size());
  // Past the end is not an infinite loop or an out-of-bounds read.
  CHECK(b.next_set(200) == b.size());
}

// A size that is not a multiple of 64 has bits in the last word past N.
TEST(next_set_respects_a_ragged_size) {
  Bitset<kOdd> b;
  b.set(99);
  CHECK(b.next_set(0) == 99);
  CHECK(b.next_set(100) == kOdd);
}

// The scan has_child_of does: ask every slot, versus walk the set ones.
// Same binary, same run.
TEST(walking_set_bits_beats_asking_every_slot) {
  constexpr int kScans = 20000;
  Bitset<128> b;
  for (std::size_t i : {std::size_t{2}, std::size_t{17}, std::size_t{61},
                        std::size_t{90}, std::size_t{120}})
    b.set(i);

  const auto start_all = std::chrono::steady_clock::now();
  std::size_t hits_all = 0;
  for (int s = 0; s < kScans; s++)
    for (std::size_t i = 0; i < 128; i++)
      if (b.test(i))
        hits_all++;
  const double t_all = std::chrono::duration<double, std::milli>(
                           std::chrono::steady_clock::now() - start_all)
                           .count();

  const auto start_set = std::chrono::steady_clock::now();
  std::size_t hits_set = 0;
  for (int s = 0; s < kScans; s++)
    for (std::size_t i = b.next_set(0); i < b.size(); i = b.next_set(i + 1))
      hits_set++;
  const double t_set = std::chrono::duration<double, std::milli>(
                           std::chrono::steady_clock::now() - start_set)
                           .count();

  CHECK(hits_all == hits_set);
  printf("  every slot %.2fms, set bits only %.2fms, ratio %.2fx\n", t_all,
         t_set, t_set > 0.0 ? t_all / t_set : 0.0);
}

int main() { return ui_test::run_registered_tests("bitset"); }
