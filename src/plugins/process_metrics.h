#pragma once

#include <optional>
#if defined(__APPLE__)
#include <mach/mach.h>
#include <sys/resource.h>
#endif

namespace afterhours::profiling {
struct ProcessMetrics {
    std::optional<double> cpu_seconds;
    std::optional<double> resident_mb;
};
inline ProcessMetrics process_metrics() {
    ProcessMetrics result;
#if defined(__APPLE__)
    rusage usage{};
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
        result.cpu_seconds = usage.ru_utime.tv_sec + usage.ru_utime.tv_usec / 1e6 +
                             usage.ru_stime.tv_sec + usage.ru_stime.tv_usec / 1e6;
    }
    mach_task_basic_info_data_t info{};
    mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO,
                  reinterpret_cast<task_info_t>(&info), &count) == KERN_SUCCESS)
        result.resident_mb = static_cast<double>(info.resident_size) / (1024. * 1024.);
#endif
    return result;
}
}
