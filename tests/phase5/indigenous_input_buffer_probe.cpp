#include "interactive/text_input/InputBuffer.hpp"
#include "interactive/text_input/InputModes.hpp"
#include <algorithm>
#include <iostream>
#include <map>
using namespace batbox;
using namespace batbox::interactive::text_input;
Json entry(const BufferEntryRef& value){
 if(!value)return nullptr;
 return {{"text",value->text},{"cursorOffset",value->cursorOffset},
   {"pastedContents",*value->pastedContents},{"timestamp",value->timestamp}};
}
int main(){
 for(std::string line;std::getline(std::cin,line);){
  const auto p=Json::parse(line);Json trace=Json::array();double now=1000;int nextId=1;
  struct Timer{double at;std::function<void()> call;};std::map<int,Timer> timers;
  InputBufferServices services;
  services.now=[&]{return now;};services.setTimeout=[&](double delay,std::function<void()> fn){
    const auto id=nextId++;timers.emplace(id,Timer{now+delay,std::move(fn)});
    trace.push_back(Json::array({"timer",id,delay}));
    return [&,id]{timers.erase(id);trace.push_back(Json::array({"cancel",id}));};};
  InputBuffer buffer(p.value("max",3),p.value("debounce",100.0),services);
  auto pastes=std::make_shared<Json>(Json{{"1",{{"content","image"}}}});
  for(const auto& step:p.at("steps")){
    const auto op=step.at("op").get<std::string>();
    if(op=="push"){
      const auto text=step.at("text").get<std::string>();
      buffer.push(text,step.value("offset",static_cast<double>(value::stringLength(text))),
        step.value("pastes",false)?pastes:nullptr);
    }else if(op=="undo")trace.push_back(Json::array({"undo",entry(buffer.undo())}));
    else if(op=="clear")buffer.clear();
    else if(op=="pastes")(*pastes)["1"]["content"]=step.at("value");
    else if(op=="advance"){
      now+=step.at("ms").get<double>();
      for(;;){auto found=std::find_if(timers.begin(),timers.end(),[&](const auto& item){return item.second.at<=now;});
        if(found==timers.end())break;
        auto fn=found->second.call;timers.erase(found);fn();
      }
    }
    Json entries=Json::array();for(const auto& value:buffer.entries())entries.push_back(entry(value));
    trace.push_back(Json::array({"state",buffer.index(),buffer.canUndo(),entries}));
  }
  Json modes=Json::array();
  for(const std::string input:{"","!","!echo hi","hello","!!","é"})
    modes.push_back(Json::array({getModeFromInput(input),getValueFromInput(input),
      isInputModeCharacter(input),prependModeCharacterToInput(input,"bash"),prependModeCharacterToInput(input,"prompt")}));
  std::cout<<Json{{"trace",trace},{"modes",modes}}.dump()<<'\n';
 }
}
