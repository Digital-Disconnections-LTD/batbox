#include "application_engine/GadgetErrors.hpp"
#include "bats/RunBat.hpp"
#include "gadgets/bash/BashGadget.hpp"
#include "transport/Errors.hpp"
#include "runtime/JsonValue.hpp"

namespace batbox::application_engine {
namespace {
constexpr const char* interrupted="[Request interrupted by user for tool use]";
}
std::vector<std::string> getGadgetErrorParts(const std::exception& error){
  if(const auto* shell=dynamic_cast<const bash_gadget::ShellError*>(&error))
    return {"Exit code "+std::to_string(shell->code),
      shell->interrupted?interrupted:"",shell->stderrText,shell->stdoutText};
  return {error.what()};
}
std::string formatGadgetError(const std::exception& error){
  if(dynamic_cast<const transport::APIUserAbortError*>(&error)||
      dynamic_cast<const bats::BatAbortError*>(&error)){
    const std::string message=error.what();
    return message.empty()?interrupted:message;
  }
  std::string message;
  for(const auto& part:getGadgetErrorParts(error))if(!part.empty()){
    if(!message.empty())message+='\n';
    message+=part;
  }
  message=value::trim(message);
  if(message.empty())message="Command failed with no output";
  const auto length=value::stringLength(message);
  if(length<=10000)return message;
  return value::slice(message,0,5000)+"\n\n... ["+
    std::to_string(length-10000)+" characters truncated] ...\n\n"+
    value::slice(message,length-5000);
}
}
