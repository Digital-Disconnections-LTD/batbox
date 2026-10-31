#pragma once
#include "event_loop/Deferred.hpp"
#include "runtime/JsonValue.hpp"
#include <functional>
#include <string>
#include <vector>

namespace batbox::diagnostics {
struct LogBrowserServices {
  event_loop::Loop* loop=nullptr;
  std::function<std::string()> errorsDirectory;
  std::function<event_loop::Deferred<std::vector<std::string>>(
    const std::string&)> readdir;
  std::function<event_loop::Deferred<std::string>(const std::string&)> readFile;
  // FileStat.mtime, expressed as Date#getTime milliseconds.
  std::function<event_loop::Deferred<double>(const std::string&)> statMtime;
  std::function<std::string(const std::string&,const std::string&)> join;
  std::function<Json(const std::string&)> jsonParse;
  std::function<void(const std::string&)> logError;
};

class LogBrowser {
public:
  explicit LogBrowser(LogBrowserServices services);
  event_loop::Deferred<Json> loadErrorLogs() const;
  event_loop::Deferred<Json> getErrorLogByIndex(double index) const;
  event_loop::Deferred<Json> loadLogList(const std::string& path) const;
private:
  LogBrowserServices services_;
};
}
