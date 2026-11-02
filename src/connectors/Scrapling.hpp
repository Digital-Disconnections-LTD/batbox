#pragma once
#include "runtime/JsonValue.hpp"
#include "connectors/Abort.hpp"
#include <chrono>
#include <atomic>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>
#ifndef _WIN32
#include <sys/types.h>
#endif
namespace batbox::process {class IndigenousJsonlProcess;}
namespace batbox {
// Indigenous JSONL MCP transport to the external Scrapling server. The process
// starts on the first request, so a missing helper never affects startup, and
// restarts on a later request if it exited. Keeping it alive preserves its
// browser/flight IDs between requests. Durable authentication uses the
// server's browser_save_state/browser_load_state gadgets.
class Scrapling {
#ifdef _WIN32
  std::unique_ptr<process::IndigenousJsonlProcess> indigenousProcess;
#else
  pid_t child = -1;
#endif
  int input = -1, output = -1, diagnosticOutput = -1;
  std::thread diagnosticReader;
  std::atomic<bool> stopDiagnostic{false};
  mutable std::mutex diagnosticMutex;
  std::string diagnosticTail;
  bool diagnosticTruncated = false;
  long nextId = 0;
  std::string buffered;
  std::chrono::milliseconds timeout;
  std::vector<std::string> command;
  std::atomic<bool> stopped{false},started{false};
  std::timed_mutex mutex;
  void start(std::stop_token signal);
  void ensureRunning(std::stop_token signal);
  void send(const Json& message, std::chrono::steady_clock::time_point deadline,std::stop_token signal={});
  Json receive(std::chrono::steady_clock::time_point deadline,std::stop_token signal={});
  Json request(const std::string& method, const Json& params,std::stop_token signal={},
    std::optional<std::chrono::milliseconds> limit={});
  std::unique_lock<std::timed_mutex> acquire(std::stop_token signal);
  void readDiagnostics();
  void close() noexcept;
public:
  explicit Scrapling(const std::vector<std::string>& command,
    std::chrono::milliseconds timeout = std::chrono::hours(2));
  ~Scrapling();
  // After shutdown no request starts the helper again.
  void shutdown()noexcept{stopped=true;close();}
  // True while a helper process is running.
  bool running()const noexcept{return started;}
  std::string stderrDiagnostic()const;
  Scrapling(const Scrapling&) = delete;
  Scrapling& operator=(const Scrapling&) = delete;
  Json listGadgets(std::stop_token signal={});
  Json callGadget(const std::string& name, const Json& arguments,std::stop_token signal={});
  Json fetch(const std::string& url,std::stop_token signal={});
};
} // namespace batbox
