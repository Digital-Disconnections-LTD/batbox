#include "cli/remote/WorkerStateUploader.hpp"
#include "runtime/JsonValue.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace batbox::cli::remote {
namespace {
Json spreadObject(const Json& value){
  Json result=Json::object();
  if(value.is_object()){
    for(auto it=value.begin();it!=value.end();++it)
      result[it.key()]=it.value();
  }else if(value.is_array()){
    for(std::size_t index=0;index<value.size();++index)
      result[std::to_string(index)]=value[index];
  }
  return result;
}
}
Json coalesceWorkerPatches(const Json& base,const Json& overlay){
  auto merged=spreadObject(base);
  if(!overlay.is_object())return merged;
  for(auto it=overlay.begin();it!=overlay.end();++it){
    const auto& key=it.key();
    const auto& value=it.value();
    const auto& prior=value::property(merged,key);
    if((key=="external_metadata"||key=="internal_metadata")&&
       value::truthy(prior)&&(prior.is_object()||prior.is_array())&&
       (value.is_object()||value.is_array())){
      auto metadata=spreadObject(prior);
      const auto overlayMetadata=spreadObject(value);
      for(auto member=overlayMetadata.begin();
          member!=overlayMetadata.end();++member)
        metadata[member.key()]=member.value();
      merged[key]=std::move(metadata);
    }else merged[key]=value;
  }
  return merged;
}
WorkerStateUploader::WorkerStateUploader(event_loop::Loop& loop,
    WorkerStateConfig config):loop_(loop),config_(std::move(config)){
  if(!config_.send||!config_.randomUnit)
    throw std::invalid_argument("Worker state uploader imports required");
}
void WorkerStateUploader::enqueue(Json patch){
  if(closed_)return;
  pending_=pending_?coalesceWorkerPatches(*pending_,patch):std::move(patch);
  drain();
}
void WorkerStateUploader::close(){
  closed_=true;pending_.reset();
}
void WorkerStateUploader::drain(){
  if(inflight_||closed_||!pending_)return;
  current_=std::move(pending_);pending_.reset();
  failures_=0;inflight_=true;sendCurrent();
}
double WorkerStateUploader::retryDelay()const{
  const auto exponential=std::min(config_.baseDelayMs*
    std::pow(2.0,static_cast<double>(failures_-1)),config_.maxDelayMs);
  return exponential+config_.randomUnit()*config_.jitterMs;
}
void WorkerStateUploader::sendCurrent(){
  if(closed_){inflight_=false;current_.reset();return;}
  try{
    auto request=config_.send(*current_);
    const std::weak_ptr<WorkerStateUploader> weak=shared_from_this();
    request.then([weak](const bool* success,const std::exception_ptr& error){
      const auto self=weak.lock();if(!self)return;
      if(error){
        return;
      }
      if(success&&*success){
        self->inflight_=false;self->current_.reset();
        if(self->pending_&&!self->closed_)self->drain();
        return;
      }
      ++self->failures_;
      const auto delay=self->retryDelay();
      self->retryTimer_=self->loop_.setTimeout(delay,[weak]{
        if(const auto current=weak.lock()){
          current->retryTimer_={};
          if(current->closed_){
            current->inflight_=false;current->current_.reset();return;
          }
          if(current->pending_){
            current->current_=coalesceWorkerPatches(*current->current_,
              *current->pending_);
            current->pending_.reset();
          }
          current->sendCurrent();
        }
      });
    });
  }catch(...){/* A rejected send retains the in-flight drain. */}
}
}
