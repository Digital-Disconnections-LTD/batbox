#include "model/Lifecycle.hpp"
#include "services/api/batbox.hpp"
#include <iostream>

using batbox::Json;
using namespace batbox::model;

int main() {
  std::string line;
  while (std::getline(std::cin, line)) {
    try {
      const Json input = Json::parse(line);
      const std::string op = input.value("op", "");
      Json result;
      if (op == "watchdog") {
        auto settings = watchdogSettings(input.value("env", Json::object()));
        result = {{"enabled", settings.enabled}, {"timeoutMs", settings.timeoutMs}, {"warningMs", settings.warningMs}};
      } else if (op == "route") {
        StreamingFailureInput failure;
        failure.isAPIUserAbortError = input.value("isAbort", false);
        failure.signalAborted = input.value("signalAborted", false);
        failure.disableFallbackEnvironment = input.value("disableFallbackEnvironment", false);
        failure.disableFallbackFeature = input.value("disableFallbackFeature", false);
        bool didFallback = false;
        bool callbackSaw = false;
        auto action = routeStreamingFailure(failure, didFallback, [&] { callbackSaw = didFallback; });
        if (action == StreamingFailureAction::NonStreamingFallback) {
          result = {{"action", "fallback"}, {"didFallBackToNonStreaming", didFallback}, {"callbackSaw", callbackSaw}};
        } else if (action == StreamingFailureAction::Timeout) {
          result = {{"action", "timeout"}, {"didFallBackToNonStreaming", didFallback},
                    {"errorName", "APIConnectionTimeoutError"}, {"errorMessage", "Request timed out"}};
        } else {
          result = {{"action", "propagate"}, {"didFallBackToNonStreaming", didFallback},
                    {"errorName", failure.isAPIUserAbortError ? "APIUserAbortError" : "Error"},
                    {"errorMessage", failure.isAPIUserAbortError ? "aborted" : "stream failed"}};
        }
      } else if (op == "creation404") {
        result = is404StreamCreationError(input.value("didFallback", false), input.value("cannotRetry", false),
                                          input.value("apiError", false), input.value("status", 0));
      } else if (op == "creation404Route") {
        bool didFallback=input.value("didFallback",false);
        bool callbackSaw=false;
        const bool eligible=route404StreamCreationError(didFallback,input.value("cannotRetry",false),
          input.value("apiError",false),input.value("status",0),[&]{callbackSaw=didFallback;});
        result={{"eligible",eligible},{"didFallBackToNonStreaming",didFallback}};
        if(eligible){
          result["callbackSaw"]=callbackSaw;
          result["failedRequestId"]=failedStreamRequestId(
            input.contains("requestId") && !input.at("requestId").is_null()
              ? std::optional<std::string>(input.at("requestId").get<std::string>()) : std::nullopt);
        }
      } else if (op == "cleanup") {
        StreamResources resources;
        Json calls = Json::array();
        if (input.value("stream", false)) {
          resources.setStream(input.value("aborted", false), [&] {
            calls.push_back("abort");
            if (input.value("abortThrows", false)) throw std::runtime_error("abort failed");
          });
        }
        if (input.value("response", false)) resources.setResponse([&] { calls.push_back("cancel"); });
        resources.release();
        resources.release();
        result = {{"calls", calls}, {"hasStream", resources.hasStream()}, {"hasResponse", resources.hasResponse()}};
      } else if (op == "fallbackAccounting") {
        if (input.contains("fallbackMessage") && !input.at("fallbackMessage").is_null()) {
          auto accounted = accountFallbackMessage(input.at("fallbackMessage"), input.value("priorCostUSD", 0.0),
            input.value("resolvedModel", ""), input.value("model", ""),
            [](const std::string&, const Json& usage) { return usage.at("output_tokens").get<double>() / 1000; },
            [](double cost, const Json&, const std::string&) { return cost + .5; });
          result = {{"usage", accounted.usage}, {"stopReason", accounted.stopReason}, {"costUSD", accounted.costUSD}};
        } else {
          result = {{"usage", batbox::EMPTY_USAGE}, {"stopReason", nullptr}, {"costUSD", input.value("priorCostUSD", 0.0)}};
        }
      } else if (op == "fallbackMessage") {
        result = makeFallbackAssistantMessage(input.at("result"), [](const Json& content) { return content; },
          input.contains("requestId") && !input.at("requestId").is_null() ? std::optional<std::string>(input.at("requestId").get<std::string>()) : std::nullopt,
          input.contains("advisorModel") && !input.at("advisorModel").is_null() ? std::optional<std::string>(input.at("advisorModel").get<std::string>()) : std::nullopt,
          [] { return "uuid-1"; }, [] { return "2026-10-28T12:00:00.000Z"; });
      }
      std::cout << result.dump() << '\n';
    } catch (const std::exception& error) {
      std::cout << Json{{"fatal", error.what()}}.dump() << '\n';
    }
  }
}
