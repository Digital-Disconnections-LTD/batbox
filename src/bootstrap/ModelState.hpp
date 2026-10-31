#pragma once
#include "runtime/JsonValue.hpp"
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace batbox::bootstrap {
struct ModelState {
  std::optional<Json> mainLoopModelOverride;
  Json initialMainLoopModel=nullptr;
  std::shared_ptr<Json> modelStrings;
  std::shared_ptr<std::vector<std::string>> sdkBetas;
};
const std::optional<Json>& getMainLoopModelOverride(
  const ModelState& state);
const Json& getInitialMainLoopModel(const ModelState& state);
void setMainLoopModelOverride(ModelState& state,std::optional<Json> model);
void setInitialMainLoopModel(ModelState& state,Json model);
std::shared_ptr<std::vector<std::string>> getSdkBetas(
  const ModelState& state);
void setSdkBetas(ModelState& state,
  std::shared_ptr<std::vector<std::string>> betas);
std::shared_ptr<Json> getModelStrings(const ModelState& state);
void setModelStrings(ModelState& state,std::shared_ptr<Json> strings);
void resetModelStringsForTestingOnly(ModelState& state);
}
