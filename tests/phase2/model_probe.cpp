#include "model/ModelWrappers.hpp"
#include "requests/assembly.hpp"
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
      if (op == "without" || op == "with") {
        const Json stream = input.value("stream", Json::array());
        int consumed = 0;
        StreamFactory factory = [&] {
          return [stream, &input, &consumed, index = std::size_t{0}]() mutable -> std::optional<MessageRef> {
            if (input.contains("throwAt") && input.at("throwAt") == consumed)
              throw std::runtime_error("Injected stream failure");
            if (index >= stream.size()) return std::nullopt;
            consumed++;
            return std::make_shared<Json>(stream.at(index++));
          };
        };
        try {
          if (op == "without") {
            auto message = queryModelWithoutStreaming(factory, [&] { return input.value("aborted", false); });
            result = {{"result", *message}, {"consumed", consumed}};
          } else {
            Json messages = Json::array();
            auto next = queryModelWithStreaming(factory);
            while (auto message = next()) messages.push_back(**message);
            result = {{"result", messages}, {"consumed", consumed}};
          }
        } catch (const UserAbortError& error) {
          result = {{"error", {{"name", "APIUserAbortError"}, {"message", error.what()}}}, {"consumed", consumed}};
        } catch (const std::exception& error) {
          result = {{"error", {{"name", "Error"}, {"message", error.what()}}}, {"consumed", consumed}};
        }
      } else if (op == "timeout") {
        result = {{"result", getNonstreamingFallbackTimeoutMs(input.value("env", Json::object()))}};
      } else if (op == "adjust") {
        result = {{"result", batbox::requests::adjustParamsForNonStreaming(input.at("params"), input.at("cap").get<double>())}};
      } else if (op == "previous") {
        auto previous = getPreviousRequestIdFromMessages(input.at("messages"));
        result = {{"result", previous ? Json(*previous) : Json(nullptr)}};
      }
      std::cout << result.dump() << '\n';
    } catch (const std::exception& error) {
      std::cout << Json{{"fatal", error.what()}}.dump() << '\n';
    }
  }
}
