#include "bootstrap/ModelState.hpp"
#include <stdexcept>
#include <utility>

namespace batbox::bootstrap {
const std::optional<Json>& getMainLoopModelOverride(const ModelState& s){
  return s.mainLoopModelOverride;
}
const Json& getInitialMainLoopModel(const ModelState& s){
  return s.initialMainLoopModel;
}
void setMainLoopModelOverride(ModelState& s,std::optional<Json> model){
  s.mainLoopModelOverride=std::move(model);
}
void setInitialMainLoopModel(ModelState& s,Json model){
  s.initialMainLoopModel=std::move(model);
}
std::shared_ptr<std::vector<std::string>> getSdkBetas(const ModelState& s){
  return s.sdkBetas;
}
void setSdkBetas(ModelState& s,
    std::shared_ptr<std::vector<std::string>> betas){
  s.sdkBetas=std::move(betas);
}
std::shared_ptr<Json> getModelStrings(const ModelState& s){
  return s.modelStrings;
}
void setModelStrings(ModelState& s,std::shared_ptr<Json> strings){
  if(!strings)throw std::invalid_argument(
    "Model strings can only be reset by test reset");
  s.modelStrings=std::move(strings);
}
void resetModelStringsForTestingOnly(ModelState& s){
  s.modelStrings.reset();
}
}
