#include "diagnostics/ErrorLogQueue.hpp"
#include "runtime/JsonValue.hpp"
#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <utility>

namespace batbox::diagnostics {
namespace {
bool envTruthy(const std::optional<std::string>& raw){
  if(!raw)return false;
  auto value=value::trim(*raw);
  for(auto& c:value)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return value=="1"||value=="true"||value=="yes"||value=="on";
}
std::string errorText(const ErrorValue& error){
  return error.stack&&!error.stack->empty()?*error.stack:error.message;
}
}
struct ErrorLogQueue::State {
  explicit State(ErrorQueueServices value):services(std::move(value)){}
  struct Event {
    enum class Kind { Error, MCPError, MCPUnknownError, MCPDebug } kind;
    std::shared_ptr<ErrorValue> error;
    std::shared_ptr<Json> unknown;
    std::string server,message;
  };
  ErrorQueueServices services;
  std::optional<bool> hardFailMode;
  Json inMemory=Json::array();
  std::vector<Event> queue;
  std::optional<ErrorLogCallbacks> sink;
  bool isHardFailMode(){
    if(!hardFailMode){
      const auto args=services.argv();
      hardFailMode=std::find(args.begin(),args.end(),"--hard-fail")!=args.end();
    }
    return *hardFailMode;
  }
  void attach(ErrorLogCallbacks value){
    if(sink)return;
    sink=std::move(value);
    if(queue.empty())return;
    auto pending=std::move(queue);
    queue.clear();
    for(const auto& event:pending){
      switch(event.kind){
        case Event::Kind::Error:sink->logError(*event.error);break;
        case Event::Kind::MCPError:sink->logMCPError(event.server,*event.error);break;
        case Event::Kind::MCPUnknownError:
          sink->logMCPUnknownError(event.server,*event.unknown);break;
        case Event::Kind::MCPDebug:sink->logMCPDebug(event.server,event.message);break;
      }
    }
  }
  void log(std::shared_ptr<ErrorValue> error){
    if(services.hardFailFeature()&&isHardFailMode()){
      services.hardFailExit(errorText(*error));
      throw std::runtime_error("Hard-fail exit returned");
    }
    try{
      if(envTruthy(services.env("BATBOX_USE_BEDROCK"))||
          envTruthy(services.env("BATBOX_USE_VERTEX"))||
          envTruthy(services.env("BATBOX_USE_FOUNDRY"))||
          [&]{const auto disabled=services.env("DISABLE_ERROR_REPORTING");
            return disabled&&!disabled->empty();}()||
          services.isEssentialTrafficOnly())return;
      if(inMemory.size()>=100)inMemory.erase(inMemory.begin());
      inMemory.push_back({{"error",errorText(*error)},
        {"timestamp",services.timestampISO()}});
      if(!sink){queue.push_back({Event::Kind::Error,std::move(error),{}, {},{}});return;}
      sink->logError(*error);
    }catch(...){}
  }
  void mcpError(std::string server,std::shared_ptr<ErrorValue> error){
    try{
      if(!sink){queue.push_back({Event::Kind::MCPError,std::move(error),{},
        std::move(server),{}});return;}
      sink->logMCPError(server,*error);
    }catch(...){}
  }
  void mcpUnknown(std::string server,std::shared_ptr<Json> error){
    try{
      if(!sink){queue.push_back({Event::Kind::MCPUnknownError,{},std::move(error),
        std::move(server),{}});return;}
      sink->logMCPUnknownError(server,*error);
    }catch(...){}
  }
  void mcpDebug(std::string server,std::string message){
    try{
      if(!sink){queue.push_back({Event::Kind::MCPDebug,{},{},
        std::move(server),std::move(message)});return;}
      sink->logMCPDebug(server,message);
    }catch(...){}
  }
};
ErrorLogQueue::ErrorLogQueue(ErrorQueueServices services){
  if(!services.hardFailFeature||!services.argv||!services.env||
      !services.isEssentialTrafficOnly||!services.timestampISO||
      !services.hardFailExit)
    throw std::invalid_argument("Error queue imports are required");
  state_=std::make_shared<State>(std::move(services));
}
void ErrorLogQueue::attachErrorLogSink(ErrorLogCallbacks sink){
  state_->attach(std::move(sink));
}
void ErrorLogQueue::logError(const ErrorValue& error){
  state_->log(std::make_shared<ErrorValue>(error));
}
void ErrorLogQueue::logError(std::shared_ptr<ErrorValue> error){
  if(!error)throw std::invalid_argument("Error reference is required");
  state_->log(std::move(error));
}
void ErrorLogQueue::logMCPError(const std::string& server,const ErrorValue& error){
  state_->mcpError(server,std::make_shared<ErrorValue>(error));
}
void ErrorLogQueue::logMCPError(const std::string& server,
    std::shared_ptr<ErrorValue> error){
  if(!error)throw std::invalid_argument("Error reference is required");
  state_->mcpError(server,std::move(error));
}
void ErrorLogQueue::logMCPUnknownError(const std::string& server,const Json& error){
  state_->mcpUnknown(server,std::make_shared<Json>(error));
}
void ErrorLogQueue::logMCPUnknownError(const std::string& server,
    std::shared_ptr<Json> error){
  if(!error)throw std::invalid_argument("Error reference is required");
  state_->mcpUnknown(server,std::move(error));
}
void ErrorLogQueue::logMCPDebug(const std::string& server,const std::string& message){
  state_->mcpDebug(server,message);
}
Json ErrorLogQueue::getInMemoryErrors()const{return state_->inMemory;}
void ErrorLogQueue::resetForTesting(){
  state_->sink.reset();state_->queue.clear();state_->inMemory=Json::array();
}
std::size_t ErrorLogQueue::queuedCount()const{return state_->queue.size();}
}
