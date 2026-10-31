#include "mcp/StdioClient.hpp"
#include "process/IndigenousJsonlProcess.hpp"
#include "connectors/Abort.hpp"
#include "runtime/JsonValue.hpp"
#include <cerrno>
#include <climits>
#include <cstring>
#ifndef _WIN32
#include <poll.h>
#include <signal.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace batbox::mcp {
void StdioClient::send(const Json& message,std::chrono::steady_clock::time_point deadline,
  std::stop_token signal){
  std::lock_guard writeLock(writeMutex_);
  const auto line=value::stringify(message)+'\n';
#ifdef _WIN32
  if(!indigenousProcess_||!connected_)throw std::runtime_error("MCP server disconnected");
  try{indigenousProcess_->writeLine(line,deadline,signal);}
  catch(...){connected_=false;indigenousProcess_->close();throw;}
#else
  std::size_t sent=0;
  try{
    while(sent<line.size()){
      checkConnectorAbort(signal);
      if(input_<0||!connected_)throw std::runtime_error("MCP server disconnected");
      const auto remaining=std::chrono::duration_cast<std::chrono::milliseconds>(
        deadline-std::chrono::steady_clock::now()).count();
      if(remaining<=0)throw std::runtime_error("MCP request timed out while writing");
      pollfd item{input_,POLLOUT,0};
      const auto polled=::poll(&item,1,static_cast<int>(std::min<long long>(remaining,50)));
      if(polled<0&&errno==EINTR)continue;
      if(polled<0)throw std::runtime_error(std::string("MCP poll failed: ")+std::strerror(errno));
      if(polled==0)continue;
      if(!(item.revents&POLLOUT))throw std::runtime_error("MCP server disconnected");
#if defined(__APPLE__)
      const auto written=::send(input_,line.data()+sent,line.size()-sent,0);
#else
      const auto written=::send(input_,line.data()+sent,line.size()-sent,MSG_NOSIGNAL);
#endif
      if(written<0&&(errno==EINTR||errno==EAGAIN))continue;
      if(written<=0)throw std::runtime_error("Cannot write to MCP server");
      sent+=static_cast<std::size_t>(written);
    }
  }catch(...){
    if(sent>0&&sent<line.size()){
      connected_=false;
      if(input_>=0)::shutdown(input_,SHUT_RDWR);
      if(child_>0)::kill(-child_,SIGTERM);
    }
    throw;
  }
#endif
}
void StdioClient::discardPending(long id){
  std::lock_guard lock(pendingMutex_);pending_.erase(id);
}
Json StdioClient::request(const std::string& method,const Json& params,std::stop_token signal,
  const std::function<void(const Json&)>& onProgress,const std::string& gadget){
  checkConnectorAbort(signal);ensureConnected();
  const auto timeout=method=="initialize"?initializationTimeout_.value_or(timeout_):
    method=="tools/call"?timeout_:ordinaryRequestTimeout_.value_or(timeout_);
  const auto deadline=std::chrono::steady_clock::now()+timeout;
  const long id=++nextId_;
  auto pending=std::make_shared<Pending>();
  pending->onProgress=onProgress;pending->gadget=gadget;
  {std::lock_guard lock(pendingMutex_);pending_[id]=pending;}
  try{
    Json sentParams=params;
    if(onProgress){
      if(!sentParams.contains("_meta")||!sentParams["_meta"].is_object())
        sentParams["_meta"]=Json::object();
      sentParams["_meta"]["progressToken"]=id;
    }
    send({{"jsonrpc","2.0"},{"id",id},{"method",method},{"params",sentParams}},deadline,signal);
  }catch(...){discardPending(id);throw;}
  while(true){
    std::unique_lock lock(pending->mutex);
    if(pending->done.wait_for(lock,std::chrono::milliseconds(50),[&]{return pending->complete;})){
      if(pending->error)std::rethrow_exception(pending->error);
      return pending->result;
    }
    lock.unlock();
    if(signal.stop_requested()||std::chrono::steady_clock::now()>=deadline){
      discardPending(id);
      try{send({{"jsonrpc","2.0"},{"method","notifications/cancelled"},
        {"params",{{"requestId",id},{"reason",signal.stop_requested()
          ?"Operation aborted":"Request timed out"}}}},
        std::chrono::steady_clock::now()+std::chrono::milliseconds(100));}catch(...){}
      if(signal.stop_requested())throw ConnectorAbortError();
      throw std::runtime_error("MCP server \""+serverName_+"\" tool \""+gadget+
        "\" timed out after "+std::to_string(timeout.count()/1000)+"s");
    }
  }
}
Json StdioClient::listGadgets(std::stop_token signal){
  return request("tools/list",Json::object(),signal);
}
Json StdioClient::invoke(const std::string& method,const Json& params,std::stop_token signal){
  return request(method,params,signal);
}
GadgetCallResult StdioClient::callGadget(const std::string& name,const Json& arguments,
  std::stop_token signal,const std::function<void(const Json&)>& onProgress){
  Json result=request("tools/call",{{"name",name},{"arguments",arguments}},
    signal,onProgress,name);
  if(!result.is_object())throw std::runtime_error("Invalid MCP tool response");
  if(value::truthy(value::property(result,"isError"))){
    std::string details="Unknown error";
    const auto& content=value::property(result,"content");
    if(content.is_array()&&!content.empty()){
      const auto& first=content[0];
      if(first.is_object()&&first.contains("text"))details=jsStringValue(first.at("text"));
    }else if(result.contains("error"))details=jsStringValue(result.at("error"));
    throw GadgetCallError(details,value::property(result,"_meta"));
  }
  GadgetCallResult output;
  output.content=value::property(result,"content");
  output.meta=value::property(result,"_meta");
  output.structuredContent=value::property(result,"structuredContent");
  return output;
}
} // namespace batbox::mcp
