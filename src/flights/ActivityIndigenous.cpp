#include "flights/Activity.hpp"
#include "cleanup/Signals.hpp"
#include "utils/envUtils.hpp"
#include <chrono>
#include <cstdlib>

namespace batbox::flights {
ActivityServices indigenousActivityServices(event_loop::Loop& loop,cleanup::Roster& roster,
  std::function<void(const std::string&,const std::string&,Json)> diagnostic){
  ActivityServices s;
  s.now=[]{return std::chrono::duration<double,std::milli>(
    std::chrono::system_clock::now().time_since_epoch()).count();};
  s.sendKeepalives=[]{const auto* value=std::getenv("BATBOX_REMOTE_SEND_KEEPALIVES");
    return isEnvTruthy(value?Json(value):Json());};
  s.setInterval=[&loop](double delay,std::function<void()> callback){
    return cleanup::indigenousUnrefInterval(loop,delay,std::move(callback));
  };
  s.setTimeout=[&loop](double delay,std::function<void()> callback){
    const auto timer=loop.setTimeout(delay,std::move(callback));
    return cleanup::ShutdownTimer{[timer]{timer.cancel();},[timer]{timer.unref();}};
  };
  s.registerCleanup=[&loop,&roster](std::function<void()> callback){
    roster.registerCleanup([&loop,callback=std::move(callback)]{
      event_loop::Deferred<event_loop::Unit> result(loop);
      try{callback();result.resolve(event_loop::Unit{});}
      catch(...){result.reject(std::current_exception());}
      return result;
    });
  };
  s.diagnostic=std::move(diagnostic);return s;
}
} // namespace batbox::flights
