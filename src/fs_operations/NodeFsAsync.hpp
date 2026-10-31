#pragma once
#include "event_loop/Deferred.hpp"
#include "runtime/JsonValue.hpp"
#include <exception>
#include <functional>
#include <optional>
#include <string>

namespace batbox::fs_operations {
struct NodeFsAsyncImports {
  event_loop::Loop* loop=nullptr;
  std::function<event_loop::Deferred<Json>(const std::string&)> stat;
  std::function<event_loop::Deferred<Json>(const std::string&,bool)> readdir;
  std::function<event_loop::Deferred<event_loop::Unit>(const std::string&)> unlink,rmdir;
  std::function<event_loop::Deferred<event_loop::Unit>(
    const std::string&,const std::optional<Json>&)> rm;
  std::function<event_loop::Deferred<event_loop::Unit>(
    const std::string&,const Json&)> mkdir;
  std::function<event_loop::Deferred<std::string>(
    const std::string&,const std::string&)> readFile;
  std::function<event_loop::Deferred<event_loop::Unit>(
    const std::string&,const std::string&)> rename;
  std::function<std::optional<std::string>(const std::exception_ptr&)> errnoCode;
};

class NodeFsAsync {
public:
  explicit NodeFsAsync(NodeFsAsyncImports imports);
  event_loop::Deferred<Json> stat(const std::string& path)const;
  event_loop::Deferred<Json> readdir(const std::string& path)const;
  event_loop::Deferred<event_loop::Unit> unlink(const std::string& path)const;
  event_loop::Deferred<event_loop::Unit> rmdir(const std::string& path)const;
  event_loop::Deferred<event_loop::Unit> rm(const std::string& path,
    std::optional<Json> options={})const;
  event_loop::Deferred<event_loop::Unit> mkdir(const std::string& path,
    std::optional<Json> options={})const;
  event_loop::Deferred<std::string> readFile(const std::string& path,
    const std::string& encoding)const;
  event_loop::Deferred<event_loop::Unit> rename(const std::string& source,
    const std::string& destination)const;
private:
  NodeFsAsyncImports imports_;
};
}
