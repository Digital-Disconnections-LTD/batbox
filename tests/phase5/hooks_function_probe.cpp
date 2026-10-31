#include "instructions/hooks/runtime/FunctionHook.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace batbox;
namespace hook=batbox::instructions::hooks::runtime;
namespace {
struct AbortNamedError:std::runtime_error{using std::runtime_error::runtime_error;};
std::string thrownText(std::exception_ptr error){
  try{std::rethrow_exception(error);}
  catch(const AbortNamedError& value){return "AbortError: "+std::string(value.what());}
  catch(const std::exception& value){return "Error: "+std::string(value.what());}
  catch(const std::string& value){return value;}
  catch(...){return "unknown";}
}
Json run(const Json& item){
  event_loop::Loop loop;std::stop_source parent;Json trace=Json::array(),result=nullptr,error=nullptr;
  if(item.value("preAbort",false))parent.request_stop();
  hook::FunctionHookServices services;
  services.logError=[&](std::exception_ptr failure){
    trace.push_back("log:"+thrownText(failure));};
  services.createAttachmentMessage=[&](const Json& attachment){
    trace.push_back("attachment");return Json{{"attachment",attachment}};};
  services.isErrorInstance=[](std::exception_ptr failure){
    try{std::rethrow_exception(failure);}
    catch(const std::exception&){return true;}
    catch(...){return false;}};
  services.isAbortError=[](std::exception_ptr failure){
    try{std::rethrow_exception(failure);}
    catch(const AbortNamedError&){return true;}
    catch(...){return false;}};
  services.errorMessage=[](std::exception_ptr failure){
    try{std::rethrow_exception(failure);}
    catch(const std::exception& value){return std::string(value.what());}
    catch(...){return std::string{};}};
  hook::FunctionHookHost host(loop,services);
  auto configured=std::make_shared<hook::FunctionHook>();
  configured->fields={{"type","function"},{"errorMessage","blocked by callback"}};
  configured->errorMessage="blocked by callback";
  if(item.contains("hookTimeout"))configured->timeout=item.at("hookTimeout").get<double>();
  configured->callback=[&](const Json&,std::stop_token signal)->hook::FunctionHookReply{
    trace.push_back(std::string("callback:")+(signal.stop_requested()?"true":"false"));
    const auto behavior=item.at("behavior").get<std::string>();
    if(behavior=="true")return true;
    if(behavior=="false")return false;
    if(behavior=="throw-primitive")throw std::string("bad");
    event_loop::Deferred<bool> reply(loop);
    if(behavior=="resolved-false")reply.resolve(false);
    else if(behavior=="reject-error")reply.reject(std::make_exception_ptr(
      std::runtime_error("boom")));
    else if(behavior=="reject-abort")reply.reject(std::make_exception_ptr(
      AbortNamedError("interrupted")));
    else if(behavior=="reject-cancel")reply.reject(std::make_exception_ptr(
      std::runtime_error("Function hook cancelled")));
    return reply;};
  hook::FunctionHookCall call;
  call.hook=configured;call.messages=Json::array({Json{{"type","user"}}});
  call.hookName="Stop:Callback";call.gadgetUseID="tool-1";
  call.hookEvent="Stop";call.timeoutMs=100;call.signal=parent.get_token();
  host.execute(std::move(call)).then([&](const hook::FunctionHookResult* value,
    const std::exception_ptr& failure){
    if(failure){error=thrownText(failure);return;}
    Json blocking=nullptr,message=nullptr;
    if(value->blockingError)blocking={{"blockingError",
      value->blockingError->blockingError},{"command",value->blockingError->command}};
    if(value->message)message=*value->message;
    result={{"outcome",value->outcome},{"blockingError",blocking},
      {"message",message},{"sameHook",value->hook==configured}};
  });
  if(item.value("laterAbort",false))parent.request_stop();
  std::optional<event_loop::Loop::KeepAlive> hold;
  if(item.contains("hookTimeout")){
    hold.emplace(loop.hold());loop.setTimeout(8,[&]{hold->release();});
  }
  loop.run();
  if(host.activeCount()!=0)throw std::runtime_error("Function hook run was retained");
  return {{"name",item.at("name")},{"trace",trace},{"result",result},
    {"error",error}};
}
}
int main(int argc,char** argv){
  if(argc!=2)throw std::invalid_argument("cases path required");
  std::ifstream input(argv[1]);Json cases;input>>cases;
  Json rows=Json::array();for(const auto& item:cases)rows.push_back(run(item));
  std::cout<<rows.dump()<<'\n';
}
