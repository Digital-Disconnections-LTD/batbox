#pragma once
#include "event_loop/Deferred.hpp"
#include "flights/Stash.hpp"

namespace batbox::flights {
using AsyncTranscriptLoad=std::function<event_loop::Deferred<LoadedTranscript>(const std::string&)>;
using FlightUuidSet=std::shared_ptr<runtime::OrderedStringSet>;
event_loop::Deferred<LoadedTranscript> loadFlightFileAsync(event_loop::Loop&,
  const std::string& flight,Paths&,const AsyncTranscriptLoad& loadTranscriptFile);

class AsyncFlightMessageStash {
  event_loop::Loop& loop_;
  AsyncTranscriptLoad load_;
  std::unordered_map<std::string,event_loop::Deferred<FlightUuidSet>> entries_;
public:
  AsyncFlightMessageStash(event_loop::Loop& loop,AsyncTranscriptLoad load):loop_(loop),load_(std::move(load)){}
  event_loop::Deferred<FlightUuidSet> getFlightMessages(const std::string& flight);
  void clearFlightMessagesStash(){entries_.clear();}
  event_loop::Deferred<bool> doesMessageExistInFlight(const std::string& flight,const std::string& uuid);
};
} // namespace batbox::flights
