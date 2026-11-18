#include <cstdio>
#include <string>

#include "../../../../src/local_time.h"

int main() {
  const auto now = afterhours::local_time_now();
  if (!now)
    return 1;
  printf("%s %s %d   %02d:%02d:%02d\n", std::string(now->weekday_name()).c_str(),
         std::string(now->month_name()).c_str(), now->day, now->hour,
         now->minute, now->second);
  return 0;
}
