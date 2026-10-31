#include "interactive/background/FlightBackgrounding.hpp"
#include <iostream>

using namespace batbox;
using namespace batbox::interactive;
using namespace batbox::interactive::background;
struct Fixture {
  event_loop::Loop loop;
  std::vector<std::string> trace;
  std::shared_ptr<TurnAbort> abort=std::make_shared<TurnAbort>();
  std::shared_ptr<app_state::AppStateStore> store;
  std::shared_ptr<FlightBackgrounding> controller;
  Fixture(const Json& initial){
    auto state=std::make_shared<app_state::AppState>();
    state->fields=std::make_shared<value::SharedRecord>(
      value::SharedRecord::fromJson(initial));
    state->missions=std::make_shared<app_state::OrderedStringMap<value::ValueRef>>();
    const auto& missions=value::property(initial,"tasks");
    if(missions.is_object())for(const auto& key:value::objectKeys(missions))
      state->missions->set(key,std::make_shared<Json>(missions.at(key)));
    store=std::make_shared<app_state::AppStateStore>(state);
    FlightBackgroundingServices s;
    s.store=store;
    s.abortForMission=[this](const std::string&,const Json&){return abort;};
    s.setMessages=[this](const TurnMessagesRef& messages){
      trace.push_back("messages:"+std::to_string(messages->size()));
    };
    s.setIsLoading=[this](bool value){trace.push_back(
      std::string("loading:")+(value?"true":"false"));};
    s.resetLoadingState=[this]{trace.push_back("reset");};
    s.setAbortController=[this](std::shared_ptr<TurnAbort> value){
      trace.push_back(std::string("abort:")+(value?"task":"null"));
    };
    s.onBackgroundQuery=[this]{trace.push_back("background-query");};
    controller=std::make_shared<FlightBackgrounding>(loop,std::move(s));
  }
  Json snapshot()const{
    const auto state=store->getState();
    const auto current=state->fieldRef("foregroundedTaskId");
    const auto* mission=state->missions->get("task");
    return Json{{"trace",trace},
      {"foregroundedTaskId",current&&current->is_string()?*current:Json()},
      {"isBackgrounded",mission&&*mission?value::property(**mission,"isBackgrounded"):Json()}};
  }
};
int main(int argc,char** argv){
  if(argc>1&&std::string(argv[1])!="--probe")return 2;
  {
    Fixture f(Json{{"tasks",Json::object()}});
    f.controller->handleBackgroundFlight();
    std::cout<<f.snapshot().dump()<<'\n';
  }
  {
    Fixture f(Json{{"foregroundedTaskId","task"},
      {"tasks",{{"task",{{"type","local_agent"},
        {"status","running"},{"isBackgrounded",false},
        {"messages",Json::array({Json{{"uuid","m1"}}})}}}}}});
    f.controller->handleBackgroundFlight();
    std::cout<<f.snapshot().dump()<<'\n';
  }
  {
    Fixture f(Json{{"foregroundedTaskId","missing"},
      {"tasks",Json::object()}});
    f.controller->handleBackgroundFlight();
    std::cout<<f.snapshot().dump()<<'\n';
  }
  {
    Fixture f(Json{{"foregroundedTaskId","task"},
      {"tasks",{{"task",{{"type","local_agent"},
        {"status","running"},{"isBackgrounded",false},
        {"messages",Json::array({Json{{"uuid","m1"}}})}}}}}});
    f.controller->refresh();f.controller->refresh();
    f.abort->abort("stop");f.controller->refresh();
    std::cout<<f.snapshot().dump()<<'\n';
  }
  {
    Fixture f(Json{{"foregroundedTaskId","task"},
      {"tasks",{{"task",{{"type","local_agent"},
        {"status","completed"},{"isBackgrounded",false},
        {"messages",Json::array({Json{{"uuid","m1"}}})}}}}}});
    f.controller->refresh();
    std::cout<<f.snapshot().dump()<<'\n';
  }
}
