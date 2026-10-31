#include "fs_operations/ReverseLines.hpp"
#include "runtime/JsonValue.hpp"
#include <algorithm>
#include <stdexcept>
#include <utility>

namespace batbox::fs_operations {
struct ReverseLinesCursor::State {
  State(NodeFsBytesImports value,std::string name):
    imports(std::move(value)),path(std::move(name)){}
  NodeFsBytesImports imports;
  std::string path;
  std::shared_ptr<AsyncByteHandle> handle;
  std::shared_ptr<ByteBuffer> buffer;
  ByteBuffer remainder;
  std::vector<std::string> lines;
  std::size_t nextLine=0,position=0;
  bool started=false,done=false,reading=false,closing=false,remainderEmitted=false;
  bool returnRequested=false;
  std::optional<event_loop::Deferred<std::optional<std::string>>> pending;
  std::optional<event_loop::Deferred<event_loop::Unit>> closingResult;
};
namespace {
using State=ReverseLinesCursor::State;
template<class T>void required(const T& callback){
  if(!callback)throw std::invalid_argument("Reverse-line file import is required");
}
void closeHandle(const std::shared_ptr<State>& state,std::exception_ptr primary);
void resolvePending(const std::shared_ptr<State>& state,
    std::optional<std::string> value){
  auto pending=std::move(state->pending);state->pending.reset();
  if(pending)pending->resolve(std::move(value));
  if(pending&&state->returnRequested&&!state->closing)closeHandle(state,{});
}
void failPending(const std::shared_ptr<State>& state,std::exception_ptr error){
  state->done=true;
  auto pending=std::move(state->pending);state->pending.reset();
  if(pending)pending->reject(error);
  if(state->closingResult){
    if(pending&&state->returnRequested)state->closingResult->resolve({});
    else state->closingResult->reject(error);
  }
  state->closingResult.reset();
}
void closeHandle(const std::shared_ptr<State>& state,std::exception_ptr primary){
  if(state->closing)return;
  state->closing=true;
  if(!state->handle){
    if(primary)failPending(state,primary);
    else{
      state->done=true;resolvePending(state,std::nullopt);
      if(state->closingResult)state->closingResult->resolve({});
      state->closingResult.reset();
    }
    return;
  }
  try{
    required(state->handle->close);
    auto close=state->handle->close();
    close.then([state,primary](const event_loop::Unit*,const std::exception_ptr& error){
      if(error){failPending(state,error);return;}
      if(primary){failPending(state,primary);return;}
      state->done=true;resolvePending(state,std::nullopt);
      if(state->closingResult)state->closingResult->resolve({});
      state->closingResult.reset();
    });
  }catch(...){failPending(state,std::current_exception());}
}
void advance(const std::shared_ptr<State>& state);
void afterRead(const std::shared_ptr<State>& state,std::size_t chunkSize){
  ByteBuffer combined;
  combined.reserve(chunkSize+state->remainder.size());
  combined.insert(combined.end(),state->buffer->begin(),
    state->buffer->begin()+chunkSize);
  combined.insert(combined.end(),state->remainder.begin(),state->remainder.end());
  const auto newline=std::find(combined.begin(),combined.end(),0x0a);
  if(newline==combined.end()){
    state->remainder=std::move(combined);
    advance(state);return;
  }
  state->remainder.assign(combined.begin(),newline);
  const auto start=static_cast<std::size_t>(newline-combined.begin())+1;
  const std::string bytes(reinterpret_cast<const char*>(combined.data()+start),
    combined.size()-start);
  const auto decoded=value::decodeUtf8(bytes);
  state->lines.clear();state->nextLine=0;
  std::size_t begin=0;
  while(true){
    const auto end=decoded.find('\n',begin);
    state->lines.push_back(decoded.substr(begin,
      end==std::string::npos?end:end-begin));
    if(end==std::string::npos)break;
    begin=end+1;
  }
  std::reverse(state->lines.begin(),state->lines.end());
  advance(state);
}
void advance(const std::shared_ptr<State>& state){
  if(state->done||state->closing||!state->pending)return;
  while(state->nextLine<state->lines.size()){
    auto line=state->lines[state->nextLine++];
    if(!line.empty()){resolvePending(state,std::move(line));return;}
  }
  state->lines.clear();state->nextLine=0;
  if(state->position==0){
    if(!state->remainderEmitted&&!state->remainder.empty()){
      state->remainderEmitted=true;
      const std::string bytes(reinterpret_cast<const char*>(
        state->remainder.data()),state->remainder.size());
      resolvePending(state,value::decodeUtf8(bytes));return;
    }
    closeHandle(state,{});return;
  }
  const auto chunkSize=std::min(std::size_t(4096),state->position);
  state->position-=chunkSize;
  try{
    required(state->handle->read);
    state->reading=true;
    auto read=state->handle->read(state->buffer,0,chunkSize,state->position);
    read.then([state,chunkSize](const Json*,const std::exception_ptr& error){
      state->reading=false;
      if(error){closeHandle(state,error);return;}
      if(state->closingResult&&!state->pending){closeHandle(state,{});return;}
      afterRead(state,chunkSize);
    });
  }catch(...){state->reading=false;closeHandle(state,std::current_exception());}
}
}
ReverseLinesCursor::ReverseLinesCursor(NodeFsBytesImports imports,std::string path):
    state_(std::make_shared<State>(std::move(imports),std::move(path))){
  if(!state_->imports.loop)throw std::invalid_argument("Owner event loop is required");
}
event_loop::Deferred<std::optional<std::string>> ReverseLinesCursor::next(){
  auto state=state_;
  if(!state->imports.loop->isOwnerThread())
    throw std::logic_error("Reverse-line next requires loop owner");
  event_loop::Deferred<std::optional<std::string>> result(*state->imports.loop);
  if(state->pending)throw std::logic_error("Concurrent reverse-line next");
  if(state->done){result.resolve(std::nullopt);return result;}
  state->pending=result;
  if(state->started){advance(state);return result;}
  state->started=true;
  try{
    required(state->imports.open);
    auto open=state->imports.open(state->path,"r");
    open.then([state](const std::shared_ptr<AsyncByteHandle>* handle,
        const std::exception_ptr& error){
      if(error){failPending(state,error);return;}
      state->handle=*handle;
      try{
        if(!state->handle)throw std::runtime_error("Imported open returned no handle");
        required(state->handle->stat);
        auto stat=state->handle->stat();
        stat.then([state](const Json* value,const std::exception_ptr& statError){
          if(statError){closeHandle(state,statError);return;}
          try{
            required(state->imports.toNumber);
            state->position=static_cast<std::size_t>(
              state->imports.toNumber(value->at("size")));
            state->buffer=std::make_shared<ByteBuffer>(4096);
            advance(state);
          }catch(...){closeHandle(state,std::current_exception());}
        });
      }catch(...){closeHandle(state,std::current_exception());}
    });
  }catch(...){failPending(state,std::current_exception());}
  return result;
}
event_loop::Deferred<event_loop::Unit> ReverseLinesCursor::close(){
  auto state=state_;
  if(!state->imports.loop->isOwnerThread())
    throw std::logic_error("Reverse-line close requires loop owner");
  event_loop::Deferred<event_loop::Unit> result(*state->imports.loop);
  if(state->done){result.resolve({});return result;}
  if(state->closingResult)throw std::logic_error("Reverse-line close already pending");
  state->closingResult=result;
  state->returnRequested=true;
  if(state->pending)return result;
  if(!state->started){closeHandle(state,{});return result;}
  if(!state->reading)closeHandle(state,{});
  return result;
}
}
