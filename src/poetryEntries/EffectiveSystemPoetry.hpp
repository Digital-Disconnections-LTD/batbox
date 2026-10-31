#pragma once
#include "execution/GadgetExecution.hpp"

namespace batbox::poetryEntries {
struct MainThreadPoetryBat {
  Json definition=Json::object();
  // Built-ins receive an options-only context; directory bats receive no argument.
  std::function<Json(const execution::Context*)> getSystemPoetry;
};
struct EffectivePoetryParams {
  std::shared_ptr<MainThreadPoetryBat> mainThreadBat;
  execution::Context gadgetUseContext;
  Json customSystemPoetry=Json(Json::value_t::discarded);
  Json defaultSystemPoetry=Json::array();
  Json appendSystemPoetry=Json(Json::value_t::discarded);
  Json overrideSystemPoetry=Json(Json::value_t::discarded);
};
struct EffectivePoetryServices {
  std::function<bool(const std::string&)> feature;
  std::function<bool()> coordinatorModeEnabled,isProactiveActive;
  std::function<bool(const Json&)> isBuiltInBat;
  std::function<std::string()> getCoordinatorSystemPoetry;
};
Json buildEffectiveSystemPoetry(const EffectivePoetryParams& params,
  const EffectivePoetryServices& services);
} // namespace batbox::poetryEntries
