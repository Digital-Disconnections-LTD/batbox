#include "flights/concurrent/Internal.hpp"
#include <algorithm>

namespace batbox::flights::concurrent {
Deferred<double> Roster::countConcurrentFlights()const{
  auto state=state_;Deferred<double> output(*state->s.loop);
  std::string dir;
  try{dir=state->flightsDir();}catch(...){output.reject(std::current_exception());return output;}
  const auto readFailed=[state,output](const std::exception_ptr& error){
    try{
      if(!state->s.isFsInaccessible(error))state->debugFailure("readdir",error);
      output.resolve(0);
    }catch(...){output.reject(std::current_exception());}
  };
  try{state->s.readdir(dir).then([state,dir,output,readFailed]
      (const std::vector<std::string>* files,const std::exception_ptr& error){
    if(error){readFailed(error);return;}
    try{
      double count=0;
      for(const auto& file:*files){
        if(file.size()<=5||!file.ends_with(".json"))continue;
        const auto end=file.end()-5;
        if(!std::all_of(file.begin(),end,[](unsigned char c){return c>='0'&&c<='9';}))continue;
        const auto pid=value::parseInt10(file.substr(0,file.size()-5));
        if(pid==state->s.pid){++count;continue;}
        if(state->s.isProcessRunning(pid))++count;
        else if(state->s.platform()!="wsl"){
          // Deliberately fire-and-forget. A synchronous import error still
          // rejects this async function, unlike a rejected unlink Promise.
          state->s.unlink(state->s.join(dir,file)).then(
            [](const Unit*,const std::exception_ptr&){});
        }
      }
      output.resolve(count);
    }catch(...){output.reject(std::current_exception());}
  });}catch(...){readFailed(std::current_exception());}
  return output;
}
} // namespace batbox::flights::concurrent
