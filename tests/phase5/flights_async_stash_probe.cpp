#include "flights/AsyncStash.hpp"
#include <iostream>
using namespace batbox;
using event_loop::Deferred;
Json evaluate(const Json& input){
  event_loop::Loop loop;Json calls=Json::array(),observations=Json::array();
  std::vector<Deferred<flights::LoadedTranscript>> pending;
  flights::Paths paths({[]{return "/config";},[]{return "s";},[]{return "/work";},
    []{return std::optional<std::string>("/project");},[](const auto&){return "/fallback";}});
  auto load=[&](const std::string& path){calls.push_back(path);Deferred<flights::LoadedTranscript> result(loop);pending.push_back(result);return result;};
  flights::AsyncFlightMessageStash stash(loop,[&](const std::string& flight){return flights::loadFlightFileAsync(loop,flight,paths,load);});
  const auto loaded=[](const Json& ids){flights::LoadedTranscript value;for(const auto& id:ids)value.messages.set(id,Json{{"uuid",id}});return value;};
  const auto values=input.at("values");const auto scenario=input.at("case");
  if(scenario=="parallel"){
    auto a=stash.getFlightMessages("s"),b=stash.getFlightMessages("s");observations.push_back({"samePromise",a.samePromise(b)});
    flights::FlightUuidSet first,second;
    a.then([&](const auto* value,const auto& error){if(error)std::rethrow_exception(error);first=*value;});
    b.then([&](const auto* value,const auto& error){if(error)std::rethrow_exception(error);second=*value;});stash.clearFlightMessagesStash();
    auto current=stash.getFlightMessages("s");observations.push_back({"afterClear",a.samePromise(current)});
    flights::FlightUuidSet now;current.then([&](const auto* value,const auto& error){if(error)std::rethrow_exception(error);now=*value;});
    pending[1].resolve(loaded(values));loop.runReady();pending[0].resolve(loaded(Json::array({"old","a"})));loop.runReady();
    observations.push_back({"sameSet",first==second});observations.push_back({"old",Json(*first)});
    observations.push_back({"current",Json(*now)});observations.push_back({"stillCurrent",current.samePromise(stash.getFlightMessages("s"))});
    first->insert("retained");observations.push_back({"isolated",Json(*now)});
  }else if(scenario=="failure"){
    const auto failure=[&](const std::string& which){return [&,which](const auto*,const std::exception_ptr& error){
      try{std::rethrow_exception(error);}catch(const std::exception& e){observations.push_back({which,e.what()});}};};
    auto a=stash.getFlightMessages("s");a.then(failure("first"));pending[0].reject(std::make_exception_ptr(std::runtime_error("")));
    loop.runReady();auto b=stash.getFlightMessages("s");observations.push_back({"cachedFailure",a.samePromise(b)});b.then(failure("second"));
    loop.runReady();stash.clearFlightMessagesStash();auto c=stash.getFlightMessages("s");
    c.then([&](const auto* value,const auto& error){if(error)std::rethrow_exception(error);observations.push_back({"fresh",Json(**value)});});
    pending[1].resolve(loaded(values));loop.runReady();
  }else{
    auto a=stash.getFlightMessages("s");flights::FlightUuidSet set;
    a.then([&](const auto* value,const auto& error){if(error)std::rethrow_exception(error);set=*value;});pending[0].resolve(loaded(values));loop.runReady();
    auto exists=stash.doesMessageExistInFlight("s","late");set->insert("late");
    exists.then([&](const bool* value,const auto& error){if(error)std::rethrow_exception(error);observations.push_back({"exists",*value});observations.push_back({"set",Json(*set)});});loop.runReady();
  }
  return {{"calls",calls},{"observations",observations}};
}
int main(){std::string line;while(std::getline(std::cin,line))std::cout<<value::stringify(evaluate(value::parse(line)))<<'\n';}
