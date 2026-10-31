#include "application_hooks/Provider.hpp"
#include "application_hooks/PoetryArguments.hpp"
#include "instructions/hooks/runtime/HookResponseSchema.hpp"
#include "instructions/hooks/runtime/PoetryModelBinding.hpp"
#include "messages/Messages.hpp"
#include "messages/TextContent.hpp"
#include "shell/ShellQuote.hpp"
#include "skills/Skill.hpp"
#include "runtime/JsonValue.hpp"
#include <stdexcept>

namespace batbox::application_hooks {
namespace {
std::string failureText(std::exception_ptr error){
  try{if(error)std::rethrow_exception(error);}
  catch(const std::exception& value){return value.what();}
  catch(...){return "unknown prompt hook error";}
  return {};
}
std::optional<std::vector<std::string>> parseArguments(
    const std::string& raw){
  try{
    const auto value=shell_quote::parse(raw,[](const std::string& name)
        ->std::optional<Json>{return Json("$"+name);});
    if(!value.is_array())return {};
    std::vector<std::string> result;
    for(const auto& part:value)if(part.is_string())
      result.push_back(part.get<std::string>());
    return result;
  }catch(...){return {};}
}
}
std::string hookPoetryArguments(const std::string& poetry,
    const std::string& input){
  skills::SkillServices skill;
  skill.tryParseShellCommand=parseArguments;
  return skills::substituteArguments(poetry,input,true,{},skill);
}
void Provider::bindPoetryHooks(){
  if(!inputs_.model)return;
  hooks::PoetryHookServices services;
  services.randomUUID=inputs_.randomUUID;
  services.getSmallFastModel=[model=inputs_.model]{
    return model->config().smallModel;};
  services.addArgumentsToPoetry=hookPoetryArguments;
  services.logForDebugging=inputs_.log;
  services.createUserMessage=[messages=inputs_.messages](
      const std::string& poetry){
    if(!messages)throw std::logic_error("Prompt hook message services missing");
    return messages::createUserMessage(Json{{"content",poetry}},*messages);
  };
  hooks::bindPoetryModelBackend(owner_,services,
    [model=inputs_.model](model::SimpleQueryRequest request){
      if(!request.sourceContext)throw std::logic_error(
        "Prompt hook native tool snapshot missing");
      const auto context=std::static_pointer_cast<IndigenousHookContext>(
        request.sourceContext);
      return model->queryPoetryHook(std::move(request),context->gadgets);
    });
  services.extractTextContent=[](const Json& content){
    return messages::extractTextContent(content);};
  services.addResponseLength=[add=inputs_.addResponseLength](std::size_t size){
    if(!add)throw std::logic_error("Prompt hook response-length import missing");
    add(size);
  };
  services.safeParseJSON=[](const std::string& value)->std::optional<Json>{
    auto parsed=value::parse(value,false);
    if(parsed.is_discarded())return {};
    return parsed;
  };
  services.schemaSafeParse=hooks::hookResponseSchemaSafeParse;
  services.createAttachmentMessage=inputs_.createAttachmentMessage;
  services.errorMessage=failureText;
  poetryHost_=std::make_shared<hooks::PoetryHookHost>(owner_,
    std::move(services));
}
} // namespace batbox::application_hooks
