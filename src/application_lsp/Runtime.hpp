#pragma once
#include "lsp/Manager.hpp"
#include "gadgets/Gadget.hpp"
#include <condition_variable>
#include <deque>
#include <future>
#include <memory>
#include <mutex>
#include <thread>

namespace batbox::application_lsp {
// Shared roost owner. File callbacks enqueue in order without blocking
// the terminal owner. Gadget calls wait on the same worker and see all earlier edits.
class Runtime final:public std::enable_shared_from_this<Runtime> {
public:
  static std::shared_ptr<Runtime> create(std::filesystem::path roost,
    Json serverConfig=Json::object());
  ~Runtime();
  Runtime(const Runtime&)=delete;Runtime& operator=(const Runtime&)=delete;
  bool available()const{return manager_.available();}
  bool supportsFile(const std::string& path)const{return manager_.hasServer(path);}
  void changeFile(std::string path,std::string content);
  void saveFile(std::string path);
  void closeFile(std::string path);
  void clearDeliveredDiagnostics(const std::string& uri);
  Json takeDiagnosticSets();
  Json takeDiagnosticAttachments();
  std::shared_future<void> shutdownAsync();
  gadgets::Gadget makeGadget(std::function<std::future<Json>(const Json&,const Json&)> permission={});
  std::future<gadgets::GadgetResult> callGadget(Json input,gadgets::GadgetCallContext context);
  const std::filesystem::path& roost()const{return manager_.roost();}
private:
  Runtime(std::filesystem::path roost,Json serverConfig);
  void enqueue(std::function<void()> job);
  void run();
  gadgets::GadgetResult perform(const Json& input,const gadgets::GadgetCallContext& context);
  std::shared_ptr<lsp::Diagnostics> diagnostics_;
  lsp::Manager manager_;
  mutable std::mutex mutex_;
  std::condition_variable ready_;
  std::deque<std::function<void()>> jobs_;
  std::thread worker_;
  bool closing_=false,done_=false;
  std::shared_future<void> shutdown_;
};
}
