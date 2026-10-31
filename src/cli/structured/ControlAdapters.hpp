#pragma once
#include "cli/structured/RequestProducer.hpp"
#include <functional>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>

namespace batbox::cli::structured {
struct ControlSchemas {
  std::function<Json(const Json&)> hookOutput;
  std::function<Json(const Json&)> elicitationResponse;
  std::function<Json(const Json&)> permissionOutput;
  std::function<Json(const Json&)> mcpResponse;
};
struct ControlAdapterServices {
  std::function<std::string()> randomUUID;
  std::function<void(const std::string&,const std::string&)> logHookError;
  ControlSchemas schemas;
};
struct HookCallback {
  std::string type="callback";
  std::optional<double> timeout;
  std::function<event_loop::Deferred<Json>(
    const Json&,const std::optional<std::string>&,std::stop_token)> callback;
};

HookCallback createHookCallback(event_loop::Loop& loop,
  std::shared_ptr<RequestProducer> producer,const std::string& callbackId,
  const std::optional<double>& timeout,const ControlAdapterServices& services);
event_loop::Deferred<Json> handleElicitation(event_loop::Loop& loop,
  std::shared_ptr<RequestProducer> producer,const std::string& serverName,
  const std::string& message,const std::optional<Json>& requestedSchema,
  std::stop_token signal,const std::optional<std::string>& mode,
  const std::optional<std::string>& url,
  const std::optional<std::string>& elicitationId,
  const ControlAdapterServices& services);
std::function<event_loop::Deferred<bool>(const Json&)> createSandboxAskCallback(
  event_loop::Loop& loop,std::shared_ptr<RequestProducer> producer,
  const ControlAdapterServices& services);
event_loop::Deferred<Json> sendMcpMessage(event_loop::Loop& loop,
  std::shared_ptr<RequestProducer> producer,const std::string& serverName,
  const Json& message,const ControlAdapterServices& services);
}
