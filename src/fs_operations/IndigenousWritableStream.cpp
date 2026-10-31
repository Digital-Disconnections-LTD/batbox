#include "fs_operations/IndigenousWritableStream.hpp"
#include "runtime/Encoding.hpp"
#include "runtime/IndigenousPath.hpp"
#include <algorithm>
#include <atomic>
#include <cerrno>
#include <deque>
#ifndef _WIN32
#include <fcntl.h>
#include <unistd.h>
#else
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace batbox::fs_operations {
namespace {
[[noreturn]] void fail(const char* operation){
#ifdef _WIN32
  throw std::system_error(static_cast<int>(::GetLastError()),std::system_category(),operation);
#else
  throw std::system_error(errno,std::generic_category(),operation);
#endif
}
#ifdef _WIN32
using IndigenousHandle=HANDLE;
const IndigenousHandle invalidHandle=INVALID_HANDLE_VALUE;
void closeIndigenous(IndigenousHandle handle){if(handle!=invalidHandle)::CloseHandle(handle);}
#else
using IndigenousHandle=int;
constexpr IndigenousHandle invalidHandle=-1;
void closeIndigenous(IndigenousHandle handle){if(handle>=0)(void)::close(handle);}
#endif
struct IndigenousFd {
  explicit IndigenousFd(IndigenousHandle value):descriptor(value){}
  ~IndigenousFd(){closeIndigenous(descriptor.exchange(invalidHandle));}
  std::atomic<IndigenousHandle> descriptor;
};
void writeAll(const std::shared_ptr<IndigenousFd>& fd,const std::string& chunk){
  for(std::size_t offset=0;offset<chunk.size();){
#ifdef _WIN32
    const auto wanted=static_cast<DWORD>(std::min<std::size_t>(chunk.size()-offset,0xffffffffu));
    DWORD size=0;
    if(!::WriteFile(fd->descriptor.load(),chunk.data()+offset,wanted,&size,nullptr))
      fail("write stream");
#else
    const auto size=::write(fd->descriptor.load(),chunk.data()+offset,
      chunk.size()-offset);
    if(size<0){if(errno==EINTR)continue;fail("write stream");}
#endif
    if(size==0)throw std::runtime_error("Short stream write");
    offset+=static_cast<std::size_t>(size);
  }
}
}
struct IndigenousWritableStream::State {
  State(event_loop::Loop& owner,std::shared_ptr<IndigenousAsyncFsHost> workers,
      std::string name):loop(owner),host(std::move(workers)),path(std::move(name)){}
  event_loop::Loop& loop;
  std::shared_ptr<IndigenousAsyncFsHost> host;
  std::string path;
  std::shared_ptr<IndigenousFd> fd;
  std::deque<std::string> chunks;
  std::size_t bufferedBytes=0;
  bool opened=false,openSettled=false,writing=false,ended=false,finished=false,failed=false;
  bool abandoned=false,needDrain=false,closing=false,closed=false;
  std::vector<std::function<void()>> finishListeners,drainListeners,closeListeners;
  std::vector<std::function<void(const std::exception_ptr&)>> errorListeners;
};
namespace {
using State=IndigenousWritableStream::State;
void pump(const std::shared_ptr<State>& state);
void emitClose(const std::shared_ptr<State>& state){
  if(state->closed)return;
  state->closed=true;
  const auto listeners=state->closeListeners;
  for(const auto& listener:listeners)listener();
}
void closeFile(const std::shared_ptr<State>& state){
  if(state->closing||!state->fd)return;
  state->closing=true;
  auto closing=state->host->submit<event_loop::Unit>([fd=state->fd]{
    const auto descriptor=fd->descriptor.exchange(invalidHandle);
#ifdef _WIN32
    if(descriptor!=invalidHandle&&!::CloseHandle(descriptor))fail("close stream");
#else
    if(descriptor>=0&&::close(descriptor)<0)fail("close stream");
#endif
    return event_loop::Unit{};
  });
  std::weak_ptr<State> weak=state;
  closing.then([weak](const event_loop::Unit*,const std::exception_ptr& error){
    if(const auto state=weak.lock()){
      state->fd.reset();
      if(error&&!state->abandoned){
        state->failed=true;
        if(state->errorListeners.empty())std::rethrow_exception(error);
        const auto listeners=state->errorListeners;
        for(const auto& listener:listeners)listener(error);
      }
      emitClose(state);
    }
  });
}
void emitError(const std::shared_ptr<State>& state,std::exception_ptr error){
  if(state->failed||state->abandoned)return;
  state->failed=true;
  state->chunks.clear();state->bufferedBytes=0;
  bool closeImmediately=false;
  if(!state->writing){
    if(state->fd)closeFile(state);
    else closeImmediately=state->openSettled;
  }
  if(state->errorListeners.empty())std::rethrow_exception(error);
  const auto listeners=state->errorListeners;
  for(const auto& listener:listeners)listener(error);
  if(closeImmediately)emitClose(state);
}
void finish(const std::shared_ptr<State>& state){
  if(state->finished||state->failed||state->abandoned)return;
  state->finished=true;
  const auto listeners=state->finishListeners;
  for(const auto& listener:listeners)listener();
  closeFile(state);
}
void pump(const std::shared_ptr<State>& state){
  if(state->closing||state->writing||!state->opened)return;
  if(state->failed||state->abandoned){closeFile(state);return;}
  if(state->chunks.empty()){
    if(state->ended)finish(state);
    return;
  }
  auto chunk=std::move(state->chunks.front());state->chunks.pop_front();
  state->writing=true;
  const auto count=chunk.size();
  auto operation=state->host->submit<event_loop::Unit>(
    [fd=state->fd,chunk=std::move(chunk)]{
      writeAll(fd,chunk);return event_loop::Unit{};
    });
  std::weak_ptr<State> weak=state;
  operation.then([weak,count](const event_loop::Unit*,const std::exception_ptr& error){
    const auto state=weak.lock();if(!state)return;
    state->writing=false;
    state->bufferedBytes-=count;
    if(error){
      if(state->abandoned)emitClose(state);
      else emitError(state,error);
      return;
    }
    if(state->needDrain&&!state->ended&&state->bufferedBytes==0){
      state->needDrain=false;
      const auto listeners=state->drainListeners;
      for(const auto& listener:listeners)listener();
    }
    pump(state);
  });
}
}
IndigenousWritableStream::IndigenousWritableStream(event_loop::Loop& loop,
    std::shared_ptr<IndigenousAsyncFsHost> host,std::string path):
    state_(std::make_shared<State>(loop,std::move(host),std::move(path))){
  if(!state_->host)throw std::invalid_argument("Filesystem worker host required");
}
void IndigenousWritableStream::start(){
  auto state=state_;
  if(!state->loop.isOwnerThread())throw std::logic_error("Stream owner required");
  auto opening=state->host->submit<std::shared_ptr<IndigenousFd>>([path=state->path]{
#ifdef _WIN32
    const auto indigenousPath=runtime::pathFromUtf8(value::encodeUtf8(path));
    const auto descriptor=::CreateFileW(indigenousPath.c_str(),GENERIC_WRITE,
      FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,CREATE_ALWAYS,
      FILE_ATTRIBUTE_NORMAL,nullptr);
    if(descriptor==invalidHandle)fail("open stream");
#else
    const auto indigenousPath=value::encodeUtf8(path);
    const int descriptor=::open(indigenousPath.c_str(),
      O_WRONLY|O_CREAT|O_TRUNC|O_CLOEXEC,0666);
    if(descriptor<0)fail("open stream");
#endif
    return std::make_shared<IndigenousFd>(descriptor);
  });
  std::weak_ptr<State> weak=state;
  opening.then([weak](const std::shared_ptr<IndigenousFd>* fd,
      const std::exception_ptr& error){
    const auto state=weak.lock();if(!state)return;
    state->openSettled=true;
    if(error){emitError(state,error);return;}
    state->fd=*fd;state->opened=true;
    pump(state);
  });
}
bool IndigenousWritableStream::write(std::string_view bytes){
  auto state=state_;
  if(!state->loop.isOwnerThread())throw std::logic_error("Stream owner required");
  if(state->ended||state->abandoned||state->failed){
    std::weak_ptr<State> weak=state;
    state->loop.queueMicrotask([weak]{if(auto current=weak.lock())
      emitError(current,std::make_exception_ptr(
        std::logic_error("Write after stream end")));});
    return false;
  }
  state->chunks.emplace_back(bytes);
  state->bufferedBytes+=bytes.size();
  const bool canWrite=state->bufferedBytes<16384;
  if(!canWrite)state->needDrain=true;
  pump(state);
  return canWrite;
}
void IndigenousWritableStream::end(){
  auto state=state_;
  if(!state->loop.isOwnerThread())throw std::logic_error("Stream owner required");
  if(state->ended||state->abandoned)return;
  state->ended=true;pump(state);
}
void IndigenousWritableStream::abort(){
  auto state=state_;
  if(!state->loop.isOwnerThread())throw std::logic_error("Stream owner required");
  if(state->abandoned)return;
  state->abandoned=true;state->chunks.clear();
  if(!state->writing){
    if(state->fd)closeFile(state);
    else if(state->openSettled)emitClose(state);
  }
}
void IndigenousWritableStream::onFinish(std::function<void()> callback){
  if(!state_->loop.isOwnerThread())throw std::logic_error("Stream owner required");
  state_->finishListeners.push_back(std::move(callback));
}
void IndigenousWritableStream::onError(
    std::function<void(const std::exception_ptr&)> callback){
  if(!state_->loop.isOwnerThread())throw std::logic_error("Stream owner required");
  state_->errorListeners.push_back(std::move(callback));
}
void IndigenousWritableStream::onDrain(std::function<void()> callback){
  if(!state_->loop.isOwnerThread())throw std::logic_error("Stream owner required");
  state_->drainListeners.push_back(std::move(callback));
}
void IndigenousWritableStream::onClose(std::function<void()> callback){
  if(!state_->loop.isOwnerThread())throw std::logic_error("Stream owner required");
  state_->closeListeners.push_back(std::move(callback));
}
std::shared_ptr<WritableStream> createIndigenousWritableStream(
    event_loop::Loop& loop,std::shared_ptr<IndigenousAsyncFsHost> host,
    const std::string& path){
  auto stream=std::make_shared<IndigenousWritableStream>(loop,std::move(host),path);
  stream->start();return stream;
}
void bindIndigenousCreateWriteStream(FsImplementation& implementation,
    event_loop::Loop& loop,std::shared_ptr<IndigenousAsyncFsHost> host){
  implementation.createWriteStream=[&loop,host=std::move(host)](
      const std::string& path){
    return createIndigenousWritableStream(loop,host,path);
  };
}
}
