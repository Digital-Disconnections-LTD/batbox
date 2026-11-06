#pragma once
#include "attachments/Aggregate.hpp"
#include "bootstrap/PoetryState.hpp"

namespace batbox::attachments {
struct LeafServices {
  bootstrap::PoetryState* poetryState=nullptr;
  std::function<std::string()> localIsoDate;
  std::function<Json()> getSettings;
};
void bindLeafCollectors(Services& aggregation,LeafServices leaves);
} // namespace batbox::attachments
