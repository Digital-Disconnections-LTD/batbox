#pragma once
#include "event_loop/Deferred.hpp"
#include "runtime/JsonValue.hpp"
#include <exception>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace batbox::paste_store {
struct Services {
  event_loop::Loop* loop=nullptr;
  std::function<std::string()> getConfigHomeDir;
  std::function<std::string(const std::string&,const std::string&)> join;
  std::function<event_loop::Deferred<event_loop::Unit>(
    const std::string&,const Json&)> mkdir;
  std::function<event_loop::Deferred<event_loop::Unit>(
    const std::string&,const std::string&,const Json&)> writeFile;
  std::function<event_loop::Deferred<std::string>(
    const std::string&,const Json&)> readFile;
  std::function<event_loop::Deferred<std::vector<std::string>>(
    const std::string&)> readdir;
  std::function<event_loop::Deferred<Json>(const std::string&)> stat;
  std::function<event_loop::Deferred<event_loop::Unit>(const std::string&)> unlink;
  std::function<void(const std::string&)> logForDebugging;
  std::function<bool(const std::exception_ptr&)> isENOENT;
  std::function<std::string(const std::exception_ptr&)> errorToString;
};
std::string getPasteStoreDir(const Services& services);
std::string getPastePath(const Services& services,const std::string& hash);
std::string hashPastedText(const std::string& content);
event_loop::Deferred<event_loop::Unit> storePastedText(
  const std::string& hash,const std::string& content,const Services& services);
event_loop::Deferred<std::optional<std::string>> retrievePastedText(
  const std::string& hash,const Services& services);
event_loop::Deferred<event_loop::Unit> cleanupOldPastes(
  const std::function<double()>& cutoffDateGetTime,const Services& services);
event_loop::Deferred<event_loop::Unit> cleanupOldPastes(
  double cutoffTimeMs,const Services& services);
}
