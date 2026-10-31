#include "user_input/ImmediateUiBinding.hpp"
#include <cassert>
#include <iostream>

using namespace batbox;
namespace {
Json run(const std::string& caseName){
  event_loop::Loop loop;
  Json trace=Json::array();
  interactive::CommandUi::Callbacks callbacks;
  callbacks.onUiChanged=[&](const auto& view){
    if(view)trace.push_back("view");};
  callbacks.onSettled=[](const auto&,const auto&,bool){};
  callbacks.onError=[](const std::string&){};
  callbacks.onQueueBlocked=[](bool){};
  callbacks.onPoetryFocus=[](bool){};
  auto ui=std::make_shared<interactive::CommandUi>(loop,std::move(callbacks));
  interactive::CommandUi::Done laterDone;
  interactive::CommandUi::Ready laterReady;
  auto abort=std::make_shared<interactive::TurnAbort>();
  bool contextSeen=false;
  auto loader=[&](const user_input::CommandEntry&,const std::string&,
    const interactive::submit::ImmediateContext& context){
    contextSeen=context.messages==Json::array({"previous"})&&
      context.abort==abort&&context.mainLoopModel=="standard";
    return interactive::CommandUi::Invoke([&](auto done,auto ready,auto error){
      interactive::UiRequest view;view.id="view";
      if(caseName=="ready"){
        laterDone=done;
        ready(view,[](const interactive::UiAction&){});
      }else if(caseName=="early"){
        done(interactive::CommandCompletion{std::string("finished"),{}});
        ready(view,[](const interactive::UiAction&){});
      }else if(caseName=="empty")ready(std::nullopt,{});
      else if(caseName=="error")error("failed");
      else if(caseName=="delayed"){
        done(interactive::CommandCompletion{std::string("finished"),{}});
        laterReady=ready;
      }
    });
  };
  auto invoke=user_input::bindImmediateUi(loop,ui,loader);
  user_input::CommandEntry entry;
  entry.descriptor={{"name","config"},{"type","local-jsx"},{"immediate",true}};
  interactive::submit::ImmediateContext context{
    Json::array({"previous"}),abort,"standard"};
  auto pending=invoke(entry,"alpha",std::move(context),
    [&](const interactive::CommandCompletion&){
    trace.push_back("onDone");});
  assert(contextSeen);
  pending.then([&](const event_loop::Unit*,const std::exception_ptr& error){
    if(error){try{std::rethrow_exception(error);}catch(const std::exception& failure){
      trace.push_back(std::string("error:")+failure.what());}}
    else trace.push_back("returned");
  });
  loop.runReady();
  if(caseName=="delayed"){
    trace.push_back(laterReady?"waiting":"missing-call");
    interactive::UiRequest view;view.id="view";
    laterReady(view,[](const interactive::UiAction&){});
    loop.runReady();
  }
  if(caseName=="ready"){
    laterDone(interactive::CommandCompletion{std::string("finished"),{}});
    loop.runReady();
  }
  if(caseName=="empty"&&ui->busy())
    throw std::logic_error("Empty immediate command left UI busy");
  return trace;
}
}
int main(){
  std::string line;
  while(std::getline(std::cin,line)){
    try{std::cout<<value::stringify(run(value::parse(line).get<std::string>()))<<'\n';}
    catch(const std::exception& e){
      std::cout<<value::stringify(Json{{"error",e.what()}})<<'\n';
    }
  }
}
