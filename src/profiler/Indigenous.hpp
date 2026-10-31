#pragma once
#include "profiler/HeadlessProfiler.hpp"
#include "profiler/QueryProfiler.hpp"

namespace batbox::profiler {
double indigenousPerformanceNowMs();
MemoryUsage indigenousMemoryUsage();
std::shared_ptr<Timeline> makeIndigenousTimeline();
QueryProfilerServices indigenousQueryProfilerServices(
    std::shared_ptr<Timeline> timeline,
    std::function<void(const std::string&)> localDebug);
HeadlessProfilerServices indigenousHeadlessProfilerServices(
    std::shared_ptr<Timeline> timeline,
    std::function<bool()> isNonInteractiveFlight,
    std::function<void(const std::string&)> localDebug);
}
