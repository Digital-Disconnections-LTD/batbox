#pragma once
#include "mcp/Mcp.hpp"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <exception>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#ifndef _WIN32
#include <sys/types.h>
#endif

namespace batbox::process {class IndigenousJsonlProcess;}

namespace batbox::mcp {
struct ProtocolError : std::runtime_error {
  int code;
  Json data;
  ProtocolError(int rpcCode,const std::string& message,Json rpcData=Json())
    :std::runtime_error(message),code(rpcCode),data(std::move(rpcData)){}
};
class StdioClient:public std::enable_shared_from_this<StdioClient> {
 public:
  StdioClient(const std::vector<std::string>& command,const std::string& serverName,
    std::chrono::milliseconds timeout=std::chrono::milliseconds(100000000),
    const std::map<std::string,std::string>& environmentOverrides={},
    std::optional<std::chrono::milliseconds> initializationTimeout={},
    std::optional<std::chrono::milliseconds> ordinaryRequestTimeout={},
    ClientHandlers handlers={},std::stop_token initializeSignal={});
  ~StdioClient();
  void shutdown()noexcept{close();}
  StdioClient(const StdioClient&)=delete;
  StdioClient& operator=(const StdioClient&)=delete;
  Json listGadgets(std::stop_token signal={});
  Json invoke(const std::string& method,const Json& params=Json::object(),std::stop_token signal={});
  GadgetCallResult callGadget(const std::string& name,const Json& arguments,
    std::stop_token signal={},const std::function<void(const Json&)>& onProgress={});
  bool hasGadgetCapability() const{return gadgetsCapability_;}
  const std::string& instructions() const{return instructions_;}
  const Json& capabilities()const{return capabilities_;}
  const Json& serverInfo()const{return serverInfo_;}
  // A bounded, sanitized tail of server stderr for local diagnostics.
  std::string stderrDiagnostic() const;
  void ensureConnected() const;
 private:
  struct Pending {
    std::mutex mutex;
    std::condition_variable done;
    bool complete=false;
    Json result;
    std::exception_ptr error;
    std::function<void(const Json&)> onProgress;
    std::string gadget;
  };
#ifdef _WIN32
  std::unique_ptr<process::IndigenousJsonlProcess> indigenousProcess_;
#else
  pid_t child_=-1;
#endif
  int input_=-1,output_=-1,stderr_=-1;
  std::string serverName_;
  std::chrono::milliseconds timeout_;
  std::optional<std::chrono::milliseconds> initializationTimeout_,ordinaryRequestTimeout_;
  std::atomic<long> nextId_{0};
  std::atomic<bool> connected_{false};
  bool gadgetsCapability_=false;
  std::string instructions_;
  Json capabilities_=Json::object(),serverInfo_=Json::object();
  ClientHandlers handlers_;
  std::stop_source requestLifetime_;
  mutable std::mutex stderrMutex_;
  std::string stderrTail_;
  bool stderrTruncated_=false;
  std::atomic<bool> stderrStop_{false};
  std::vector<std::string> sensitiveEnvironmentValues_;
  std::mutex writeMutex_,pendingMutex_;
  std::map<long,std::shared_ptr<Pending>> pending_;
  std::thread reader_,stderrReader_;
  void send(const Json& message,std::chrono::steady_clock::time_point deadline,
    std::stop_token signal={});
  Json request(const std::string& method,const Json& params,std::stop_token signal,
    const std::function<void(const Json&)>& onProgress={},const std::string& gadget="");
  void readLoop();
  void stderrLoop();
  void handleMessage(const Json& message);
  void failAll(std::exception_ptr error);
  void discardPending(long id);
  void close() noexcept;
};
std::shared_ptr<Connection> makeStdioConnection(const std::shared_ptr<StdioClient>& client,
  const std::string& serverName,bool skipPrefix=false);
} // namespace batbox::mcp
