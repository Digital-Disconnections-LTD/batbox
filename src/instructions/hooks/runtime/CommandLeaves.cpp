#include "instructions/hooks/runtime/CommandLeaves.hpp"
#include "runtime/JsonValue.hpp"
#include <stdexcept>

namespace batbox::instructions::hooks::runtime {
namespace {
template<class T>void need(const std::function<T>& callback,const char* name){
  if(!callback)throw std::invalid_argument(name);
}
std::optional<Json> setting(const CommandLeafServices& services,
  const char* key){
  need(services.shouldAllowManagedHooksOnly,"Managed hook policy import required");
  const auto managed=services.shouldAllowManagedHooksOnly();
  if(managed)need(services.getPolicySettings,"Policy settings import required");
  else need(services.getSettings,"Settings import required");
  const auto settings=managed?services.getPolicySettings():services.getSettings();
  if(!settings||!settings->is_object()||!settings->contains(key))return std::nullopt;
  const auto value=settings->at(key);
  if(value.is_null())return std::nullopt;
  return value;
}
bool commandConfigured(const std::optional<Json>& value){
  return value&&value->is_object()&&value->contains("type")&&
    value->at("type")=="command";
}
std::string commandText(const Json& setting){
  if(!setting.is_object()||!setting.contains("command")||
    !setting.at("command").is_string())return "";
  return setting.at("command").get<std::string>();
}
void log(const CommandLeafServices& services,const std::string& value,
  const std::string& level="info"){
  need(services.logForDebugging,"Hook debug import required");
  services.logForDebugging(value,level);
}
std::string thrown(const CommandLeafServices& services,std::exception_ptr error){
  need(services.stringifyThrown,"Hook thrown-value formatter required");
  return services.stringifyThrown(error);
}
std::vector<std::string> lines(const std::string& input,bool trimWhole){
  const auto& text=trimWhole?value::trim(input):input;
  std::vector<std::string> output;
  std::size_t start=0;
  while(true){
    const auto end=text.find('\n',start);
    const auto line=value::trim(text.substr(start,
      end==std::string::npos?std::string::npos:end-start));
    if(!line.empty())output.push_back(line);
    if(end==std::string::npos)break;
    start=end+1;
  }
  return output;
}
std::string joinLines(const std::vector<std::string>& input){
  std::string output;bool first=true;
  for(const auto& line:input){
    if(!first)output+='\n';first=false;output+=line;
  }
  return output;
}
CommandRequest requestFor(const Json& hook,const std::string& event,
  const std::string& hookName,const Json& input,std::stop_token signal,
  const CommandLeafServices& services){
  CommandRequest request;
  request.hook=hook;request.hookEvent=event;request.hookName=hookName;
  request.jsonInput=value::stringify(input);request.signal=signal;
  need(services.randomUUID,"Hook UUID import required");
  request.uuid=services.randomUUID();
  return request;
}
}
event_loop::Deferred<std::optional<std::string>> executeStatusLineCommand(
  event_loop::Loop& loop,const CommandLeafServices& services,
  const Json& statusLineInput,std::optional<std::stop_token> signal,
  double timeoutMs,bool logResult){
  if(!loop.isOwnerThread())throw std::logic_error("StatusLine owner required");
  event_loop::Deferred<std::optional<std::string>> output(loop);
  try{
    need(services.shouldDisableAllHooksIncludingManaged,
      "Managed hook disable import required");
    if(services.shouldDisableAllHooksIncludingManaged()){
      output.resolve(std::nullopt);return output;
    }
    need(services.shouldSkipHookDueToTrust,"Hook trust import required");
    if(services.shouldSkipHookDueToTrust()){
      log(services,"Skipping StatusLine command execution - workspace trust not accepted");
      output.resolve(std::nullopt);return output;
    }
    const auto hook=setting(services,"statusLine");
    if(!commandConfigured(hook)){output.resolve(std::nullopt);return output;}
    const auto generated=signal?std::shared_ptr<HookAbort>{}:
      HookAbort::create(loop,std::stop_token{},timeoutMs);
    const auto active=signal.value_or(generated->signal());
    try{
      need(services.execCommandHook,"StatusLine command executor required");
      const auto request=requestFor(*hook,"StatusLine","statusLine",
        statusLineInput,active,services);
      services.execCommandHook(request).then([output,services,hook,
        generated,logResult](const CommandOutcome* result,
        const std::exception_ptr& failure){
        if(generated)generated->cleanup();
        try{
          if(failure)std::rethrow_exception(failure);
          if(result->aborted){output.resolve(std::nullopt);return;}
          if(result->status==0){
            const auto text=joinLines(lines(result->stdoutText,true));
            if(!text.empty()){
              if(logResult)log(services,"StatusLine ["+commandText(*hook)+
                "] completed with status "+std::to_string(result->status));
              output.resolve(text);return;
            }
          }else if(logResult){
            log(services,"StatusLine ["+commandText(*hook)+
              "] completed with status "+std::to_string(result->status),"warn");
          }
          output.resolve(std::nullopt);
        }catch(...){
          try{log(services,"Status hook failed: "+
            thrown(services,std::current_exception()),"error");
            output.resolve(std::nullopt);
          }catch(...){output.reject(std::current_exception());}
        }
      });
    }catch(...){
      if(generated)generated->cleanup();
      try{log(services,"Status hook failed: "+
        thrown(services,std::current_exception()),"error");
        output.resolve(std::nullopt);
      }catch(...){output.reject(std::current_exception());}
    }
  }catch(...){output.reject(std::current_exception());}
  return output;
}
event_loop::Deferred<std::vector<std::string>> executeFileSuggestionCommand(
  event_loop::Loop& loop,const CommandLeafServices& services,
  const Json& fileSuggestionInput,std::optional<std::stop_token> signal,
  double timeoutMs){
  if(!loop.isOwnerThread())throw std::logic_error("FileSuggestion owner required");
  event_loop::Deferred<std::vector<std::string>> output(loop);
  try{
    need(services.shouldDisableAllHooksIncludingManaged,
      "Managed hook disable import required");
    if(services.shouldDisableAllHooksIncludingManaged()){
      output.resolve({});return output;
    }
    need(services.shouldSkipHookDueToTrust,"Hook trust import required");
    if(services.shouldSkipHookDueToTrust()){
      log(services,"Skipping FileSuggestion command execution - workspace trust not accepted");
      output.resolve({});return output;
    }
    const auto configured=setting(services,"fileSuggestion");
    if(!commandConfigured(configured)){output.resolve({});return output;}
    const auto generated=signal?std::shared_ptr<HookAbort>{}:
      HookAbort::create(loop,std::stop_token{},timeoutMs);
    const auto active=signal.value_or(generated->signal());
    try{
      need(services.execCommandHook,"FileSuggestion command executor required");
      const Json hook={{"type","command"},{"command",commandText(*configured)}};
      const auto request=requestFor(hook,"FileSuggestion","FileSuggestion",
        fileSuggestionInput,active,services);
      services.execCommandHook(request).then([output,services,generated](
        const CommandOutcome* result,const std::exception_ptr& failure){
        if(generated)generated->cleanup();
        try{
          if(failure)std::rethrow_exception(failure);
          if(result->aborted||result->status!=0){output.resolve({});return;}
          output.resolve(lines(result->stdoutText,false));
        }catch(...){
          try{log(services,"File suggestion helper failed: "+
            thrown(services,std::current_exception()),"error");
            output.resolve({});
          }catch(...){output.reject(std::current_exception());}
        }
      });
    }catch(...){
      if(generated)generated->cleanup();
      try{log(services,"File suggestion helper failed: "+
        thrown(services,std::current_exception()),"error");
        output.resolve({});
      }catch(...){output.reject(std::current_exception());}
    }
  }catch(...){output.reject(std::current_exception());}
  return output;
}
} // namespace batbox::instructions::hooks::runtime
