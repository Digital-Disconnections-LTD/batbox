#pragma once
#include "diagnostics/ErrorLogSink.hpp"
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace batbox::diagnostics {
struct ErrorQueueServices {
  std::function<bool()> hardFailFeature;
  std::function<std::vector<std::string>()> argv;
  std::function<std::optional<std::string>(const std::string&)> env;
  std::function<bool()> isEssentialTrafficOnly;
  std::function<std::string()> timestampISO;
  std::function<void(const std::string&)> hardFailExit;
};
class ErrorLogQueue {
public:
  explicit ErrorLogQueue(ErrorQueueServices services);
  void attachErrorLogSink(ErrorLogCallbacks sink);
  void logError(const ErrorValue& error);
  void logError(std::shared_ptr<ErrorValue> error);
  void logMCPError(const std::string& server,const ErrorValue& error);
  void logMCPError(const std::string& server,std::shared_ptr<ErrorValue> error);
  void logMCPUnknownError(const std::string& server,const Json& error);
  void logMCPUnknownError(const std::string& server,std::shared_ptr<Json> error);
  void logMCPDebug(const std::string& server,const std::string& message);
  Json getInMemoryErrors()const;
  void resetForTesting();
  std::size_t queuedCount()const;
private:
  struct State;
  std::shared_ptr<State> state_;
};
}
