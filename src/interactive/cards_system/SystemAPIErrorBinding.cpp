#include "interactive/cards_system/SystemAPIErrorCard.hpp"
#include "api_errors/ApiErrors.hpp"
#include "retry/Retry.hpp"
#include <stdexcept>

namespace batbox::interactive::cards_system {
namespace {
double number(const Json& object, const char* key) {
  const auto found = object.find(key);
  if (found == object.end() || !found->is_number())
    throw std::invalid_argument(std::string("API error message requires ") + key);
  return found->get<double>();
}
}

APIErrorInput apiErrorInputFromMessage(
    const Json& message, bool verbose,
    std::optional<std::string> apiTimeoutEnvironment) {
  const auto found = message.find("error");
  if (found == message.end() || !found->is_object())
    throw std::invalid_argument("API error message requires error object");
  return {number(message, "retryAttempt"), number(message, "retryInMs"),
          number(message, "maxRetries"),
          api_errors::formatAPIError(retry::errorFromJson(*found)),
          verbose, std::move(apiTimeoutEnvironment)};
}

} // namespace batbox::interactive::cards_system
