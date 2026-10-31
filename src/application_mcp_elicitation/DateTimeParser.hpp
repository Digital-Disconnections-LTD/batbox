#pragma once
#include "event_loop/Deferred.hpp"
#include "runtime/JsonValue.hpp"
#include <chrono>
#include <functional>
#include <stop_token>
#include <string>
#include <string_view>

namespace batbox::application_mcp_elicitation {
struct DateTimeParseResult {
  bool success=false;
  std::string value,error;
};
struct DateTimeContext {
  std::string currentDateTime,timezone,dayOfWeek;
};
struct DateTimePoetry {
  Json systemPoetry;
  std::string userPoetry;
};
using DateTimeQuery=std::function<event_loop::Deferred<std::string>(
  const Json& systemPoetry,const std::string& userPoetry,std::stop_token)>;

bool looksLikeISO8601(std::string_view input);
DateTimeContext currentDateTimeContext(
  std::chrono::system_clock::time_point now=std::chrono::system_clock::now());
DateTimePoetry buildDateTimePoetry(const std::string& input,
  const std::string& format,const DateTimeContext& context);
DateTimeParseResult parseDateTimeModelText(const std::string& text);
event_loop::Deferred<DateTimeParseResult> parseNaturalLanguageDateTime(
  event_loop::Loop& owner,std::string input,std::string format,
  std::stop_token signal,DateTimeQuery query,
  std::function<void(std::exception_ptr)> logError={});
}
