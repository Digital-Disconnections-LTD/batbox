#pragma once
#include "diagnostics/LogBrowser.hpp"
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>

namespace batbox::diagnostics {
class IndigenousLogBrowserIO {
public:
  explicit IndigenousLogBrowserIO(event_loop::Loop& loop,unsigned workers=4);
  ~IndigenousLogBrowserIO();
  IndigenousLogBrowserIO(const IndigenousLogBrowserIO&)=delete;
  IndigenousLogBrowserIO& operator=(const IndigenousLogBrowserIO&)=delete;
  event_loop::Deferred<std::vector<std::string>> readdir(const std::string& path);
  event_loop::Deferred<std::string> readFile(const std::string& path);
  event_loop::Deferred<double> statMtime(const std::string& path);
private:
  void submit(std::function<void()> work);
  void work();
  event_loop::Loop& loop_;
  std::mutex mutex_;
  std::condition_variable ready_;
  std::deque<std::function<void()>> jobs_;
  std::vector<std::thread> workers_;
  bool stopping_=false;
};

LogBrowserServices indigenousLogBrowserServices(event_loop::Loop& loop,
  std::shared_ptr<IndigenousLogBrowserIO> io,
  std::function<std::string()> errorsDirectory,
  std::function<std::string(const std::string&,const std::string&)> join,
  std::function<Json(const std::string&)> jsonParse,
  std::function<void(const std::string&)> logError);
}
