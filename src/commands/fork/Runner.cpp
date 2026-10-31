#include "commands/fork/ForkCommand.hpp"
#include <stdexcept>

namespace batbox::commands::fork {
BatRunner bindIndigenousRunner(event_loop::Loop& loop,
  std::shared_ptr<bats::RunBatPreflightServices> preflight,
  std::shared_ptr<bats::RunBatSetupServices> setup,
  std::shared_ptr<bats::RunBatLoopServices> batLoop,
  std::function<void(std::function<void()>)> launch){
  if(!preflight||!setup||!batLoop||!launch)
    throw std::invalid_argument("Native agent runner requires services and launcher");
  const auto dispatcher=loop.dispatcher();
  return [dispatcher,preflight=std::move(preflight),setup=std::move(setup),
    batLoop=std::move(batLoop),launch=std::move(launch)](
    bats::RunBatParams params,BatMessage onMessage){
    event_loop::Deferred<event_loop::Unit> done(dispatcher);
    launch([dispatcher,preflight,setup,batLoop,params=std::move(params),
      onMessage=std::move(onMessage),done]()mutable{
      try{
        const query::Yield yield=query::Yield::withReferences(
          [dispatcher,onMessage](const query::MessageRef& message){
          if(!onMessage)return;
          if(dispatcher.isOwnerThread())onMessage(message);
          else dispatcher.postMission([onMessage,message]{onMessage(message);});
        });
        bats::runBat(params,*preflight,*setup,*batLoop,yield);
        done.resolve(event_loop::Unit{});
      }catch(...){done.reject(std::current_exception());}
    });
    return done;
  };
}
} // namespace batbox::commands::fork
