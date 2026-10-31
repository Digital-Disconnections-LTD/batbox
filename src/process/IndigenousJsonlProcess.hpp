#pragma once
#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <stop_token>
#include <stdexcept>
#include <string>
#include <vector>

namespace batbox::process {
struct IndigenousChildSpawnError : std::runtime_error {
  int uvCode;
  IndigenousChildSpawnError(int code,const std::string& message):
    std::runtime_error(message),uvCode(code){}
};
struct IndigenousChildOptions {
  bool lineMode=true;
  bool inheritEnvironment=true;
  std::optional<std::string> cwd;
  // Called on the child I/O thread in read order. true denotes stdout.
  std::function<void(const std::string&,bool)> onData;
};
// Persistent child transport for line-delimited JSON RPC. The child and its
// libuv handles live on a dedicated thread; callers use bounded synchronous
// reads/writes without sharing that event loop with the application UI.
class IndigenousJsonlProcess final {
public:
  IndigenousJsonlProcess(const std::vector<std::string>& command,
    const std::map<std::string,std::string>& environmentOverrides={},
    IndigenousChildOptions options={});
  ~IndigenousJsonlProcess();
  IndigenousJsonlProcess(const IndigenousJsonlProcess&)=delete;
  IndigenousJsonlProcess& operator=(const IndigenousJsonlProcess&)=delete;
  void writeLine(const std::string& bytes,
    std::chrono::steady_clock::time_point deadline,std::stop_token signal={});
  std::string readLine(std::chrono::steady_clock::time_point deadline,
    std::stop_token signal={});
  std::string stderrTail()const;
  bool stderrTruncated()const;
  void closeStdin();
  void kill()noexcept;
  int waitForExit(std::chrono::steady_clock::time_point deadline,
    std::stop_token signal={});
  long pid()const;
  void close()noexcept;
private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
} // namespace batbox::process
