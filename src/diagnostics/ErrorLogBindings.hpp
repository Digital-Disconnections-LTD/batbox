#pragma once
#include "diagnostics/ErrorLogQueue.hpp"
#include "diagnostics/ErrorLogSink.hpp"

namespace batbox::diagnostics {
inline std::shared_ptr<ErrorLogSink> bindIndigenousErrorLogSink(
    event_loop::Loop& loop,cleanup::Roster& roster,
    std::shared_ptr<DebugLog> debug,ErrorLogQueue& queue,
    std::function<std::string()> errorsDirectory,
    std::function<std::string(const std::string&)> mcpLogsDirectory,
    std::function<std::string()> timestampISO,
    std::function<std::string()> flightId,
    std::function<std::string()> cwd,
    std::function<std::string()> version){
  const auto startupISO=timestampISO();
  auto services=indigenousErrorLogServices(loop,roster,std::move(debug),
    std::move(errorsDirectory),std::move(mcpLogsDirectory),
    std::move(timestampISO),std::move(flightId),std::move(cwd),
    std::move(version),[&queue](ErrorLogCallbacks sink){
      queue.attachErrorLogSink(std::move(sink));
    });
  auto instance=std::make_shared<ErrorLogSink>(std::move(services),startupISO);
  instance->initializeErrorLogSink();
  return instance;
}
}
