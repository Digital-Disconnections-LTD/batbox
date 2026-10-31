#include "fs_operations/FileRanges.hpp"
#include "diagnostics/SlowOperations.hpp"
#include "runtime/JsonValue.hpp"
#include <algorithm>
#include <stdexcept>
#include <utility>

namespace batbox::fs_operations {
namespace {
template<class T>void required(const T& callback){
  if(!callback)throw std::invalid_argument("File range import is required");
}
struct State {
  State(event_loop::Loop& loop,bool isTail):rangeResult(loop),tailResult(loop),tail(isTail){}
  event_loop::Deferred<std::optional<Json>> rangeResult;
  event_loop::Deferred<Json> tailResult;
  NodeFsBytesImports imports;
  std::shared_ptr<AsyncByteHandle> handle;
  std::shared_ptr<ByteBuffer> buffer;
  std::size_t offset=0,maxBytes=0,position=0,bytesToRead=0,totalRead=0;
  double size=0;
  bool tail=false,finishing=false,emptyRange=false;
};
void settle(const std::shared_ptr<State>& state,std::exception_ptr error){
  if(error){
    if(state->tail)state->tailResult.reject(error);
    else state->rangeResult.reject(error);
    return;
  }
  if(state->emptyRange){state->rangeResult.resolve(std::nullopt);return;}
  std::string bytes;
  if(state->totalRead)bytes.assign(
    reinterpret_cast<const char*>(state->buffer->data()),state->totalRead);
  Json result=Json::object({{"content",value::decodeUtf8(bytes)},
    {"bytesRead",state->totalRead},{"bytesTotal",state->size}});
  if(state->tail)state->tailResult.resolve(std::move(result));
  else state->rangeResult.resolve(std::move(result));
}
void finish(const std::shared_ptr<State>& state,std::exception_ptr primary){
  if(state->finishing)return;
  state->finishing=true;
  try{
    required(state->handle->close);
    auto close=state->handle->close();
    close.then([state,primary](const event_loop::Unit*,const std::exception_ptr& closeError){
      if(primary&&closeError){
        settle(state,std::make_exception_ptr(diagnostics::SuppressedOperationError(
          closeError,primary)));
      }else settle(state,closeError?closeError:primary);
    });
  }catch(...){
    auto closeError=std::current_exception();
    if(primary)settle(state,std::make_exception_ptr(
      diagnostics::SuppressedOperationError(closeError,primary)));
    else settle(state,closeError);
  }
}
void nextRead(const std::shared_ptr<State>& state){
  if(state->totalRead>=state->bytesToRead){finish(state,{});return;}
  try{
    required(state->handle->read);
    auto read=state->handle->read(state->buffer,state->totalRead,
      state->bytesToRead-state->totalRead,state->position+state->totalRead);
    read.then([state](const Json* result,const std::exception_ptr& error){
      if(error){finish(state,error);return;}
      try{
        const auto count=result->at("bytesRead").get<std::size_t>();
        if(count>state->bytesToRead-state->totalRead)
          throw std::runtime_error("Invalid imported read result");
        if(count==0){finish(state,{});return;}
        state->totalRead+=count;
        nextRead(state);
      }catch(...){finish(state,std::current_exception());}
    });
  }catch(...){finish(state,std::current_exception());}
}
void afterStat(const std::shared_ptr<State>& state,const Json& stats){
  try{
    required(state->imports.toNumber);required(state->imports.allocUnsafe);
    state->size=state->imports.toNumber(stats.at("size"));
    if(!state->tail&&state->size<=static_cast<double>(state->offset)){
      state->emptyRange=true;finish(state,{});return;
    }
    if(state->tail){
      if(state->size==0){
        state->buffer=std::make_shared<ByteBuffer>();finish(state,{});return;
      }
      state->position=state->size>state->maxBytes?
        static_cast<std::size_t>(state->size-state->maxBytes):0;
      state->bytesToRead=static_cast<std::size_t>(state->size)-state->position;
    }else{
      state->position=state->offset;
      state->bytesToRead=std::min(static_cast<std::size_t>(state->size-state->offset),
        state->maxBytes);
    }
    state->buffer=state->imports.allocUnsafe(static_cast<double>(state->bytesToRead));
    if(!state->buffer)throw std::runtime_error("Imported allocation returned no buffer");
    nextRead(state);
  }catch(...){finish(state,std::current_exception());}
}
void afterOpen(const std::shared_ptr<State>& state,
    const std::shared_ptr<AsyncByteHandle>& handle){
  state->handle=handle;
  try{
    if(!handle)throw std::runtime_error("Imported open returned no handle");
    required(handle->stat);
    auto stat=handle->stat();
    stat.then([state](const Json* value,const std::exception_ptr& error){
      if(error){finish(state,error);return;}
      afterStat(state,*value);
    });
  }catch(...){finish(state,std::current_exception());}
}
std::shared_ptr<State> start(const NodeFsBytesImports& imports,
    const std::string& path,std::size_t offset,std::size_t maxBytes,bool tail){
  if(!imports.loop)throw std::invalid_argument("Owner event loop is required");
  auto state=std::make_shared<State>(*imports.loop,tail);
  state->imports=imports;state->offset=offset;state->maxBytes=maxBytes;
  try{
    required(imports.open);
    auto open=imports.open(path,"r");
    open.then([state](const std::shared_ptr<AsyncByteHandle>* handle,
        const std::exception_ptr& error){
      if(error){settle(state,error);return;}
      afterOpen(state,*handle);
    });
  }catch(...){settle(state,std::current_exception());}
  return state;
}
}
event_loop::Deferred<std::optional<Json>> readFileRange(
    const NodeFsBytesImports& imports,const std::string& path,
    std::size_t offset,std::size_t maxBytes){
  return start(imports,path,offset,maxBytes,false)->rangeResult;
}
event_loop::Deferred<Json> tailFile(const NodeFsBytesImports& imports,
    const std::string& path,std::size_t maxBytes){
  return start(imports,path,0,maxBytes,true)->tailResult;
}
}
