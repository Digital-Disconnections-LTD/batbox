#pragma once
#include "application_model/LocalModelService.hpp"
#include "providers/RequestSettings.hpp"

namespace batbox::providers {
// Copies an endpoint's resolved request limits into a model profile; shared
// by startup profiles and profiles added at runtime.
inline void applyRequestSettings(application_model::ModelProfile& profile,
    const application::InferenceProfile& configured){
  const auto settings=requestSettings(configured);
  if(!profile.client)profile.client.emplace();
  profile.client->requestTimeout=settings.requestTimeout;
  profile.client->streamIdleTimeout=settings.streamIdleTimeout;
  profile.client->connectTimeout=settings.connectTimeout;
  profile.local=settings.local;
  profile.nonStreamingFallback=settings.nonStreamingFallback;
  profile.nonStreamingTimeoutMs=static_cast<double>(settings.nonStreamingTimeout.count());
  profile.maxRetries=settings.maxRetries;
}
} // namespace batbox::providers
