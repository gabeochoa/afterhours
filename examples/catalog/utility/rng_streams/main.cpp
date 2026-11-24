#include <cstdio>
#include <string>

#include "../../../../src/random_engine.h"

using afterhours::RNGStream;

int main() {
  RNGStream shop("run-42-shop");
  RNGStream battle("run-42-battle");

  for (int i = 0; i < 3; i++)
    printf("shop offer %d\n", shop.get_int(1, 100));

  const std::string save_file = battle.save();
  const int next_hit = battle.get_int(1, 20);
  printf("battle roll %d\n", next_hit);

  auto resumed = RNGStream::load(save_file);
  if (!resumed)
    return 1;
  const int replayed = resumed->get_int(1, 20);
  printf("after reload, battle roll %d again\n", replayed);

  return replayed == next_hit ? 0 : 1;
}
