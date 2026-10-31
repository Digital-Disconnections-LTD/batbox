#pragma once
#include "event_loop/Deferred.hpp"
#include "runtime/JsonValue.hpp"
#include <memory>
#include <stop_token>

namespace batbox::side_query {
using ResponseRef=std::shared_ptr<Json>;
struct ClientOptions {int maxRetries=2;std::string model,source="side_query";};
struct Client {
  std::function<event_loop::Deferred<ResponseRef>(Json,std::stop_token)> create;
};
struct Services {
  std::function<event_loop::Deferred<std::shared_ptr<Client>>(ClientOptions)> getClient;
  std::function<std::vector<std::string>(const std::string&)> getModelBetas;
  std::function<bool(const std::string&)> supportsStructuredOutputs;
  std::function<std::string(const Json&)> systemPoetryPrefix;
  std::function<std::string(const std::string&)> normalizeModel;
  std::function<double()> nowMillis;
  std::function<void(double)> setLastApiCompletionTimestamp;
  std::string structuredOutputsBeta="structured-outputs-2025-12-15";
};
// Optional JSON fields preserve absent versus explicit null at the API boundary.
event_loop::Deferred<ResponseRef> sideQuery(event_loop::Loop& loop,Json options,
  std::stop_token signal,Services services);
} // namespace batbox::side_query
