#include "flights/AsyncStash.hpp"

namespace batbox::flights {
event_loop::Deferred<LoadedTranscript> loadFlightFileAsync(event_loop::Loop& loop,
  const std::string& flight,Paths& paths,const AsyncTranscriptLoad& load){
  try{return event_loop::adoptAsyncReturn(load(paths.getFlightProjectTranscriptPath(flight)));}
  catch(...){event_loop::Deferred<LoadedTranscript> result(loop);result.reject(std::current_exception());return result;}
}
event_loop::Deferred<FlightUuidSet> AsyncFlightMessageStash::getFlightMessages(const std::string& flight){
  if(const auto found=entries_.find(flight);found!=entries_.end())return found->second;
  event_loop::Deferred<FlightUuidSet> result(loop_);
  try{
    load_(flight).then([result](const LoadedTranscript* transcript,const std::exception_ptr& error){
      if(error){result.reject(error);return;}
      try{
        auto messages=std::make_shared<runtime::OrderedStringSet>();
        for(const auto& uuid:transcript->messages.keys())messages->insert(uuid);
        result.resolve(std::move(messages));
      }catch(...){result.reject(std::current_exception());}
    });
  }catch(...){result.reject(std::current_exception());}
  // Stash the Promise returned by the async callback, including a rejected
  // Promise. Clearing cannot revoke references already returned.
  entries_.emplace(flight,result);return result;
}
event_loop::Deferred<bool> AsyncFlightMessageStash::doesMessageExistInFlight(
  const std::string& flight,const std::string& uuid){
  event_loop::Deferred<bool> result(loop_);
  try{getFlightMessages(flight).then([result,uuid](const FlightUuidSet* messages,const std::exception_ptr& error){
    if(error)result.reject(error);else result.resolve((*messages)->contains(uuid));
  });}catch(...){result.reject(std::current_exception());}
  return result;
}
} // namespace batbox::flights
