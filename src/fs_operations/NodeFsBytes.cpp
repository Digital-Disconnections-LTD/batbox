#include "fs_operations/NodeFsBytes.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace batbox::fs_operations {
namespace {
template<class T>void required(const T& callback){
  if(!callback)throw std::invalid_argument("Node byte filesystem import is required");
}
struct ReadState {
  explicit ReadState(event_loop::Loop& loop):result(loop){}
  event_loop::Deferred<ByteBuffer> result;
  NodeFsBytesImports imports;
  std::optional<Json> importsMaxBytes;
  std::shared_ptr<AsyncByteHandle> handle;
  std::shared_ptr<ByteBuffer> buffer;
  std::size_t readSize=0;
  std::size_t offset=0;
  bool finishing=false;
};
void finish(const std::shared_ptr<ReadState>& state,std::exception_ptr primary){
  if(state->finishing)return;
  state->finishing=true;
  try{
    required(state->handle->close);
    auto closing=state->handle->close();
    closing.then([state,primary](const event_loop::Unit*,const std::exception_ptr& closeError){
      if(closeError){state->result.reject(closeError);return;}
      if(primary){state->result.reject(primary);return;}
      if(state->offset<state->readSize)state->buffer->resize(state->offset);
      state->result.resolve(std::move(*state->buffer));
    });
  }catch(...){state->result.reject(std::current_exception());}
}
void readNext(const std::shared_ptr<ReadState>& state){
  if(state->offset>=state->readSize){finish(state,{});return;}
  try{
    required(state->handle->read);
    auto reading=state->handle->read(state->buffer,state->offset,
      state->readSize-state->offset,state->offset);
    reading.then([state](const Json* readResult,const std::exception_ptr& error){
      if(error){finish(state,error);return;}
      try{
        const auto value=readResult->at("bytesRead").get<double>();
        if(!std::isfinite(value)||value<0||std::floor(value)!=value||
            value>static_cast<double>(state->readSize-state->offset))
          throw std::runtime_error("Invalid imported read result");
        const auto bytesRead=static_cast<std::size_t>(value);
        if(bytesRead==0){finish(state,{});return;}
        state->offset+=bytesRead;
        readNext(state);
      }catch(...){finish(state,std::current_exception());}
    });
  }catch(...){finish(state,std::current_exception());}
}
void afterOpen(const std::shared_ptr<ReadState>& state,
    const std::shared_ptr<AsyncByteHandle>& handle){
  state->handle=handle;
  try{
    if(!state->handle)throw std::runtime_error("Imported open returned no file handle");
    required(state->handle->stat);
    auto stat=state->handle->stat();
    stat.then([state](const Json* stats,const std::exception_ptr& error){
      if(error){finish(state,error);return;}
      try{
        required(state->imports.toNumber);required(state->imports.allocUnsafe);
        const auto size=state->imports.toNumber(stats->at("size"));
        const auto max=state->imports.toNumber(*state->importsMaxBytes);
        const auto readSize=(std::isnan(size)||std::isnan(max))
          ?std::numeric_limits<double>::quiet_NaN():std::min(size,max);
        state->buffer=state->imports.allocUnsafe(readSize);
        if(!state->buffer)throw std::runtime_error("Imported allocation returned no buffer");
        state->readSize=state->buffer->size();
        readNext(state);
      }catch(...){finish(state,std::current_exception());}
    });
  }catch(...){finish(state,std::current_exception());}
}
}
NodeFsBytes::NodeFsBytes(NodeFsBytesImports imports):imports_(std::move(imports)){
  if(!imports_.loop)throw std::invalid_argument("Owner event loop is required");
}
event_loop::Deferred<ByteBuffer> NodeFsBytes::readFileBytes(
    const std::string& path,std::optional<Json> maxBytes)const{
  event_loop::Deferred<ByteBuffer> result(*imports_.loop);
  if(!maxBytes){
    try{
      required(imports_.readFile);
      auto inner=imports_.readFile(path);
      return event_loop::adoptAsyncReturn(inner);
    }catch(...){result.reject(std::current_exception());return result;}
  }
  auto state=std::make_shared<ReadState>(*imports_.loop);
  state->imports=imports_;
  state->importsMaxBytes=std::move(maxBytes);
  try{
    required(imports_.open);
    auto opening=imports_.open(path,"r");
    opening.then([state](const std::shared_ptr<AsyncByteHandle>* handle,
        const std::exception_ptr& error){
      if(error){state->result.reject(error);return;}
      afterOpen(state,*handle);
    });
  }catch(...){state->result.reject(std::current_exception());}
  return state->result;
}
}
