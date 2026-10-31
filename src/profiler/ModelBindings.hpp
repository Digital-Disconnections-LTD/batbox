#pragma once
#include "model/QueryModel.hpp"
#include "profiler/HeadlessProfiler.hpp"
#include "profiler/QueryProfiler.hpp"
#include <memory>
#include <stdexcept>

namespace batbox::profiler {
inline void bindModelProfiler(model::QueryServices& services,
    std::shared_ptr<QueryProfiler> query,
    std::shared_ptr<HeadlessProfiler> headless){
  if(!query||!headless)throw std::invalid_argument("Profiler instances are required");
  services.queryCheckpoint=[query](const std::string& name){
    query->queryCheckpoint(name);
  };
  services.headlessProfilerCheckpoint=[headless](const std::string& name){
    headless->headlessProfilerCheckpoint(name);
  };
  services.endQueryProfile=[query]{query->endQueryProfile();};
}
}
