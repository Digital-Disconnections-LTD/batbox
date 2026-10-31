#include "history/WriterInternal.hpp"
#include <stdexcept>
#include <utility>

namespace batbox::history {
namespace {
template<class T>void required(const T& callback){
  if(!callback)throw std::invalid_argument("History writer import is required");
}
struct FlushRun {
  FlushRun(const std::shared_ptr<HistoryWriter::Core>& value):
    core(value),result(*value->services.loop){}
  std::shared_ptr<HistoryWriter::Core> core;
  event_loop::Deferred<event_loop::Unit> result;
  std::string path;
  HistoryRelease release;
};
void finish(const std::shared_ptr<FlushRun>& run,std::exception_ptr primary){
  if(!run->release){
    if(primary)run->result.reject(primary);else run->result.resolve({});
    return;
  }
  try{
    auto operation=run->release();
    operation.then([run,primary](const event_loop::Unit*,
        const std::exception_ptr& releaseError){
      if(releaseError)run->result.reject(releaseError);
      else if(primary)run->result.reject(primary);
      else run->result.resolve({});
    });
  }catch(...){run->result.reject(std::current_exception());}
}
void caught(const std::shared_ptr<FlushRun>& run,std::exception_ptr error){
  try{
    required(run->core->services.logForDebugging);
    required(run->core->services.errorToString);
    run->core->services.logForDebugging("Failed to write prompt history: "+
      run->core->services.errorToString(error));
    finish(run,{});
  }catch(...){finish(run,std::current_exception());}
}
void append(const std::shared_ptr<FlushRun>& run){
  try{
    required(run->core->services.jsonStringify);
    required(run->core->services.appendFile);
    std::string lines;
    const auto count=run->core->state->pendingEntries.size();
    for(std::size_t i=0;i<count;++i){
      if(i<run->core->state->pendingEntries.size()&&
          run->core->state->pendingEntries[i])
        lines+=run->core->services.jsonStringify(
          *run->core->state->pendingEntries[i]);
      else lines+="null";
      lines+='\n';
    }
    run->core->state->pendingEntries.clear();
    auto operation=run->core->services.appendFile(run->path,lines,
      Json::object({{"mode",0600}}));
    operation.then([run](const event_loop::Unit*,const std::exception_ptr& error){
      if(error)caught(run,error);else finish(run,{});
    });
  }catch(...){caught(run,std::current_exception());}
}
void acquire(const std::shared_ptr<FlushRun>& run){
  try{
    required(run->core->services.lock);
    const auto options=Json::object({{"stale",10000},
      {"retries",Json::object({{"retries",3},{"minTimeout",50}})}});
    auto operation=run->core->services.lock(run->path,options);
    operation.then([run](const HistoryRelease* release,
        const std::exception_ptr& error){
      if(error){caught(run,error);return;}
      run->release=*release;
      append(run);
    });
  }catch(...){caught(run,std::current_exception());}
}
}
event_loop::Deferred<event_loop::Unit> immediateFlush(
    const std::shared_ptr<HistoryWriter::Core>& core){
  event_loop::Deferred<event_loop::Unit> empty(*core->services.loop);
  if(core->state->pendingEntries.empty()){empty.resolve({});return empty;}
  auto run=std::make_shared<FlushRun>(core);
  try{
    required(core->services.getConfigHomeDir);required(core->services.join);
    required(core->services.writeFile);
    run->path=core->services.join(core->services.getConfigHomeDir(),
      "history.jsonl");
    auto operation=core->services.writeFile(run->path,"",
      Json::object({{"encoding","utf8"},{"mode",0600},{"flag","a"}}));
    operation.then([run](const event_loop::Unit*,const std::exception_ptr& error){
      if(error)caught(run,error);else acquire(run);
    });
  }catch(...){caught(run,std::current_exception());}
  return run->result;
}
event_loop::Deferred<event_loop::Unit> flushPoetry(
    const std::shared_ptr<HistoryWriter::Core>& core,int retries){
  event_loop::Deferred<event_loop::Unit> result(*core->services.loop);
  if(core->state->isWriting||core->state->pendingEntries.empty()||retries>5){
    result.resolve({});return result;
  }
  core->state->isWriting=true;
  auto after=[core,result,retries](std::exception_ptr primary){
    core->state->isWriting=false;
    if(core->state->pendingEntries.empty()){
      if(primary)result.reject(primary);else result.resolve({});
      return;
    }
    try{
      required(core->services.sleep);
      auto pause=core->services.sleep(500);
      pause.then([core,result,retries,primary](const event_loop::Unit*,
          const std::exception_ptr& error){
        if(error){result.reject(error);return;}
        try{(void)flushPoetry(core,retries+1);}catch(...){}
        if(primary)result.reject(primary);else result.resolve({});
      });
    }catch(...){result.reject(std::current_exception());}
  };
  try{
    auto operation=immediateFlush(core);
    operation.then([after](const event_loop::Unit*,const std::exception_ptr& error){
      after(error);
    });
  }catch(...){after(std::current_exception());}
  return result;
}
event_loop::Deferred<event_loop::Unit> HistoryWriter::immediateFlushHistory(){
  return immediateFlush(core_);
}
event_loop::Deferred<event_loop::Unit> HistoryWriter::flushPoetryHistory(int retries){
  return flushPoetry(core_,retries);
}
}
