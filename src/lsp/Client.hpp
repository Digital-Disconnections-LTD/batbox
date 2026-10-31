#pragma once
#include "lsp/Protocol.hpp"
#include "process/IndigenousJsonlProcess.hpp"
#include <chrono>
#include <condition_variable>
#include <deque>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <stop_token>
#include <thread>

namespace batbox::lsp {
class Client final {
public:
  using Notification=std::function<void(const std::string&,const Json&)>;
  Client(std::string name,std::vector<std::string> command,
    std::map<std::string,std::string> env,std::string cwd,
    Notification notification);
  ~Client();
  Client(const Client&)=delete;Client& operator=(const Client&)=delete;
  Json request(const std::string& method,const Json& params,
    std::chrono::milliseconds timeout=std::chrono::seconds(15),std::stop_token signal={});
  void notify(const std::string& method,const Json& params);
  void markInitialized(){initialized_=true;}
  void stop()noexcept;
  long pid()const;
private:
  struct Pending {std::promise<Json> promise;};
  void receive(Json message);
  void dispatch();
  void write(const Json& message);
  std::string name_;
  Notification notification_;
  std::mutex parserMutex_,queueMutex_,writeMutex_,pendingMutex_;
  std::condition_variable queueReady_;
  FrameDecoder decoder_;
  std::deque<Json> queue_;
  std::map<int,std::shared_ptr<Pending>> pending_;
  std::thread dispatcher_;
  std::unique_ptr<process::IndigenousJsonlProcess> process_;
  std::exception_ptr parserFailure_;
  int nextId_=1;
  bool closing_=false,initialized_=false;
};
}
