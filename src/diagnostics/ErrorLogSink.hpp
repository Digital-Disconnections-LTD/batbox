#pragma once
#include "diagnostics/BufferedWriter.hpp"
#include "diagnostics/DebugLog.hpp"
#include "runtime/JsonValue.hpp"
#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace batbox::cleanup { class Roster; }
namespace batbox::diagnostics {
struct ErrorValue {
  std::string name="Error",message;
  std::optional<std::string> stack;
  bool isHttpClientError=false;
  std::optional<std::string> requestUrl;
  std::optional<int> responseStatus;
  Json responseData=nullptr;
};
struct ErrorLogCallbacks {
  std::function<void(const ErrorValue&)> logError;
  std::function<void(const std::string&,const ErrorValue&)> logMCPError;
  std::function<void(const std::string&,const Json&)> logMCPUnknownError;
  std::function<void(const std::string&,const std::string&)> logMCPDebug;
  std::function<std::string()> getErrorsPath;
  std::function<std::string(const std::string&)> getMCPLogsPath;
};
struct ErrorLogServices {
  event_loop::Loop* loop=nullptr;
  std::function<std::string()> errorsDirectory;
  std::function<std::string(const std::string&)> mcpLogsDirectory;
  std::function<std::string()> timestampISO;
  std::function<std::string()> flightId;
  std::function<std::string()> cwd;
  std::function<std::string()> version;
  std::function<bool()> isInternalUser;
  std::function<std::string()> internalUserTypeLabel;
  std::function<void(const std::string&,const std::string&)> logForDebugging;
  std::function<void(const std::string&,const std::string&)> appendFileSync;
  std::function<void(const std::string&)> mkdirSync;
  std::function<void(std::function<void()>)> registerCleanup;
  std::function<void(ErrorLogCallbacks)> attachErrorLogSink;
  std::function<BufferedWriterServices(
    std::function<void(const std::string&)>)> makeBufferedWriterServices;
};

std::string dateToLogFilename(const std::string& isoDate);
std::optional<std::string> extractServerMessage(const Json& data);

class ErrorLogSink {
public:
  ErrorLogSink(ErrorLogServices services,std::string startupISO);
  std::string getErrorsPath()const;
  std::string getMCPLogsPath(const std::string& serverName)const;
  void logError(const ErrorValue& error);
  void logMCPError(const std::string& serverName,const ErrorValue& error);
  void logMCPUnknownError(const std::string& serverName,const Json& error);
  void logMCPDebug(const std::string& serverName,const std::string& message);
  void flushLogWritersForTesting();
  void clearLogWritersForTesting();
  void initializeErrorLogSink();
private:
  struct State;
  std::shared_ptr<State> state_;
};
ErrorLogServices indigenousErrorLogServices(event_loop::Loop& loop,
  cleanup::Roster& roster,std::shared_ptr<DebugLog> debug,
  std::function<std::string()> errorsDirectory,
  std::function<std::string(const std::string&)> mcpLogsDirectory,
  std::function<std::string()> timestampISO,
  std::function<std::string()> flightId,
  std::function<std::string()> cwd,
  std::function<std::string()> version,
  std::function<void(ErrorLogCallbacks)> attachSink);
}
