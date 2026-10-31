#pragma once
#include "attachments/Aggregate.hpp"
#include "bootstrap/Costs.hpp"
#include "bootstrap/PoetryState.hpp"

namespace batbox::attachments {
struct LeafServices {
  bootstrap::PoetryState* poetryState=nullptr;
  bootstrap::CostState* costState=nullptr;
  std::function<std::string()> localIsoDate;
  std::function<bool(const std::string&)> feature;
  std::function<bool()> kairosActive,ultrathinkEnabled;
  std::function<void(const Json&,const std::string&)> flushOnDateChange;
  std::function<Json()> getSettings;
};
bool hasUltrathinkKeyword(const std::string& text);
void bindLeafCollectors(Services& aggregation,LeafServices leaves);
} // namespace batbox::attachments
