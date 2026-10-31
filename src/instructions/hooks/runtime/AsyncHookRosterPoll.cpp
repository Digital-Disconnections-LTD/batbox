#include "instructions/hooks/runtime/AsyncHookRoster.hpp"
#include <memory>
#include <stdexcept>

namespace batbox::instructions::hooks::runtime {
namespace {
std::vector<std::string> splitLines(const std::string& text){
  std::vector<std::string> lines;
  std::size_t from=0;
  for(;;){
    const auto end=text.find('\n',from);
    lines.push_back(text.substr(from,end==std::string::npos?
      std::string::npos:end-from));
    if(end==std::string::npos)break;
    from=end+1;
  }
  return lines;
}
}
struct RosterPoll:std::enable_shared_from_this<RosterPoll>{
  enum class Kind { Skip,Remove,Response };
  struct Outcome {
    Kind kind=Kind::Skip;
    std::string id;
    bool flightStart=false;
    CheckedAsyncHookResponse response;
    std::exception_ptr error;
  };
  AsyncHookRoster& roster;
  event_loop::Deferred<std::vector<CheckedAsyncHookResponse>> output;
  std::vector<std::shared_ptr<PendingAsyncHook>> hooks;
  std::vector<Outcome> outcomes;
  std::size_t remaining=0;
  explicit RosterPoll(AsyncHookRoster& owner):roster(owner),output(owner.loop_){}
  void start(){
    try{
      roster.log("Hooks: Found "+std::to_string(roster.pending_.size())+
        " total hooks in registry");
      for(const auto& id:roster.order_){
        const auto found=roster.pending_.find(id);
        if(found!=roster.pending_.end())hooks.push_back(found->second);
      }
      outcomes.resize(hooks.size());remaining=hooks.size();
      if(hooks.empty()){roster.log("Hooks: checkForNewResponses returning 0 responses");
        output.resolve({});return;}
      for(std::size_t index=0;index<hooks.size();++index)launch(index);
    }catch(...){output.reject(std::current_exception());}
  }
  void finish(std::size_t index,Outcome outcome){
    outcomes[index]=std::move(outcome);
    if(--remaining==0)complete();
  }
  void fail(std::size_t index,std::exception_ptr error){
    Outcome outcome;outcome.error=error;finish(index,std::move(outcome));
  }
  void launch(std::size_t index){
    auto hook=hooks[index];
    try{
      if(!hook->process){
        event_loop::Deferred<std::string> empty(roster.loop_);
        empty.resolve("");
        empty.then([self=shared_from_this(),index](const std::string* stdoutText,
          const std::exception_ptr& error){
          if(error){self->fail(index,error);return;}
          self->afterStdout(index,*stdoutText);
        });
      }else hook->process->getStdout().then(
        [self=shared_from_this(),index](const std::string* stdoutText,
          const std::exception_ptr& error){
          if(error){self->fail(index,error);return;}
          self->afterStdout(index,*stdoutText);
        });
    }catch(...){fail(index,std::current_exception());}
  }
  void afterStdout(std::size_t index,const std::string& stdoutText){
    auto hook=hooks[index];
    try{
      const auto stderrText=hook->process?hook->process->getStderr():"";
      roster.log("Hooks: Checking hook "+hook->processId+" ("+
        hook->hookName+") - attachmentSent: "+
        (hook->responseAttachmentSent?"true":"false")+
        ", stdout length: "+std::to_string(value::stringLength(stdoutText)));
      if(!hook->process){
        roster.log("Hooks: Hook "+hook->processId+
          " has no shell command, removing from registry");
        hook->stopProgressInterval();
        Outcome result;result.kind=Kind::Remove;result.id=hook->processId;
        finish(index,std::move(result));return;
      }
      const auto status=hook->process->status();
      roster.log("Hooks: Hook shell status "+status);
      if(status=="killed"){
        roster.log("Hooks: Hook "+hook->processId+
          " is "+status+", removing from registry");
        hook->stopProgressInterval();hook->process->cleanup();
        Outcome result;result.kind=Kind::Remove;result.id=hook->processId;
        finish(index,std::move(result));return;
      }
      if(status!="completed"){
        finish(index,Outcome{});return;
      }
      if(hook->responseAttachmentSent||value::trim(stdoutText).empty()){
        roster.log("Hooks: Skipping hook "+hook->processId+
          " - already delivered/sent or no stdout");
        hook->stopProgressInterval();
        Outcome result;result.kind=Kind::Remove;result.id=hook->processId;
        finish(index,std::move(result));return;
      }
      const auto lines=splitLines(stdoutText);
      roster.log("Hooks: Processing "+std::to_string(lines.size())+
        " lines of stdout for "+hook->processId);
      hook->process->resultCode().then(
        [self=shared_from_this(),index,stdoutText,stderrText,lines](
          const int* code,const std::exception_ptr& error){
          if(error){self->fail(index,error);return;}
          self->afterResult(index,stdoutText,stderrText,lines,*code);
        });
    }catch(...){fail(index,std::current_exception());}
  }
  void afterResult(std::size_t index,const std::string& stdoutText,
    const std::string& stderrText,const std::vector<std::string>& lines,int code){
    auto hook=hooks[index];
    try{
      Json response=Json::object();
      for(const auto& line:lines){
        const auto trimmed=value::trim(line);
        if(!trimmed.starts_with('{'))continue;
        roster.log("Hooks: Found JSON line: "+value::substring(trimmed,0,100)+"...");
        try{
          const auto parsed=value::parse(trimmed);
          if(!parsed.is_object())throw std::runtime_error("Hook JSON line must be object");
          if(!parsed.contains("async")){
            roster.log("Hooks: Found sync response from "+hook->processId+
              ": "+value::stringify(parsed));
            response=parsed;break;
          }
        }catch(...){
          roster.log("Hooks: Failed to parse JSON from "+hook->processId+
            ": "+trimmed);
        }
      }
      hook->responseAttachmentSent=true;
      roster.finalizeHook(*hook,code,code==0?"success":"error").then(
        [self=shared_from_this(),index,hook,stdoutText,stderrText,
          response,code](const event_loop::Unit*,const std::exception_ptr& error){
          if(error){self->fail(index,error);return;}
          Outcome outcome;outcome.kind=Kind::Response;
          outcome.id=hook->processId;
          outcome.flightStart=hook->hookEvent=="SessionStart";
          outcome.response={hook->processId,hook->hookName,hook->hookEvent,
            stdoutText,stderrText,hook->gadgetName,hook->pluginId,response,code};
          self->finish(index,std::move(outcome));
        });
    }catch(...){fail(index,std::current_exception());}
  }
  void complete(){
    try{
      std::vector<CheckedAsyncHookResponse> responses;
      bool flightStartCompleted=false;
      for(const auto& outcome:outcomes){
        if(outcome.error){
          roster.log("Hooks: checkForAsyncHookResponses callback rejected: "+
            roster.services_.stringifyThrown(outcome.error),"error");
          continue;
        }
        if(outcome.kind==Kind::Remove)roster.erase(outcome.id);
        else if(outcome.kind==Kind::Response){
          responses.push_back(outcome.response);roster.erase(outcome.id);
          if(outcome.flightStart)flightStartCompleted=true;
        }
      }
      if(flightStartCompleted){
        roster.log("Invalidating session env cache after SessionStart hook completed");
        roster.services_.invalidateFlightEnvStash();
      }
      roster.log("Hooks: checkForNewResponses returning "+
        std::to_string(responses.size())+" responses");
      output.resolve(std::move(responses));
    }catch(...){output.reject(std::current_exception());}
  }
};
event_loop::Deferred<std::vector<CheckedAsyncHookResponse>>
AsyncHookRoster::checkForAsyncHookResponses(){
  if(!loop_.isOwnerThread())throw std::logic_error("Async hook registry owner required");
  auto run=std::make_shared<RosterPoll>(*this);
  run->start();return run->output;
}
} // namespace batbox::instructions::hooks::runtime
