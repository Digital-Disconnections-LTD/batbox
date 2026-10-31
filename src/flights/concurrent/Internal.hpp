#pragma once
#include "flights/concurrent/Roster.hpp"

namespace batbox::flights::concurrent {
struct Roster::State:std::enable_shared_from_this<Roster::State> {
  explicit State(Services value):s(std::move(value)){}
  Services s;
  std::string flightsDir()const{return s.join(s.configHome(),"sessions");}
  std::string pidFile()const{return s.join(flightsDir(),value::numberToString(s.pid)+".json");}
  std::optional<std::string> envFlightKind()const;
  Deferred<Unit> updatePidFile(Json patch);
  void debugFailure(const std::string& operation,const std::exception_ptr& error)const{
    s.debug("[concurrentSessions] "+operation+" failed: "+s.errorMessage(error));
  }
};
Json spread(const Json& value);
} // namespace batbox::flights::concurrent
