#include "gadgets/bash/AsyncBashCall.hpp"
#include <future>
#include <utility>

namespace batbox::bash_gadget {
gadgets::Gadget makeAsyncBashGadget(Services services,event_loop::Loop& loop,
  AsyncMissionHost host){
  auto gadget=makeBashGadget(services);
  auto shared=std::make_shared<const Services>(std::move(services));
  auto missionHost=std::make_shared<const AsyncMissionHost>(std::move(host));
  const auto dispatcher=loop.dispatcher();
  gadget.call=[&loop,dispatcher,shared,missionHost](const Json& input,
    const gadgets::GadgetCallContext& context,
    const std::function<std::future<Json>(const Json&)>&,const Json& parent,
    const std::function<void(const gadgets::GadgetProgress&)>& progress){
    auto promise=std::make_shared<std::promise<gadgets::GadgetResult>>();
    auto future=promise->get_future();
    auto begin=[&loop,promise,shared,missionHost,input,context,parent,progress]{
      try{
        callAsyncBash(loop,shared,missionHost,input,context,parent,progress).then(
          [promise](const gadgets::GadgetResult* value,const std::exception_ptr& error){
            if(error)promise->set_exception(error);else promise->set_value(*value);
          });
      }catch(...){promise->set_exception(std::current_exception());}
    };
    try{if(dispatcher.isOwnerThread())begin();else dispatcher.postMission(std::move(begin));}
    catch(...){promise->set_exception(std::current_exception());}
    return future;
  };
  return gadget;
}
} // namespace batbox::bash_gadget
