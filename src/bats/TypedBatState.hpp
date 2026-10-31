#pragma once

#include "bats/ForkedBat.hpp"

namespace batbox::bats {
std::function<app_state::AppStateRef()> effectiveTypedBatGetAppState(
  const BatContext& context);
std::function<app_state::AppStateRef()> createTypedBatGetAppState(
  std::function<app_state::AppStateRef()> parentGetAppState,
  Json batDefinition,bool isAsync,
  std::optional<bool> canShowPermissionPoetryEntries,
  std::optional<Json> allowedGadgets,
  std::function<bool(const std::string&)> feature);
} // namespace batbox::bats
