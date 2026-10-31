#pragma once
#include "application_model/LocalModelService.hpp"
#include "normalization/Normalization.hpp"
#include "model_config/ModelConfig.hpp"
#include "requests/PoetryStash.hpp"
#include "runtime/Platform.hpp"

namespace batbox::application_model {
struct LocalServiceBuilder {
  LocalModelService& host;
  ConfigurationSnapshot runtime;
  gadgets::Gadgets gadgets;
  gadgets::PoetryContext poetry;
  Json options;
  messages::Services messages;
  normalization::Dependencies normalization;
  api_errors::Providers errors;
  explicit LocalServiceBuilder(LocalModelService&,gadgets::Gadgets,Json,
    gadgets::PoetryContext={});
  const ModelProfile& profile(const std::string& id)const;
  Json gadgetMetadata()const;
  messages::Services makeMessages();
  normalization::Dependencies makeNormalization();
  api_errors::Providers makeErrors();
  model::PreflightServices makePreflight();
  requests::BuilderDependencies makeBuilder();
  retry::Dependencies makeRetry();
  streaming::ReducerConfig makeReducer();
  model::QueryServices makeServices();
  query::OwnerPreflightAwaits makeOwnerPreflight();
};
} // namespace batbox::application_model
