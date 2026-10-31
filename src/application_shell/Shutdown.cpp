#include "application_shell/ApplicationShell.hpp"
#include "application/Async.hpp"
#include <stdexcept>

namespace batbox::application_shell {
event_loop::Deferred<event_loop::Unit> stopAndWait(event_loop::Loop& owner,
    const Host& host){
  if(!owner.isOwnerThread())
    throw std::logic_error("Managed shell shutdown requires owner loop");
  if(!host.shell||!host.poller||!host.missions)
    throw std::invalid_argument("Managed shell host incomplete");
  host.shell->beginShutdown();
  return application::runInWorker<event_loop::Unit>(owner,[host]{
    std::exception_ptr failure;
    try{host.poller->shutdown();}
    catch(...){failure=std::current_exception();}
    for(const auto& mission:host.missions->list()){
      if(mission.type!="local_bash"||mission.status!="running")continue;
      try{(void)host.missions->stop(mission.id);}
      catch(const shell_missions::StopMissionError&){}
      catch(const std::out_of_range&){} // process exited between list and stop
      catch(...){if(!failure)failure=std::current_exception();}
    }
    // A child can be between ManagedShell::spawn and mission registration.
    // beginShutdown closes that insertion window; reap remaining jobs here.
    for(const auto& id:host.shell->activeJobIds()){
      try{
        (void)host.shell->requestKill(id);
        host.shell->waitForShutdown(id);
      }catch(const std::out_of_range&){}
      catch(...){if(!failure)failure=std::current_exception();}
    }
    if(failure)std::rethrow_exception(failure);
    return event_loop::Unit{};
  });
}
} // namespace batbox::application_shell
