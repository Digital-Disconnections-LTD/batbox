#include "keyboard_input/Dispatch.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <unordered_map>
using namespace batbox;
namespace ki=batbox::keyboard_input;
namespace te=batbox::terminal_events;
namespace {
using Listener=te::EventEmitter::Listener;
Json run(const Json& spec){
  event_loop::Loop loop;
  termio::TerminalQuerier querier(loop,[](const std::string&){});
  te::EventEmitter events;
  std::vector<ki::ParsedInput> items;
  std::unordered_map<const Json*,std::string> responses;
  std::unordered_map<const ki::ParsedKey*,std::string> keys;
  std::unordered_map<const ki::ParsedMouse*,std::string> mice;
  for(const auto& entry:spec.at("items")){
    const auto id=entry.at("id").get<std::string>();
    const auto kind=entry.at("kind").get<std::string>();
    if(kind=="response"){
      auto response=std::make_shared<Json>(entry.at("response"));
      responses[response.get()]=id;
      items.push_back(ki::ParsedResponse{entry.value("sequence",""),response});
    }else if(kind=="mouse"){
      auto mouse=std::make_shared<ki::ParsedMouse>();
      mouse->button=entry.value("button",0.0);
      mouse->action=entry.value("action","");
      mouse->col=entry.value("col",0.0);mouse->row=entry.value("row",0.0);
      mouse->sequence=entry.value("sequence","");
      mice[mouse.get()]=id;items.push_back(std::move(mouse));
    }else{
      auto key=std::make_shared<ki::ParsedKey>();
      if(entry.contains("name"))key->name=entry["name"].get<std::string>();
      if(entry.contains("sequence"))key->sequence=entry["sequence"].get<std::string>();
      key->ctrl=entry.value("ctrl",false);
      keys[key.get()]=id;items.push_back(std::move(key));
    }
  }
  Json trace=Json::array();bool focused=spec.value("focused",true);
  const auto fail=spec.value("throwAt",std::string{});
  auto mark=[&](const std::string& value){trace.push_back(value);
    if(value==fail)throw std::runtime_error("failure at "+value);};
  std::string mutate=spec.value("mutateNextOnInput",std::string{});
  events.on("input",std::make_shared<Listener>([&](auto&,const auto& args){
    auto input=std::dynamic_pointer_cast<te::InputEvent>(args.at(0).event);
    mark("emit:input:"+keys.at(input->keypress.get()));
    if(!mutate.empty())for(auto& item:items){
      if(auto key=std::get_if<ki::KeyRef>(&item))
        if(keys.at(key->get())==mutate){(*key)->name="z";(*key)->ctrl=true;}
    }
  }));
  for(const char* name:{"terminalfocus","terminalblur"})
    events.on(name,std::make_shared<Listener>([&,name](auto&,const auto&){
      mark(std::string("emit:")+name);}));
  // A real querier matcher sees the exact response pointer passed by dispatch.
  // Matchers run synchronously in onResponse before deferred resolution.
  for(const auto& item:items)if(const auto response=std::get_if<ki::ParsedResponse>(&item)){
    const auto* expected=response->response.get();
    querier.send({"",[&,expected](const termio::ResponseRef& value){
      if(value.get()!=expected)return false;
      mark("response:"+responses.at(value.get()));return true;}});
  }
  ki::DispatchBindings binding{
    querier,events,
    [&]{mark("interaction");},
    [&](ki::MouseRef mouse){mark("mouse:"+mice.at(mouse.get()));},
    [&](bool value){mark(std::string("focus:")+(value?"true":"false"));focused=value;},
    [&]{return spec.value("dragging",false);},
    [&]{mark("finishSelection");},
    [&]{mark("selectionChange");},
    [&]{mark("getFocus");return focused;},
    [&](bool value){mark(std::string("setFocus:")+(value?"true":"false"));focused=value;},
    spec.value("supportsSuspend",true),
    [&]{mark("suspend");},
    [&](const std::optional<std::string>& input){mark("handleInput:"+
      (input?value::stringify(*input):"undefined"));},
    [&](ki::KeyRef key){mark("dom:"+keys.at(key.get()));}
  };
  std::string error;
  try{ki::processKeysInBatch(binding,items);}catch(const std::exception& e){error=e.what();}
  Json finalItems=Json::array();
  for(const auto& item:items){
    if(const auto key=std::get_if<ki::KeyRef>(&item)){
      Json record={{"id",keys.at(key->get())},{"ctrl",(*key)->ctrl}};
      if((*key)->name)record["name"]=*(*key)->name;
      finalItems.push_back(record);
    }else if(const auto mouse=std::get_if<ki::MouseRef>(&item))
      finalItems.push_back({{"id",mice.at(mouse->get())}});
    else{const auto& response=std::get<ki::ParsedResponse>(item);
      finalItems.push_back({{"id",responses.at(response.response.get())}});}
  }
  return {{"trace",trace},{"error",error.empty()?Json(nullptr):Json(error)},
    {"focused",focused},{"items",finalItems}};
}
}
int main(){const std::string data((std::istreambuf_iterator<char>(std::cin)),{});
  auto cases=value::parse(data);Json result=Json::array();
  for(const auto& item:cases)result.push_back(run(item));
  std::cout<<value::stringify(result)<<'\n';}
