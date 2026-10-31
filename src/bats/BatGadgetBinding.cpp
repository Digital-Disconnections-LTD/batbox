#include "bats/BatGadgetBinding.hpp"
#include <stdexcept>

namespace batbox::bats {
void bindBatGadgetCall(gadgets::Gadget& gadget,
    std::function<BatContext(const gadgets::GadgetCallContext&)> makeContext,
    std::function<BatGadgetCallServices()> makeServices){
  if(!makeServices)
    throw std::invalid_argument("BatGadget binding requires services");
  gadget.call=[makeContext=std::move(makeContext),makeServices=std::move(makeServices)]
      (const Json& input,const gadgets::GadgetCallContext& callContext,
       const std::function<std::future<Json>(const Json&)>& canUseGadget,
       const Json& assistantMessage,
       const std::function<void(const gadgets::GadgetProgress&)>& onProgress){
    std::promise<gadgets::GadgetResult> result;
    auto future=result.get_future();
    try{
      if(!callContext.batContext && !makeContext)
        throw std::invalid_argument("Unbound BatGadget import: Indigenous BatContext");
      BatContext context=callContext.batContext?*callContext.batContext:makeContext(callContext);
      materializeAppStateModifier(context);
      BatGadgetCallServices services=makeServices();
      services.foreground.progress.onProgress=[onProgress](const Json& progress){
        if(!onProgress)return;
        const Json& id=value::property(progress,"toolUseID");
        onProgress(gadgets::GadgetProgress{id.is_string()?id.get<std::string>():"",
          value::property(progress,"data")});
      };
      const Json output=callBatGadget(input,context,canUseGadget,assistantMessage,
        services);
      gadgets::GadgetResult value;
      value.data=value::property(output,"data");
      result.set_value(std::move(value));
    }catch(...){result.set_exception(std::current_exception());}
    return future;
  };
}
void bindBatGadgetPoetry(gadgets::Gadget& gadget,BatGadgetPoetryDependencies dependencies){
  gadget.poetry=[dependencies=std::move(dependencies)](
      const gadgets::PoetryContext& context){
    return getBatGadgetPoetry(context,dependencies);
  };
}
} // namespace batbox::bats
