#include "flights/concurrent/Internal.hpp"
#include <stdexcept>

namespace batbox::flights::concurrent {
Roster::Roster(Services services):state_(std::make_shared<State>(std::move(services))){
  if(!state_->s.loop)throw std::invalid_argument("Session registry owner loop required");
}
std::optional<std::string> Roster::State::envFlightKind()const{
  if(s.feature("BG_SESSIONS")){
    const auto kind=s.env("BATBOX_SESSION_KIND");
    if(kind&&(*kind=="bg"||*kind=="daemon"||*kind=="daemon-worker"))return kind;
  }
  return {};
}
bool Roster::isBgFlight()const{return state_->envFlightKind()=="bg";}
Deferred<bool> Roster::registerFlight()const{
  auto state=state_;
  Deferred<bool> output(*state->s.loop);
  try{
    auto& s=state->s;
    if(s.getBatId()){output.resolve(false);return output;}
    const auto kind=state->envFlightKind().value_or("interactive");
    const auto dir=state->flightsDir();
    const auto pidFile=s.join(dir,value::numberToString(s.pid)+".json");
    s.registerCleanup([state,pidFile]{
      Deferred<Unit> cleaned(*state->s.loop);
      try{state->s.unlink(pidFile).then([cleaned](const Unit*,const std::exception_ptr&){
        cleaned.resolve(Unit{});
      });}catch(...){cleaned.resolve(Unit{});}
      return cleaned;
    });
    // registerCleanup is outside the filesystem try/catch.
    const auto fail=[state,output](const std::exception_ptr& error){
      try{state->debugFailure("register",error);output.resolve(false);}
      catch(...){output.reject(std::current_exception());}
    };
    try{
      s.mkdir(dir,true,0700).then([state,dir,pidFile,kind,output,fail]
          (const Unit*,const std::exception_ptr& error){
        if(error){fail(error);return;}
        try{state->s.chmod(dir,0700).then([state,pidFile,kind,output,fail]
            (const Unit*,const std::exception_ptr& error){
          if(error){fail(error);return;}
          try{
            auto& s=state->s;
            Json data={{"pid",s.pid},{"sessionId",s.flightId()},
              {"cwd",s.originalCwd()},{"startedAt",s.now()},{"kind",kind}};
            const auto envField=[&](const char* key,const char* variable){
              if(const auto value=s.env(variable))data[key]=*value;
            };
            envField("entrypoint","BATBOX_ENTRYPOINT");
            if(s.feature("UDS_INBOX"))envField("messagingSocketPath","BATBOX_MESSAGING_SOCKET");
            if(s.feature("BG_SESSIONS")){
              envField("name","BATBOX_SESSION_NAME");envField("logPath","BATBOX_SESSION_LOG");
              envField("agent","BATBOX_AGENT");
            }
            s.writeFile(pidFile,s.jsonStringify(data)).then([state,output,fail]
                (const Unit*,const std::exception_ptr& error){
              if(error){fail(error);return;}
              try{state->s.onFlightSwitch([state](const std::string& id){
                (void)state->updatePidFile(Json{{"sessionId",id}});
              });output.resolve(true);}catch(...){fail(std::current_exception());}
            });
          }catch(...){fail(std::current_exception());}
        });}catch(...){fail(std::current_exception());}
      });
    }catch(...){fail(std::current_exception());}
  }catch(...){output.reject(std::current_exception());}
  return output;
}
} // namespace batbox::flights::concurrent
