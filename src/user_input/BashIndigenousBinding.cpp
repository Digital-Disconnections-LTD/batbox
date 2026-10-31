#include "user_input/BashIndigenousBinding.hpp"
#include "runtime/AsyncLaunch.hpp"
#include <stdexcept>

namespace batbox::user_input {
BashInputServices bindIndigenousBashInput(event_loop::Loop& loop,
  messages::Services& messages,IndigenousBashInputConfig config){
  BashInputServices services{loop,messages,{},{},{},{},{},{}};
  services.isPowerShellGadgetEnabled=std::move(config.isPowerShellGadgetEnabled);
  services.resolveDefaultShell=std::move(config.resolveDefaultShell);
  services.showProgress=std::move(config.showProgress);
  services.clearProgress=std::move(config.clearProgress);
  auto shared=std::make_shared<IndigenousBashInputConfig>(std::move(config));
  const auto dispatcher=loop.dispatcher();
  services.callShell=[shared,dispatcher](bool usePowerShell,const Json& input,
    const std::function<void(const Json&)>& onProgress){
    event_loop::Deferred<Json> output(dispatcher);
    const auto* gadget=usePowerShell?shared->powerShell:&shared->bash;
    if(!gadget||!gadget->call){
      output.reject(std::make_exception_ptr(std::invalid_argument(
        usePowerShell?"PowerShell tool is not bound":"Bash tool is not bound")));
      return output;
    }
    try{
      auto progress=[dispatcher,onProgress](const gadgets::GadgetProgress& value){
        if(!dispatcher.valid())return;
        dispatcher.postMission([onProgress,data=value.data]{onProgress(data);});
      };
      auto future=gadget->call(input,shared->callContext,{},Json(),progress);
      runtime::contextThread([output,dispatcher,future=std::move(future)]()mutable{
        try{
          auto result=future.get();
          if(dispatcher.valid())output.resolve(Json{{"data",result.data}});
        }catch(...){
          if(dispatcher.valid())output.reject(std::current_exception());
        }
      }).detach();
    }catch(...){output.reject(std::current_exception());}
    return output;
  };
  services.processGadgetResultBlock=[shared,&loop](bool usePowerShell,
    const Json& data,const std::string& id){
    const auto* gadget=usePowerShell?shared->powerShell:&shared->bash;
    if(!gadget||!gadget->mapGadgetResultToGadgetResultBlockParam)
      throw std::invalid_argument("Shell tool result mapper is not bound");
    return gadget_results::processGadgetResultBlock(loop,shared->storage,
      gadget->name,gadget->maxResultSizeChars,
      gadget->mapGadgetResultToGadgetResultBlockParam,data,id,
      shared->localResultOverrides);
  };
  return services;
}
} // namespace batbox::user_input
