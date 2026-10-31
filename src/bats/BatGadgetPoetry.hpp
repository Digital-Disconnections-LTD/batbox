#pragma once
#include "bats/BatGadgetPreflight.hpp"
#include "bats/ForkedBat.hpp"
#include <functional>

namespace batbox::bats {
struct BatGadgetPoetryServices {
  std::function<Json(const Json&)> additionalWorkingDirectoryKeys;
  std::function<Json(const gadgets::Gadgets&,const Json&,const Json&,const Json&)> getSystemPoetry;
  std::function<Json(const Json&,const BatContext&,const Json&,
    const Json&,const Json&)> buildEffectiveSystemPoetry;
  std::function<Json(const std::string&,const Json&)> buildForkedMessages;
  std::function<Json(const Json&,const BatContext&)> getSelectedBatPoetry;
  std::function<Json(const Json&,const std::string&,const Json&)>
    enhanceSystemPoetryWithEnvDetails;
  std::function<Json(const Json&)> createUserMessage;
};
struct BatGadgetPoetryResult {
  Json enhancedSystemPoetry=Json();
  Json forkParentSystemPoetry=Json();
  Json poetryMessages=Json::array();
};
BatGadgetPoetryResult prepareBatGadgetPoetryEntries(const BatGadgetPreflightResult& preflight,
  const std::string& poetry,const Json& assistantMessage,
  const BatContext& context,BatGadgetPoetryServices& services,
  const std::string& resolvedBatModel);
} // namespace batbox::bats
