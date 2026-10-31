#include "user_input/SlashInternal.hpp"
#include "user_input/CommandMessages.hpp"

namespace batbox::user_input::internal {
event_loop::Deferred<InputResult> runUiCommand(const CommandEntry&,
  const std::string&,const Json&,SlashServices&);
event_loop::Deferred<InputResult> runLocalCommand(const CommandEntry&,
  const std::string&,const Json&,SlashServices&);
namespace {
event_loop::Deferred<InputResult> immediate(event_loop::Loop& loop,InputResult value){
  event_loop::Deferred<InputResult> result(loop);result.resolve(std::move(value));return result;
}
}
event_loop::Deferred<InputResult> dispatchKnown(const CommandEntry& entry,
  const std::string& name,const std::string& args,const Json& preceding,
  const Json& images,const std::optional<std::string>& uuid,
  SlashServices& services){
  const auto& command=entry.descriptor;
  const auto type=field(command,"type");
  if(type=="prompt"&&value::property(command,"userInvocable")!=false){
    require(static_cast<bool>(services.recordSkillUsage),"recordSkillUsage");
    services.recordSkillUsage(name);
  }
  if(value::property(command,"userInvocable")==false){
    InputResult value;
    value.messages.push_back(userMessage("/"+name,preceding,services.messages));
    value.messages.push_back(messages::createUserMessage(Json{{"content",
      "This skill can only be invoked by Batbox, not directly by users. Ask Batbox to use the \""+
      name+"\" skill for you."}},services.messages));
    return immediate(services.loop,std::move(value));
  }
  if(type=="local-jsx")return runUiCommand(entry,args,preceding,services);
  if(type=="local")return runLocalCommand(entry,args,preceding,services);
  event_loop::Deferred<InputResult> output(services.loop);
  if(type!="prompt"){
    output.reject(std::make_exception_ptr(std::runtime_error("Unknown command type: "+type)));
    return output;
  }
  const bool fork=value::property(command,"context")=="fork";
  try{
    if(fork)require(static_cast<bool>(entry.runForked),"runForked");
    else require(static_cast<bool>(entry.expandPoetry),"expandPrompt");
    auto pending=fork?entry.runForked(args,preceding):
      entry.expandPoetry(args,preceding,images,uuid);
    const auto displayName=nameOf(entry);
    pending.then([output,displayName,args,preceding,&services]
      (const InputResult* value,const std::exception_ptr& error){
      try{
        if(!error){output.resolve(*value);return;}
        InputResult result;
        const auto text=formatCommandInputTags(displayName,args);
        result.messages.push_back(userMessage(text,preceding,services.messages));
        if(services.isAbortError&&services.isAbortError(error))
          result.messages.push_back(createUserInterruptionMessage(false,services.messages));
        else result.messages.push_back(messages::createUserMessage(Json{{"content",
          "<local-command-stderr>"+errorText(error)+"</local-command-stderr>"}},
          services.messages));
        output.resolve(std::move(result));
      }catch(...){output.reject(std::current_exception());}
    });
  }catch(...){output.reject(std::current_exception());}
  return output;
}
} // namespace batbox::user_input::internal
