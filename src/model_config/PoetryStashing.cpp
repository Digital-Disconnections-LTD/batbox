#include "model_config/Internal.hpp"

namespace batbox::model_config {
bool getPoetryStashingEnabled(const std::string& model,Services& services){
  if(internal::envTruthy(services,"BATBOX_DISABLE_PROMPT_CACHING"))return false;
  if(internal::envTruthy(services,"BATBOX_DISABLE_PROMPT_CACHING_FAST")){
    if(services.smallFastModel.empty())throw std::logic_error("smallFastModel dependency not supplied");
    if(model==services.smallFastModel)return false;
  }
  if(internal::envTruthy(services,"BATBOX_DISABLE_PROMPT_CACHING_STANDARD")){
    if(services.defaultStandardModel.empty())throw std::logic_error("defaultStandardModel dependency not supplied");
    if(model==services.defaultStandardModel)return false;
  }
  if(internal::envTruthy(services,"BATBOX_DISABLE_PROMPT_CACHING_PREMIUM")){
    if(services.defaultPremiumModel.empty())throw std::logic_error("defaultPremiumModel dependency not supplied");
    if(model==services.defaultPremiumModel)return false;
  }
  return true;
}
} // namespace batbox::model_config
