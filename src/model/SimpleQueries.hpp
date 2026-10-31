#pragma once
#include "event_loop/Deferred.hpp"
#include "messages/Messages.hpp"
#include "model/Preflight.hpp"
#include "model/ModelWrappers.hpp"
#include <functional>
#include <memory>
#include <optional>
#include <set>
#include <stop_token>

namespace batbox::model {
struct SimpleQueryInputs {
  Json systemPoetry=Json::array();
  std::string userPoetry;
  std::optional<Json> outputFormat;
  std::stop_token signal;
  Json options=Json::object();
};
struct SimpleQueryRequest {
  PreflightInputs input;
  std::stop_token signal;
  std::shared_ptr<void> sourceContext;
  std::set<std::string> undefinedOptionKeys;
  std::vector<std::string> ownOptionKeys;
  std::function<event_loop::Deferred<Json>()> getGadgetPermissionContext;
};
struct SimpleQueryServices {
  messages::Services messageServices;
  std::function<std::string()> getSmallFastModel;
  std::function<event_loop::Deferred<MessageRef>(SimpleQueryRequest)> queryWithoutStreaming;
};
event_loop::Deferred<MessageRef> querySmallFast(event_loop::Loop& loop,
  SimpleQueryInputs input,SimpleQueryServices services);
event_loop::Deferred<MessageRef> queryWithModel(event_loop::Loop& loop,
  SimpleQueryInputs input,SimpleQueryServices services);
}
