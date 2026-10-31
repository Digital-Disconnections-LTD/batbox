#include "commands_indigenous/Compact.hpp"
#include "commands_indigenous/CoreHelpers.hpp"
#include "compaction/Compaction.hpp"
#include "query/OwnerPromiseAll.hpp"
#include <stdexcept>

namespace batbox::commands_indigenous {
namespace {
void required(bool bound,const char* name){
  if(!bound)throw std::invalid_argument(std::string("Compact command import required: ")+name);
}
std::string errorText(std::exception_ptr error){
  try{std::rethrow_exception(error);}
  catch(const std::exception& failure){return failure.what();}
  catch(...){return "Unknown error";}
}
std::optional<std::string> nonempty(const Json& value){
  if(value.is_string()&&!value.get<std::string>().empty())
    return value.get<std::string>();
  return std::nullopt;
}
struct Run:std::enable_shared_from_this<Run> {
  CompactServices services;
  event_loop::Deferred<Json> output;
  Json messages;
  std::string instructions;
  bool inReactive=false;
  Run(std::string args,CompactServices imports):services(std::move(imports)),
    output(services.loop),instructions(value::trim(args)){}
  std::string display(const std::optional<std::string>& message={}){
    required(bool(services.verbose)&&bool(services.getUpgradeTip)&&
      bool(services.getExpandShortcut),"display dependencies");
    const auto tip=services.getUpgradeTip();
    const auto shortcut=services.getExpandShortcut();
    return compactDisplayText(services.verbose(),tip,shortcut,message,
      services.dim);
  }
  Json success(Json result,const std::optional<std::string>& message={}){
    return Json{{"type","compact"},{"compactionResult",std::move(result)},
      {"displayText",display(message)}};
  }
  void reactiveFinish(){
    if(!inReactive)return;
    inReactive=false;
    if(services.setStreamMode)services.setStreamMode("requesting");
    if(services.resetResponseLength)services.resetResponseLength();
    if(services.onCompactProgress)
      services.onCompactProgress(Json{{"type","compact_end"}});
    if(services.setSdkStatus)services.setSdkStatus(std::nullopt);
  }
  void fail(std::exception_ptr error){
    try{
      reactiveFinish();
      const auto message=errorText(error);
      if(services.aborted&&services.aborted())
        throw std::runtime_error("Compaction canceled.");
      if(message==services.notEnoughMessage||
         message==services.incompleteMessage)
        throw std::runtime_error(message);
      if(services.logError)services.logError(error);
      throw std::runtime_error("Error during compaction: Error: "+message);
    }catch(...){output.reject(std::current_exception());}
  }
  void start(){
    try{
      required(bool(services.getMessages)&&bool(services.afterCompactBoundary),
        "messages/boundary");
      messages=services.afterCompactBoundary(services.getMessages());
      if(!messages.is_array()||messages.empty())
        throw std::runtime_error("No messages to compact");
      if(instructions.empty()){
        required(bool(services.tryFlightMemoryCompaction),
          "trySessionMemoryCompaction");
        services.tryFlightMemoryCompaction(messages,
          services.batId?services.batId():std::nullopt).then(
          [self=shared_from_this()](const std::optional<Json>* result,
              const std::exception_ptr& error){
            if(error){self->fail(error);return;}
            try{
              if(result&&*result){self->flightMemorySuccess(**result);return;}
              self->afterFlightMemory();
            }catch(...){self->fail(std::current_exception());}
          });
      }else afterFlightMemory();
    }catch(...){output.reject(std::current_exception());}
  }
  void flightMemorySuccess(const Json& result){
    required(bool(services.clearUserContextStash)&&
      bool(services.runPostCompactCleanup)&&
      bool(services.markPostCompaction)&&
      bool(services.suppressCompactWarning),"Flight memory cleanup");
    services.clearUserContextStash();
    services.runPostCompactCleanup();
    if(services.feature&&services.feature("PROMPT_CACHE_BREAK_DETECTION")){
      required(bool(services.notifyCompaction),"notifyCompaction");
      services.notifyCompaction(services.querySource?services.querySource():"compact",
        services.batId?services.batId():std::nullopt);
    }
    services.markPostCompaction();
    services.suppressCompactWarning();
    output.resolve(success(result));
  }
  void afterFlightMemory(){
    if(services.reactiveOnly&&services.reactiveOnly()){
      reactive();return;
    }
    required(bool(services.microcompactMessages),"microcompactMessages");
    services.microcompactMessages(messages).then(
      [self=shared_from_this()](const Json* result,
          const std::exception_ptr& error){
        if(error){self->fail(error);return;}
        try{
          if(!result)throw std::invalid_argument("Microcompact result missing");
          self->traditionalStash(value::property(*result,"messages"));
        }catch(...){self->fail(std::current_exception());}
      });
  }
  void traditionalStash(const Json& compactMessages){
    required(bool(services.stashSharingParams),"cacheSharingParams");
    services.stashSharingParams(compactMessages).then(
      [self=shared_from_this(),compactMessages](const Json* params,
          const std::exception_ptr& error){
        if(error){self->fail(error);return;}
        try{
          required(bool(self->services.compactConversation),"compactConversation");
          self->services.compactConversation(compactMessages,
            params?*params:Json(),self->instructions).then(
            [self](const Json* result,const std::exception_ptr& failure){
              if(failure){self->fail(failure);return;}
              try{
                if(!result)throw std::invalid_argument("Compaction result missing");
                required(bool(self->services.clearLastSummarizedMessageId)&&
                  bool(self->services.suppressCompactWarning)&&
                  bool(self->services.clearUserContextStash)&&
                  bool(self->services.runPostCompactCleanup),
                  "traditional compaction cleanup");
                self->services.clearLastSummarizedMessageId();
                self->services.suppressCompactWarning();
                self->services.clearUserContextStash();
                self->services.runPostCompactCleanup();
                self->output.resolve(self->success(*result,
                  nonempty(value::property(*result,"userDisplayMessage"))));
              }catch(...){self->fail(std::current_exception());}
            });
        }catch(...){self->fail(std::current_exception());}
      });
  }
  void reactive(){
    inReactive=true;
    if(services.onCompactProgress)
      services.onCompactProgress(Json{{"type","hooks_start"},
        {"hookType","pre_compact"}});
    if(services.setSdkStatus)services.setSdkStatus("compacting");
    try{
      required(bool(services.preCompactHooks)&&bool(services.stashSharingParams),
        "reactive hooks/Stash params");
      auto hooks=services.preCompactHooks(instructions);
      auto params=services.stashSharingParams(messages);
      query::ownerPromiseAll(services.loop,
        std::vector<event_loop::Deferred<Json>>{hooks,params}).then(
        [self=shared_from_this()](const std::vector<Json>* pair,
            const std::exception_ptr& error){
          if(error){self->fail(error);return;}
          try{
            const auto merged=compaction::mergeHookInstructions(
              self->instructions.empty()?std::nullopt:
                std::optional<std::string>(self->instructions),
              nonempty(value::property(pair->at(0),"newCustomInstructions")));
            if(self->services.setStreamMode)
              self->services.setStreamMode("requesting");
            if(self->services.resetResponseLength)
              self->services.resetResponseLength();
            if(self->services.onCompactProgress)
              self->services.onCompactProgress(Json{{"type","compact_start"}});
            required(bool(self->services.reactiveCompact),"reactiveCompact");
            self->services.reactiveCompact(self->messages,pair->at(1),
              merged.value_or("")).then(
              [self,hooks=pair->at(0)](const Json* outcome,
                  const std::exception_ptr& failure){
                if(failure){self->fail(failure);return;}
                try{self->reactiveResult(*outcome,hooks);}
                catch(...){self->fail(std::current_exception());}
              });
          }catch(...){self->fail(std::current_exception());}
        });
    }catch(...){fail(std::current_exception());}
  }
  void reactiveResult(const Json& outcome,const Json& hooks){
    if(value::property(outcome,"ok")!=true){
      const auto& reason=value::property(outcome,"reason");
      if(reason=="too_few_groups")throw std::runtime_error(services.notEnoughMessage);
      if(reason=="aborted")throw std::runtime_error("API Error: Request was aborted.");
      throw std::runtime_error(services.incompleteMessage);
    }
    required(bool(services.clearLastSummarizedMessageId)&&
      bool(services.runPostCompactCleanup)&&
      bool(services.suppressCompactWarning)&&
      bool(services.clearUserContextStash),"reactive cleanup");
    services.clearLastSummarizedMessageId();
    services.runPostCompactCleanup();
    services.suppressCompactWarning();
    services.clearUserContextStash();
    Json result=value::property(outcome,"result");
    std::string combined;
    for(const auto* value:{&value::property(hooks,"userDisplayMessage"),
         &value::property(result,"userDisplayMessage")}){
      if(auto part=nonempty(*value)){
        if(!combined.empty())combined+='\n';
        combined+=*part;
      }
    }
    if(!combined.empty())result["userDisplayMessage"]=combined;
    reactiveFinish();
    output.resolve(success(std::move(result),combined.empty()?std::nullopt:
      std::optional<std::string>(combined)));
  }
};
}
event_loop::Deferred<Json> runCompactCommand(const std::string& args,
    CompactServices services){
  auto run=std::make_shared<Run>(args,std::move(services));
  run->start();return run->output;
}
user_input::CommandEntry makeCompactCommand(
    std::shared_ptr<CompactServices> services){
  if(!services)throw std::invalid_argument("Compact command services required");
  user_input::CommandEntry entry;
  entry.descriptor={{"type","local"},{"name","compact"},
    {"description","Clear conversation history but keep a summary in context"},
    {"supportsNonInteractive",true},
    {"argumentHint","<optional custom summarization instructions>"}};
  entry.loadLocal=[services]{
    event_loop::Deferred<user_input::LocalCall> loaded(services->loop);
    loaded.resolve([services](const std::string& args){
      return runCompactCommand(args,*services);
    });
    return loaded;
  };
  return entry;
}
} // namespace batbox::commands_indigenous
