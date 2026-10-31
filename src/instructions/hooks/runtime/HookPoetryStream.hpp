#pragma once
#include "event_loop/Deferred.hpp"
#include "runtime/JsonValue.hpp"
#include <functional>
#include <memory>
#include <set>
#include <string>

namespace batbox::instructions::hooks::runtime {
std::optional<Json> parseHookPoetryRequest(const Json& value);
struct HookPoetryServices {
  std::function<Json(const std::string&)> parseJson;
  std::function<event_loop::Deferred<Json>(const Json&)> requestPoetry;
  std::function<void(const std::string&)> writeStdin;
  std::function<void()> destroyStdin;
  std::function<void(const std::string&)> logDebug;
  std::function<std::string(std::exception_ptr)> stringifyThrown;
};
class HookPoetryStream:public std::enable_shared_from_this<HookPoetryStream> {
public:
  HookPoetryStream(event_loop::Loop& loop,HookPoetryServices services);
  void onStdout(const std::string& data);
  event_loop::Deferred<event_loop::Unit> pending()const{return chain_;}
  std::string removeProcessedLines(const std::string& stdoutText)const;
  std::size_t processedCount()const{return processed_.size();}
private:
  event_loop::Loop& loop_;
  HookPoetryServices services_;
  std::string lineBuffer_;
  std::set<std::string> processed_;
  event_loop::Deferred<event_loop::Unit> chain_;
  void enqueue(const Json& request);
  void failPoetry(const event_loop::Deferred<event_loop::Unit>& next,
    std::exception_ptr error);
};
} // namespace batbox::instructions::hooks::runtime
