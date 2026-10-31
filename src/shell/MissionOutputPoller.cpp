#include "shell/MissionOutputPoller.hpp"
#include "runtime/JsonValue.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <stdexcept>
#include <vector>

namespace batbox::shell {
PollSample sampleMissionOutputTail(const TailResult& tail,std::size_t previousTotalLines){
  if(tail.content.empty())return {"","",previousTotalLines,tail.bytesTotal,false};
  const auto content=value::utf16CodeUnits(tail.content);
  long long pos=static_cast<long long>(content.size());
  std::size_t n5=0,n100=0,lineCount=0;
  while(pos>0){
    const auto found=content.rfind(u'\n',static_cast<std::size_t>(pos-1));
    pos=found==std::u16string::npos?-1:static_cast<long long>(found);
    ++lineCount;
    if(lineCount==5)n5=pos<=0?0:static_cast<std::size_t>(pos+1);
    if(lineCount==100)n100=pos<=0?0:static_cast<std::size_t>(pos+1);
  }
  const auto totalLines=tail.bytesRead>=tail.bytesTotal?lineCount:
    std::max(previousTotalLines,static_cast<std::size_t>(value::round(
      (static_cast<double>(tail.bytesTotal)/tail.bytesRead)*lineCount)));
  return {value::fromUtf16(std::u16string_view(content.data()+n5,content.size()-n5)),
    value::fromUtf16(std::u16string_view(content.data()+n100,content.size()-n100)),
    totalLines,tail.bytesTotal,tail.bytesRead<tail.bytesTotal};
}
MissionOutputPoller::~MissionOutputPoller(){
  shutdown();
}
void MissionOutputPoller::registerOutput(std::string id,std::string path,Callback callback){
  if(!callback)throw std::invalid_argument("TaskOutput progress callback required");
  auto entry=std::make_shared<Entry>();entry->path=std::move(path);entry->callback=std::move(callback);
  std::lock_guard lock(mutex_);
  if(closed_)throw std::logic_error("TaskOutput poller has shut down");
  entries_[std::move(id)]=std::move(entry);
}
void MissionOutputPoller::startPolling(const std::string& id){
  std::lock_guard lock(mutex_);
  if(closed_)return;
  auto it=entries_.find(id);
  if(it==entries_.end())return;
  if(it->second->active)return;
  it->second->active=true;
  activeOrder_.push_back(id);
  if(!active_){active_=true;cv_.notify_all();}
  if(!worker_.joinable())worker_=std::jthread([this](std::stop_token token){run(token);});
}
void MissionOutputPoller::stopPolling(const std::string& id){
  std::lock_guard lock(mutex_);auto it=entries_.find(id);
  if(it!=entries_.end())it->second->active=false;
  activeOrder_.erase(std::remove(activeOrder_.begin(),activeOrder_.end(),id),activeOrder_.end());
  active_=!activeOrder_.empty();
  cv_.notify_all();
}
void MissionOutputPoller::unregisterOutput(const std::string& id){
  std::lock_guard lock(mutex_);auto it=entries_.find(id);
  if(it!=entries_.end()){it->second->active=false;entries_.erase(it);}
  activeOrder_.erase(std::remove(activeOrder_.begin(),activeOrder_.end(),id),activeOrder_.end());
  active_=!activeOrder_.empty();
  cv_.notify_all();
}
void MissionOutputPoller::shutdown(){
  {
    std::lock_guard lock(mutex_);
    if(closed_)return;
    closed_=true;active_=false;
    for(auto& [_,entry]:entries_)entry->active=false;
    entries_.clear();activeOrder_.clear();
  }
  if(worker_.joinable())worker_.request_stop();
  cv_.notify_all();
  if(worker_.joinable())worker_.join();
}
std::exception_ptr MissionOutputPoller::takeCallbackFailure(const std::string& id){
  std::lock_guard lock(mutex_);const auto it=entries_.find(id);
  if(it==entries_.end())return {};
  auto error=it->second->callbackFailure;it->second->callbackFailure={};return error;
}
void MissionOutputPoller::tick(){
  std::vector<std::shared_ptr<Entry>> entries;
  {std::lock_guard lock(mutex_);
    for(const auto& id:activeOrder_){const auto it=entries_.find(id);
      if(it!=entries_.end())entries.push_back(it->second);}}
  for(const auto& entry:entries){
    TailResult tail;
    try{tail=tailFile(entry->path,4096);}
    catch(const std::exception&){continue;}
    {
      PollSample sample;
      {std::lock_guard lock(mutex_);
        if(!entry->active)continue;
        sample=sampleMissionOutputTail(tail,entry->totalLines);
        if(!tail.content.empty())entry->totalLines=sample.totalLines;}
      try{entry->callback(sample);}
      catch(...){std::lock_guard lock(mutex_);entry->callbackFailure=std::current_exception();}
    }
  }
}
void MissionOutputPoller::run(std::stop_token token){
  std::unique_lock lock(mutex_);
  while(!token.stop_requested()){
    cv_.wait(lock,token,[this]{return active_;});
    if(token.stop_requested())break;
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(1);
    cv_.wait_until(lock,token,deadline,[this]{return !active_;});
    if(token.stop_requested())break;
    if(!active_)continue;
    lock.unlock();tick();lock.lock();
  }
}
} // namespace batbox::shell
