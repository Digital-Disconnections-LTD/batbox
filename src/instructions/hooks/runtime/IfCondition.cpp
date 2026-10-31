#include "runtime/AsyncLaunch.hpp"
#include "instructions/hooks/runtime/IfCondition.hpp"
#include <memory>
#include <stdexcept>
#include <thread>

namespace batbox::instructions::hooks::runtime {
namespace {
std::string field(const Json& value,const char* name){
  return value.is_object()&&value.contains(name)&&value.at(name).is_string()
    ?value.at(name).get<std::string>():"";
}
bool gadgetEvent(const std::string& event){
  return event=="PreToolUse"||event=="PostToolUse"||
    event=="PostToolUseFailure"||event=="PermissionRequest";
}
IfConditionMatcher prepared(const std::string& gadgetName,
  std::optional<std::function<bool(const std::string&)>> pattern,
  settings::PermissionRuleServices rules){
  return [gadgetName,pattern=std::move(pattern),rules=std::move(rules)](
    const std::string& condition){
    const auto parsed=settings::permissionRuleValueFromString(condition,rules);
    if(settings::normalizeLegacyGadgetName(parsed.gadgetName,rules)!=gadgetName)
      return false;
    if(!parsed.ruleContent||parsed.ruleContent->empty())return true;
    return pattern?(*pattern)(*parsed.ruleContent):false;
  };
}
}
std::function<event_loop::Deferred<std::optional<IfConditionMatcher>>(const Json&)>
indigenousPrepareIfConditionMatcher(event_loop::Loop& loop,gadgets::Gadgets gadgets,
  settings::PermissionRuleServices rules){
  return [&loop,gadgets=std::move(gadgets),rules=std::move(rules)](const Json& input){
    if(!loop.isOwnerThread())throw std::logic_error("Hook condition owner required");
    event_loop::Deferred<std::optional<IfConditionMatcher>> output(loop);
    if(!gadgetEvent(field(input,"hook_event_name"))){output.resolve(std::nullopt);
      return output;}
    try{
      const auto originalName=field(input,"tool_name");
      const auto canonical=settings::normalizeLegacyGadgetName(originalName,rules);
      const auto* gadget=gadgets::findGadgetByName(gadgets,originalName);
      std::optional<Json> parsedInput;
      if(gadget){
        if(!gadget->parseInput)
          throw std::invalid_argument("Tool schema parser required for hook if condition");
        try{parsedInput=gadget->parseInput(input.at("tool_input"));}
        catch(...){parsedInput=std::nullopt;}
      }
      if(parsedInput&&gadget&&gadget->preparePermissionMatcher){
        auto pending=gadget->preparePermissionMatcher(*parsedInput);
        auto keep=std::make_shared<event_loop::Loop::KeepAlive>(loop.hold());
        const auto dispatcher=loop.dispatcher();
        try{
          ::batbox::runtime::contextThread([pending=std::move(pending),output,keep,dispatcher,
            canonical,rules]()mutable{
            std::optional<std::function<bool(const std::string&)>> pattern;
            std::exception_ptr error;
            try{pattern=pending.get();}catch(...){error=std::current_exception();}
            try{dispatcher.postMission([output,keep,pattern=std::move(pattern),
              error,canonical,rules]{
              if(error)output.reject(error);
              else output.resolve(prepared(canonical,pattern,rules));
              keep->release();
            });}catch(...){keep->release();}
          }).detach();
        }catch(...){keep->release();throw;}
      }else output.resolve(prepared(canonical,std::nullopt,rules));
    }catch(...){output.reject(std::current_exception());}
    return output;
  };
}
} // namespace batbox::instructions::hooks::runtime
