#include "user_input/SlashInternal.hpp"
#include "user_input/CommandMessages.hpp"

namespace batbox::user_input::internal {
namespace {
void localError(const event_loop::Deferred<InputResult>& output,
  const Json& user,const std::exception_ptr& error,SlashServices& services){
  if(services.logError)services.logError(errorText(error));
  InputResult result;
  result.messages=Json::array({user,createCommandInputMessage(
    "<local-command-stderr>"+errorText(error)+"</local-command-stderr>",
    services.messages)});
  output.resolve(std::move(result));
}
}
event_loop::Deferred<InputResult> runLocalCommand(const CommandEntry& entry,
  const std::string& args,const Json& preceding,SlashServices& services){
  event_loop::Deferred<InputResult> output(services.loop);
  try{
    require(static_cast<bool>(entry.loadLocal),"loadLocal");
    const auto command=entry.descriptor;
    const auto displayArgs=value::truthy(value::property(command,"isSensitive"))&&
      !value::trim(args).empty()?"***":args;
    const auto formatted=formatCommandInputTags(nameOf(entry),displayArgs);
    const auto user=userMessage(formatted,preceding,services.messages);
    const auto caveat=createSyntheticUserCaveatMessage(services.messages);
    try{
    auto loaded=entry.loadLocal();
    loaded.then([output,user,caveat,args,&services]
      (const LocalCall* call,const std::exception_ptr& loadError){
      if(loadError){
        localError(output,user,loadError,services);return;
      }
      try{
        require(static_cast<bool>(*call),"local.call");
        auto pending=(*call)(args);
        pending.then([output,user,caveat,&services]
          (const Json* callResult,const std::exception_ptr& callError){
          try{
            InputResult result;
            if(callError){
              localError(output,user,callError,services);return;
            }
            const auto type=field(*callResult,"type");
            if(type=="skip"){output.resolve(std::move(result));return;}
            if(type=="compact"){
              Json slashMessages=Json::array({caveat,user});
              const auto& display=value::property(*callResult,"displayText");
              if(value::truthy(display)){
                require(static_cast<bool>(services.nowISOAfterMs),"nowISOAfterMs");
                Json message=messages::createUserMessage(Json{{"content",
                  "<local-command-stdout>"+display.get<std::string>()+
                  "</local-command-stdout>"},{"timestamp",services.nowISOAfterMs(100)}},
                  services.messages);
                slashMessages.push_back(std::move(message));
              }
              auto compact=value::property(*callResult,"compactionResult");
              if(!compact.is_object())throw std::runtime_error("Missing compaction result");
              Json keep=value::property(compact,"messagesToKeep");
              if(!keep.is_array())keep=Json::array();
              for(const auto& message:slashMessages)keep.push_back(message);
              compact["messagesToKeep"]=std::move(keep);
              require(static_cast<bool>(services.resetMicrocompactState),"resetMicrocompactState");
              services.resetMicrocompactState();
              require(static_cast<bool>(services.buildPostCompactMessages),"buildPostCompactMessages");
              result.messages=services.buildPostCompactMessages(compact);
              output.resolve(std::move(result));return;
            }
            if(type!="text")throw std::runtime_error("Unexpected local command result: "+type);
            const auto text=field(*callResult,"value");
            result.messages=Json::array({user,createCommandInputMessage(
              "<local-command-stdout>"+text+"</local-command-stdout>",services.messages)});
            result.resultText=text;
            output.resolve(std::move(result));
          }catch(...){localError(output,user,std::current_exception(),services);}
        });
      }catch(...){localError(output,user,std::current_exception(),services);}
    });
    }catch(...){localError(output,user,std::current_exception(),services);}
  }catch(...){output.reject(std::current_exception());}
  return output;
}
} // namespace batbox::user_input::internal
