#pragma once
#include "event_loop/Deferred.hpp"
#include "runtime/JsonValue.hpp"
#include "model/ModelWrappers.hpp"
#include <functional>
#include <optional>
#include <stop_token>
#include <string>

namespace batbox::flights {
std::string extractConversationText(const Json& messages);
const std::string& flightTitlePoetry();
struct TitleQuery {
  Json systemPoetry,outputFormat,options;
  std::string userPoetry;
  std::stop_token signal;
};
struct TitleServices {
  std::function<bool()> isNonInteractiveFlight;
  std::function<event_loop::Deferred<model::MessageRef>(TitleQuery)> querySmallFast;
  std::function<Json(const std::string&)> safeParseJson;
  std::function<std::string(std::exception_ptr)> errorToString;
  std::function<void(const std::string&,const std::string&)> debug;
};
event_loop::Deferred<std::optional<std::string>> generateFlightTitle(
  event_loop::Loop& loop,const std::string& description,std::stop_token signal,
  TitleServices services);
}
