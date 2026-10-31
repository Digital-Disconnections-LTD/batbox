#include "user_input/SlashInternal.hpp"
#include "user_input/CommandMessages.hpp"
#include <stdexcept>

namespace batbox::user_input {
using namespace internal;
namespace {
void rejectOrResolve(const event_loop::Deferred<InputResult>& output,
  const InputResult* result,const std::exception_ptr& error,
  messages::Services& services){
  if(error){output.reject(error);return;}
  try{output.resolve(finalizeCommandResult(*result,services));}
  catch(...){output.reject(std::current_exception());}
}
}
event_loop::Deferred<InputResult> processSlashCommand(const std::string& input,
  const Json& preceding,const Json& images,const Json& attachments,
  const std::optional<std::string>& uuid,SlashServices& services){
  event_loop::Deferred<InputResult> output(services.loop);
  try{
    if(!preceding.is_array()||!images.is_array()||!attachments.is_array())
      throw std::invalid_argument("Slash command blocks must be arrays");
    const auto parsed=commands::parseSlashCommand(input);
    if(!parsed){
      const std::string error="Commands are in the form `/command [args]`";
      InputResult result;
      result.messages.push_back(createSyntheticUserCaveatMessage(services.messages));
      for(const auto& item:attachments)result.messages.push_back(item);
      result.messages.push_back(userMessage(error,preceding,services.messages));
      result.resultText=error;
      output.resolve(std::move(result));
      return output;
    }
    const auto* entry=findEntry(parsed->commandName,services.commands);
    if(!entry){
      require(static_cast<bool>(services.pathExists),"pathExists");
      auto stat=services.pathExists("/"+parsed->commandName);
      stat.then([output,input,parsed,preceding,attachments,uuid,&services]
        (const bool* exists,const std::exception_ptr& error){
        try{
          const bool isFilePath=!error&&exists&&*exists;
          InputResult result;
          if(commands::looksLikeCommand(parsed->commandName)&&!isFilePath){
            const auto message="Unknown skill: "+parsed->commandName;
            result.messages.push_back(createSyntheticUserCaveatMessage(services.messages));
            for(const auto& item:attachments)result.messages.push_back(item);
            result.messages.push_back(userMessage(message,preceding,services.messages));
            if(!parsed->args.empty())result.messages.push_back(
              createSystemMessage("Args from unknown skill: "+parsed->args,
                "warning",services.messages));
            result.resultText=message;
          }else{
            result.messages.push_back(userMessage(input,preceding,services.messages,uuid));
            for(const auto& item:attachments)result.messages.push_back(item);
            result.shouldQuery=true;
          }
          output.resolve(std::move(result));
        }catch(...){output.reject(std::current_exception());}
      });
      return output;
    }
    auto dispatched=dispatchKnown(*entry,parsed->commandName,parsed->args,
      preceding,images,uuid,services);
    dispatched.then([output,&services](const InputResult* result,
      const std::exception_ptr& error){
      rejectOrResolve(output,result,error,services.messages);
    });
  }catch(...){output.reject(std::current_exception());}
  return output;
}
} // namespace batbox::user_input
