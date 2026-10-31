#pragma once
#include "model/QueryModel.hpp"
#include "event_loop/Loop.hpp"
#include <utility>
#include <vector>

struct QueryE2EModelFixture {
  batbox::event_loop::Loop* owner=nullptr;
  std::string baseUrl;
  int uuid=0,costCalls=0,fallbacks=0;
  std::vector<std::pair<std::string,std::string>> localLogs,localDiagnostics;
  std::optional<std::string> lastMainRequestId;
  batbox::model::QueryServices makeServices();
};
