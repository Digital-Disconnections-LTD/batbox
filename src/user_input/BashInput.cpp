#include "user_input/BashInput.hpp"
#include "user_input/CommandMessages.hpp"
#include "missions/shell/Roster.hpp"
#include "gadgets/bash/BashGadget.hpp"
#include <stdexcept>

namespace batbox::user_input {
namespace {
std::string stringField(const Json& value,const char* key){
  const auto& item=value::property(value,key);
  return item.is_string()?item.get<std::string>():std::string{};
}
InputResult resultWith(const Json& caveat,const Json& user,const Json& attachments,
  const std::optional<Json>& output){
  InputResult result;
  result.messages=Json::array({caveat,user});
  for(const auto& attachment:attachments)result.messages.push_back(attachment);
  if(output)result.messages.push_back(*output);
  return result;
}
Json userOutput(const std::string& content,messages::Services& services){
  return messages::createUserMessage(Json{{"content",content}},services);
}
void finishError(const event_loop::Deferred<InputResult>& output,
  const std::exception_ptr& error,const Json& caveat,const Json& user,
  const Json& attachments,messages::Services& messages){
  try{
    std::rethrow_exception(error);
  }catch(const bash_gadget::ShellError& shell){
    if(shell.interrupted){
      auto result=resultWith(caveat,user,Json::array(),
        createUserInterruptionMessage(false,messages));
      for(const auto& attachment:attachments)result.messages.push_back(attachment);
      output.resolve(std::move(result));return;
    }
    auto result=resultWith(caveat,user,attachments,userOutput(
      "<bash-stdout>"+shell_missions::escapeXml(shell.stdoutText)+
      "</bash-stdout><bash-stderr>"+shell_missions::escapeXml(shell.stderrText)+
      "</bash-stderr>",messages));
    output.resolve(std::move(result));return;
  }catch(const std::exception& cause){
    auto result=resultWith(caveat,user,attachments,userOutput(
      "<bash-stderr>Command failed: "+shell_missions::escapeXml(cause.what())+
      "</bash-stderr>",messages));
    output.resolve(std::move(result));return;
  }catch(...){
    auto result=resultWith(caveat,user,attachments,userOutput(
      "<bash-stderr>Command failed: Unknown error</bash-stderr>",messages));
    output.resolve(std::move(result));return;
  }
}
}
event_loop::Deferred<InputResult> processBashCommand(const std::string& input,
  const Json& preceding,const Json& attachments,BashInputServices& services){
  event_loop::Deferred<InputResult> output(services.loop);
  try{
    if(!preceding.is_array()||!attachments.is_array())
      throw std::invalid_argument("Bash input blocks must be arrays");
    if(!services.isPowerShellGadgetEnabled||!services.resolveDefaultShell||
       !services.callShell||!services.processGadgetResultBlock||
       !services.showProgress||!services.clearProgress)
      throw std::invalid_argument("Missing Bash input service binding");
    const bool usePowerShell=services.isPowerShellGadgetEnabled()&&
      services.resolveDefaultShell()=="powershell";
    const Json caveat=createSyntheticUserCaveatMessage(services.messages);
    const Json user=messages::createUserMessage(Json{{"content",
      messages::prepareUserContent(Json{{"inputString","<bash-input>"+input+
        "</bash-input>"},{"precedingInputBlocks",preceding}})}},services.messages);
    services.showProgress(input,std::nullopt);
    auto pending=services.callShell(usePowerShell,Json{{"command",input},
      {"dangerouslyDisableSandbox",true}},
      [&services,input](const Json& progress){services.showProgress(input,progress);});
    pending.then([output,caveat,user,attachments,usePowerShell,&services]
      (const Json* response,const std::exception_ptr& error){
      if(error){
        services.clearProgress();
        finishError(output,error,caveat,user,attachments,services.messages);
        return;
      }
      try{
        const auto& data=value::property(*response,"data");
        if(!value::truthy(data))throw std::runtime_error("No result received from shell command");
        const auto stderrText=stringField(data,"stderr");
        Json forMapping=data;
        forMapping["stderr"]="";
        if(!services.messages.randomUUID)
          throw std::invalid_argument("Missing randomUUID binding");
        auto mapped=services.processGadgetResultBlock(usePowerShell,forMapping,
          services.messages.randomUUID());
        mapped.then([output,caveat,user,attachments,stderrText,data,&services]
          (const Json* block,const std::exception_ptr& mapError){
          services.clearProgress();
          if(mapError){
            finishError(output,mapError,caveat,user,attachments,
              services.messages);return;
          }
          try{
            const auto& content=value::property(*block,"content");
            const auto stdoutText=content.is_string()?content.get<std::string>():
              shell_missions::escapeXml(stringField(data,"stdout"));
            output.resolve(resultWith(caveat,user,attachments,userOutput(
              "<bash-stdout>"+stdoutText+"</bash-stdout><bash-stderr>"+
              shell_missions::escapeXml(stderrText)+"</bash-stderr>",services.messages)));
          }catch(...){output.reject(std::current_exception());}
        });
      }catch(...){
        services.clearProgress();
        finishError(output,std::current_exception(),caveat,user,attachments,
          services.messages);
      }
    });
  }catch(...){
    if(services.clearProgress)services.clearProgress();
    output.reject(std::current_exception());
  }
  return output;
}
} // namespace batbox::user_input
