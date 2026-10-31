#include "gadgets/synthetic_output/SyntheticOutput.hpp"
#include "instructions/hooks/runtime/HookResponseSchema.hpp"
#include <future>
#include <stdexcept>

namespace batbox::gadgets::synthetic_output {
namespace {
template<class T>std::future<T> ready(T value){
  std::promise<T> result;result.set_value(std::move(value));
  return result.get_future();
}
Json parseObject(const Json& input){
  if(!input.is_object())
    throw std::invalid_argument("SyntheticOutput input must be an object");
  return input;
}
}
bool isEnabledForNonInteractiveFlight(bool isNonInteractiveFlight){
  return isNonInteractiveFlight;
}
Gadget makeSyntheticOutputGadget(const std::string& protocolName){
  if(protocolName.empty())
    throw std::invalid_argument("Structured-output protocol name required");
  Gadget gadget;
  gadget.name=protocolName;
  gadget.isMcp=false;
  gadget.searchHint="return the final response as structured JSON";
  gadget.maxResultSizeChars=100000;
  gadget.inputSchema={{"type","object"},{"additionalProperties",true}};
  gadget.outputSchema={{"type","string"},
    {"description","Structured output tool result"}};
  gadget.parseInput=parseObject;
  gadget.isEnabled=[](){return true;};
  gadget.isConcurrencySafe=[](const Json&){return true;};
  gadget.isReadOnly=[](const Json&){return true;};
  gadget.isOpenWorld=[](const Json&){return false;};
  gadget.description=[](const Json&,const Json&){
    return ready(std::string("Return structured output in the requested format"));
  };
  gadget.poetry=[](const PoetryContext&){
    return ready(std::string("Use this tool to return your final response in the requested structured format. You MUST call this tool exactly once at the end of your response to provide the structured output."));
  };
  gadget.call=[](const Json& input,const GadgetCallContext&,
    const std::function<std::future<Json>(const Json&)>&,const Json&,
    const std::function<void(const GadgetProgress&)>&){
    GadgetResult result;
    result.data="Structured output provided successfully";
    result.structuredOutput=input;
    return ready(std::move(result));
  };
  gadget.checkPermissions=[](const Json& input,const Json&){
    return ready(Json{{"behavior","allow"},{"updatedInput",input}});
  };
  gadget.renderGadgetUseMessage=[](const Json& input,const Json&)->Json{
    if(!input.is_object()||input.empty())return nullptr;
    if(input.size()<=3){
      std::string result;
      for(auto it=input.begin();it!=input.end();++it){
        if(!result.empty())result+=", ";
        result+=it.key()+": "+value::stringify(it.value());
      }
      return result;
    }
    std::string result=std::to_string(input.size())+" fields: ";
    std::size_t count=0;
    for(auto it=input.begin();it!=input.end()&&count<3;++it,++count){
      if(count)result+=", ";
      result+=it.key();
    }
    return result+"…";
  };
  gadget.renderGadgetUseRejectedMessage=[](const Json&,const Json&)->Json{
    return "Structured output rejected";};
  gadget.renderGadgetUseErrorMessage=[](const Json&,const Json&)->Json{
    return "Structured output error";};
  gadget.renderGadgetUseProgressMessage=[](const Json&,const Json&)->Json{
    return nullptr;};
  gadget.renderGadgetResultMessage=[](const Json& output,const Json&,
    const Json&)->Json{return output;};
  gadget.mapGadgetResultToGadgetResultBlockParam=[](const Json& content,
    const std::string& gadgetUseID){return Json{{"tool_use_id",gadgetUseID},
      {"type","tool_result"},{"content",content}};};
  return buildGadget(std::move(gadget));
}
Gadget makeHookStructuredOutputGadget(const Gadget& base){
  Gadget gadget=base;
  gadget.inputSchema={{"type","object"},{"properties",
    {{"ok",{{"type","boolean"}}},{"reason",{{"type","string"}}}}},
    {"required",Json::array({"ok"})}};
  gadget.inputJSONSchema={{"type","object"},{"properties",
    {{"ok",{{"type","boolean"},
      {"description","Whether the condition was met"}}},
     {"reason",{{"type","string"},
      {"description","Reason, if the condition was not met"}}}}},
    {"required",Json::array({"ok"})},
    {"additionalProperties",false}};
  gadget.parseInput=[](const Json& input){
    const auto parsed=instructions::hooks::runtime::
      hookResponseSchemaSafeParse(input);
    if(!parsed.success)throw std::invalid_argument(parsed.errorMessage);
    return parsed.data;
  };
  gadget.poetry=[](const PoetryContext&){
    return ready(std::string("Use this tool to return your verification result. You MUST call this tool exactly once at the end of your response."));
  };
  return gadget;
}
} // namespace batbox::gadgets::synthetic_output
