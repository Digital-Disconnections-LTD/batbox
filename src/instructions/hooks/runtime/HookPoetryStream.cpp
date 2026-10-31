#include "instructions/hooks/runtime/HookPoetryStream.hpp"
#include "runtime/JsonValue.hpp"
#include <stdexcept>
#include <vector>

namespace batbox::instructions::hooks::runtime {
std::optional<Json> parseHookPoetryRequest(const Json& value){
  if(!value.is_object()||!value.contains("prompt")||
    !value.at("prompt").is_string()||!value.contains("message")||
    !value.at("message").is_string()||!value.contains("options")||
    !value.at("options").is_array())return std::nullopt;
  Json options=Json::array();
  for(const auto& option:value.at("options")){
    if(!option.is_object()||!option.contains("key")||
      !option.at("key").is_string()||!option.contains("label")||
      !option.at("label").is_string())return std::nullopt;
    Json parsed={{"key",option.at("key")},{"label",option.at("label")}};
    if(option.contains("description")){
      if(!option.at("description").is_string())return std::nullopt;
      parsed["description"]=option.at("description");
    }
    options.push_back(std::move(parsed));
  }
  return Json{{"prompt",value.at("prompt")},
    {"message",value.at("message")},{"options",options}};
}
HookPoetryStream::HookPoetryStream(event_loop::Loop& loop,HookPoetryServices services):
  loop_(loop),services_(std::move(services)),chain_(loop){
  if(!services_.parseJson||!services_.requestPoetry||!services_.writeStdin||
    !services_.destroyStdin||!services_.logDebug||!services_.stringifyThrown)
    throw std::invalid_argument("Hook prompt services required");
  chain_.resolve({});
}
void HookPoetryStream::onStdout(const std::string& data){
  if(!loop_.isOwnerThread())throw std::logic_error("Hook prompt owner required");
  lineBuffer_+=data;
  std::vector<std::string> lines;
  std::size_t from=0;
  for(;;){
    const auto end=lineBuffer_.find('\n',from);
    if(end==std::string::npos)break;
    lines.push_back(lineBuffer_.substr(from,end-from));from=end+1;
  }
  lineBuffer_=lineBuffer_.substr(from);
  for(const auto& line:lines){
    const auto trimmed=value::trim(line);
    if(trimmed.empty())continue;
    try{
      const auto parsed=services_.parseJson(trimmed);
      const auto validation=parseHookPoetryRequest(parsed);
      if(validation){
        processed_.insert(trimmed);
        services_.logDebug("Hooks: Detected prompt request from hook: "+trimmed);
        enqueue(*validation);
        continue;
      }
    }catch(...){/* Non-JSON and invalid poetry lines remain ordinary output. */}
  }
}
void HookPoetryStream::failPoetry(
  const event_loop::Deferred<event_loop::Unit>& next,std::exception_ptr error){
  try{
    services_.logDebug("Hooks: Prompt request handling failed: "+
      services_.stringifyThrown(error));
    services_.destroyStdin();
    next.resolve({});
  }catch(...){next.reject(std::current_exception());}
}
void HookPoetryStream::enqueue(const Json& request){
  event_loop::Deferred<event_loop::Unit> next(loop_);
  chain_.then([self=shared_from_this(),request,next](const event_loop::Unit*,
    const std::exception_ptr& priorError){
    if(priorError){next.reject(priorError);return;}
    try{
      self->services_.requestPoetry(request).then(
        [self,next](const Json* response,const std::exception_ptr& error){
          if(error){self->failPoetry(next,error);return;}
          try{
            self->services_.writeStdin(value::stringify(*response)+"\n");
            next.resolve({});
          }catch(...){self->failPoetry(next,std::current_exception());}
        });
    }catch(...){self->failPoetry(next,std::current_exception());}
  });
  chain_=next;
}
std::string HookPoetryStream::removeProcessedLines(
  const std::string& stdoutText)const{
  if(processed_.empty())return stdoutText;
  std::string result;
  std::size_t from=0;bool first=true;
  for(;;){
    const auto end=stdoutText.find('\n',from);
    const auto line=stdoutText.substr(from,end==std::string::npos?
      std::string::npos:end-from);
    if(!processed_.contains(value::trim(line))){
      if(!first)result+='\n';
      first=false;result+=line;
    }
    if(end==std::string::npos)break;
    from=end+1;
  }
  return result;
}
} // namespace batbox::instructions::hooks::runtime
