#include "runtime/AsyncLaunch.hpp"
#include "gadgets/bash/AsyncBashPersistence.hpp"
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <thread>
#include <utility>

namespace batbox::bash_gadget {
namespace {
using event_loop::Deferred;
using event_loop::Unit;
constexpr std::uintmax_t maxPersistedSize=64ull*1024*1024;
void require(bool valid,const char* name){
  if(!valid)throw std::invalid_argument(std::string("Bash file import required: ")+name);
}
struct State:std::enable_shared_from_this<State>{
  event_loop::Loop& loop;
  shell::ExecResult result;
  AsyncBashFileOps operations;
  BashPersistedOutput persisted;
  Deferred<BashPersistedOutput> output;
  std::string destination;
  std::function<void(const BashPersistedOutput&)> onComplete;
  bool finished=false;
  State(event_loop::Loop& owner,shell::ExecResult value,AsyncBashFileOps ops,
    std::function<void(const BashPersistedOutput&)> complete):
    loop(owner),result(std::move(value)),operations(std::move(ops)),output(owner),
    onComplete(std::move(complete)){}
  void finish(){
    if(finished)return;
    finished=true;
    try{if(onComplete)onComplete(persisted);output.resolve(persisted);}
    catch(...){output.reject(std::current_exception());}
  }
  void start(){
    if(!result.outputFilePath||result.outputFilePath->empty()||
      !result.outputMissionId||result.outputMissionId->empty()){finish();return;}
    try{operations.stat(*result.outputFilePath).then([self=shared_from_this()](
      const std::uintmax_t* size,const std::exception_ptr& error){
      if(error){self->finish();return;}
      self->persisted.originalSize=*size;
      self->afterStat();
    });}catch(...){finish();}
  }
  void afterStat(){
    try{operations.ensureDir().then([self=shared_from_this()](
      const Unit*,const std::exception_ptr& error){
      if(error){self->finish();return;}
      self->afterEnsure();
    });}catch(...){finish();}
  }
  void afterEnsure(){
    try{
      destination=operations.getGadgetResultPath(*result.outputMissionId,false);
      if(*persisted.originalSize>maxPersistedSize){
        operations.truncate(*result.outputFilePath,maxPersistedSize).then(
          [self=shared_from_this()](const Unit*,const std::exception_ptr& error){
          if(error){self->finish();return;}
          self->afterTruncate();
        });
      }else afterTruncate();
    }catch(...){finish();}
  }
  void afterTruncate(){
    try{operations.link(*result.outputFilePath,destination).then(
      [self=shared_from_this()](const Unit*,const std::exception_ptr& error){
        if(error){self->afterLinkFailure();return;}
        self->persisted.path=self->destination;self->finish();
      });}catch(...){afterLinkFailure();}
  }
  void afterLinkFailure(){
    try{operations.copyFile(*result.outputFilePath,destination).then(
      [self=shared_from_this()](const Unit*,const std::exception_ptr& error){
        if(!error)self->persisted.path=self->destination;
        self->finish();
      });}catch(...){finish();}
  }
};
template<class Result,class Operation>Deferred<Result> onWorker(event_loop::Loop& loop,
  Operation operation){
  Deferred<Result> output(loop);
  auto keep=std::make_shared<event_loop::Loop::KeepAlive>(loop.hold());
  const auto dispatcher=loop.dispatcher();
  try{
    ::batbox::runtime::contextThread([output,operation=std::move(operation),keep,dispatcher]()mutable{
      std::optional<Result> value;std::exception_ptr error;
      try{value=operation();}catch(...){error=std::current_exception();}
      try{dispatcher.postMission([output,value=std::move(value),error,keep]{
        if(error)output.reject(error);else output.resolve(*value);
        keep->release();
      });}catch(...){keep->release();}
    }).detach();
  }catch(...){keep->release();output.reject(std::current_exception());}
  return output;
}
} // namespace
event_loop::Deferred<BashPersistedOutput> persistBashOutputAsync(
  event_loop::Loop& loop,const shell::ExecResult& result,
  const AsyncBashFileOps& operations,
  std::function<void(const BashPersistedOutput&)> onComplete){
  if(!loop.isOwnerThread())throw std::logic_error("Bash persistence owner required");
  if(!result.outputFilePath||result.outputFilePath->empty()||
    !result.outputMissionId||result.outputMissionId->empty()){
    Deferred<BashPersistedOutput> output(loop);
    if(onComplete)onComplete({});
    output.resolve({});return output;
  }
  require(bool(operations.stat),"stat");require(bool(operations.ensureDir),"ensureDir");
  require(bool(operations.getGadgetResultPath),"getToolResultPath");
  require(bool(operations.truncate),"truncate");require(bool(operations.link),"link");
  require(bool(operations.copyFile),"copyFile");
  auto state=std::make_shared<State>(loop,result,operations,std::move(onComplete));
  auto output=state->output;state->start();return output;
}
AsyncBashFileOps indigenousBashFileOps(event_loop::Loop& loop,std::string resultsDir,
  std::function<std::string(const std::string&,bool)> getGadgetResultPath){
  if(resultsDir.empty()||!getGadgetResultPath)
    throw std::invalid_argument("Bash results directory/path required");
  AsyncBashFileOps ops;
  ops.stat=[&loop](const std::string& path){
    return onWorker<std::uintmax_t>(loop,[path]{return std::filesystem::file_size(path);});};
  ops.ensureDir=[&loop,dir=std::move(resultsDir)]{
    return onWorker<Unit>(loop,[dir]{std::filesystem::create_directories(dir);return Unit{};});};
  ops.getGadgetResultPath=std::move(getGadgetResultPath);
  ops.truncate=[&loop](const std::string& path,std::uintmax_t size){
    return onWorker<Unit>(loop,[path,size]{std::filesystem::resize_file(path,size);return Unit{};});};
  ops.link=[&loop](const std::string& path,const std::string& destination){
    return onWorker<Unit>(loop,[path,destination]{
      std::filesystem::create_hard_link(path,destination);return Unit{};});};
  ops.copyFile=[&loop](const std::string& path,const std::string& destination){
    return onWorker<Unit>(loop,[path,destination]{
      std::filesystem::copy_file(path,destination,
        std::filesystem::copy_options::overwrite_existing);return Unit{};});};
  return ops;
}
} // namespace batbox::bash_gadget
