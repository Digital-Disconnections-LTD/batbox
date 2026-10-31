#include "instructions/hooks/runtime/FunctionHook.hpp"
#include <stdexcept>

namespace batbox::instructions::hooks::runtime {
namespace {
template<class T>void require(const std::function<T>& value,const char* text){
  if(!value)throw std::invalid_argument(text);
}
}
struct FunctionHookHost::State {
  event_loop::Loop& loop;
  FunctionHookServices services;
  std::map<std::size_t,std::shared_ptr<Run>> active;
  std::size_t nextId=1;
  State(event_loop::Loop& owner,FunctionHookServices imports):loop(owner),
    services(std::move(imports)){}
};
struct FunctionHookHost::Run:std::enable_shared_from_this<Run>{
  std::weak_ptr<State> host;
  event_loop::Loop& loop;
  FunctionHookCall call;
  FunctionHookServices services;
  event_loop::Deferred<FunctionHookResult> output;
  event_loop::Deferred<bool> passed;
  std::shared_ptr<CombinedAbort> combined;
  std::unique_ptr<std::stop_callback<std::function<void()>>> abortListener;
  std::size_t id;
  bool finished=false;
  Run(const std::shared_ptr<State>& owner,FunctionHookCall input,std::size_t key):
    host(owner),loop(owner->loop),call(std::move(input)),
    services(owner->services),output(loop),passed(loop),id(key){}
  void finish(FunctionHookResult result){
    if(finished)return;
    finished=true;
    if(combined)combined->cleanup();
    abortListener.reset();
    output.resolve(std::move(result));
    if(auto state=host.lock())state->active.erase(id);
  }
  void fail(std::exception_ptr error){
    if(finished)return;
    finished=true;
    if(combined)combined->cleanup();
    abortListener.reset();
    output.reject(std::move(error));
    if(auto state=host.lock())state->active.erase(id);
  }
  void settle(const bool* value,const std::exception_ptr& error){
    if(finished)return;
    try{
      if(error){
        require(services.isErrorInstance,"Function hook error-type import required");
        const bool isError=services.isErrorInstance(error);
        if(isError)require(services.errorMessage,"Function hook error-message import required");
        const auto message=isError?services.errorMessage(error):std::string{};
        if(isError&&message=="Function hook cancelled"){
          finish({"cancelled",call.hook});return;
        }
        if(isError){
          require(services.isAbortError,"Function hook abort-type import required");
          if(services.isAbortError(error)){
            finish({"cancelled",call.hook});return;
          }
        }
        require(services.logError,"Function hook local error logger required");
        services.logError(error);
        const auto content=isError?message:"Function hook execution error";
        const Json attachment={{"type","hook_error_during_execution"},
          {"hookName",call.hookName},{"toolUseID",call.gadgetUseID},
          {"hookEvent",call.hookEvent},{"content",content}};
        require(services.createAttachmentMessage,"Function hook attachment factory required");
        finish({"non_blocking_error",call.hook,std::nullopt,
          services.createAttachmentMessage(attachment)});
        return;
      }
      if(*value){finish({"success",call.hook});return;}
      finish({"blocking",call.hook,
        HookBlockingError{call.hook->errorMessage,"function"}});
    }catch(...){fail(std::current_exception());}
  }
  void start(){
    bool awaiting=false;
    try{
      if(!call.hook)throw std::invalid_argument("Function hook required");
      const auto timeout=call.hook->timeout.value_or(call.timeoutMs);
      combined=CombinedAbort::create(loop,call.signal,std::nullopt,timeout);
      if(combined->aborted()){
        finish({"cancelled",call.hook});return;
      }
      const auto weak=weak_from_this();
      passed.then([weak](const bool* value,const std::exception_ptr& error){
        if(auto current=weak.lock())current->settle(value,error);
      });
      awaiting=true;
      abortListener=std::make_unique<std::stop_callback<std::function<void()>>>(
        combined->signal(),[promise=passed]{
          promise.reject(std::make_exception_ptr(
            std::runtime_error("Function hook cancelled")));
        });
      // Promise.resolve(boolean | Promise<boolean>) queues its reaction even
      // for an immediate boolean. Deferred::then has the same owner turn.
      if(!call.hook->callback)
        throw std::invalid_argument("Function hook callback required");
      auto reply=call.hook->callback(call.messages,combined->signal());
      event_loop::Deferred<bool> callbackResult(loop);
      if(std::holds_alternative<bool>(reply)){
        callbackResult.resolve(std::get<bool>(reply));
      }else callbackResult=std::get<event_loop::Deferred<bool>>(std::move(reply));
      callbackResult.then([weak](const bool* value,const std::exception_ptr& error){
        if(auto current=weak.lock()){
          current->abortListener.reset();
          if(error)current->passed.reject(error);
          else current->passed.resolve(*value);
        }
      });
    }catch(...){
      // A synchronous callback throw rejects the Promise constructor's
      // promise; the await continuation handles it on a microtask.
      if(awaiting)passed.reject(std::current_exception());
      else fail(std::current_exception());
    }
  }
};
FunctionHookHost::FunctionHookHost(event_loop::Loop& loop,
  FunctionHookServices services):state_(std::make_shared<State>(loop,
    std::move(services))){}
event_loop::Deferred<FunctionHookResult> FunctionHookHost::execute(
  FunctionHookCall call){
  auto& state=*state_;
  if(!state.loop.isOwnerThread())
    throw std::logic_error("Function hook owner required");
  const auto id=state.nextId++;
  auto run=std::make_shared<Run>(state_,std::move(call),id);
  state.active.emplace(id,run);
  auto result=run->output;
  run->start();
  return result;
}
std::size_t FunctionHookHost::activeCount()const{
  if(!state_->loop.isOwnerThread())
    throw std::logic_error("Function hook owner required");
  return state_->active.size();
}
} // namespace batbox::instructions::hooks::runtime
