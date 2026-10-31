#include "commands/PoetryBinding.hpp"
#include <stdexcept>

namespace batbox::commands {
namespace {
user_input::InputResult convert(const Json& value){
  if(!value.is_object()||!value.contains("messages")||
     !value.at("messages").is_array())
    throw std::runtime_error("Prompt command returned invalid messages");
  user_input::InputResult result;
  result.messages=value.at("messages");
  result.shouldQuery=value::property(value,"shouldQuery")==true;
  const auto& allowed=value::property(value,"allowedTools");
  if(!allowed.is_null())result.allowedGadgets=allowed;
  for(auto [name,target]:{
      std::pair{"model",&result.model},
      std::pair{"effort",&result.effort},
      std::pair{"resultText",&result.resultText},
      std::pair{"nextInput",&result.nextInput}}){
    const auto& field=value::property(value,name);
    if(field.is_string())*target=field.get<std::string>();
  }
  const auto& submit=value::property(value,"submitNextInput");
  if(submit.is_boolean())result.submitNextInput=submit.get<bool>();
  return result;
}
}
user_input::CommandEntry bindInlinePoetryCommand(event_loop::Loop& loop,
  Json descriptor,std::shared_ptr<bats::BatContext> context,
  std::shared_ptr<skills::process::PoetryCommandServices> services){
  if(!context||!services)throw std::invalid_argument("Prompt command context and services required");
  user_input::CommandEntry entry;
  entry.descriptor=std::move(descriptor);
  entry.expandPoetry=[&loop,command=entry.descriptor,context=std::move(context),
    services=std::move(services)](const std::string& args,const Json& preceding,
    const Json& images,const std::optional<std::string>& uuid){
    event_loop::Deferred<user_input::InputResult> output(loop);
    try{
      auto value=skills::process::getMessagesForPoetrySlashCommand(
        command,args,*context,*services,preceding,images,uuid);
      output.resolve(convert(value));
    }catch(...){output.reject(std::current_exception());}
    return output;
  };
  return entry;
}
} // namespace batbox::commands
