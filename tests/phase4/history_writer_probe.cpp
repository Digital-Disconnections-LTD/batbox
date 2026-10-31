#include "history/Writer.hpp"
#include "event_loop/Loop.hpp"
#include <cassert>
#include <iostream>
#include <optional>

using namespace batbox;
using namespace batbox::history;
template<class T>T wait(event_loop::Loop& loop,event_loop::Deferred<T> promise){
  auto hold=loop.hold();std::optional<T> value;std::exception_ptr error;
  promise.then([&](const T* result,const std::exception_ptr& failure){
    if(failure)error=failure;else value=*result;hold.release();});
  loop.run();if(error)std::rethrow_exception(error);assert(value);return *value;
}
template<class T>event_loop::Deferred<T> ready(event_loop::Loop& loop,T value){
  event_loop::Deferred<T> result(loop);result.resolve(std::move(value));return result;
}
template<class T>event_loop::Deferred<T> failed(event_loop::Loop& loop,
    const std::string& text){
  event_loop::Deferred<T> result(loop);
  result.reject(std::make_exception_ptr(std::runtime_error(text)));return result;
}
int main(){
  Json input;std::cin>>input;event_loop::Loop loop;Json calls=Json::array();
  const auto failAt=input.value("failAt",std::string{});
  auto state=std::make_shared<HistoryState>();
  for(const auto& entry:input.value("pending",Json::array()))
    state->pendingEntries.push_back(std::make_shared<Json>(entry));
  const auto lastMode=input.value("lastMode",std::string{});
  if(!state->pendingEntries.empty()){
    if(lastMode=="fast")state->lastAddedEntry=state->pendingEntries.back();
    else if(lastMode=="slow")state->lastAddedEntry=
      std::make_shared<Json>(*state->pendingEntries.back());
  }
  std::function<event_loop::Deferred<event_loop::Unit>()> cleanup;
  std::optional<event_loop::Deferred<event_loop::Unit>> heldWrite;
  int writeCount=0;
  HistoryWriterServices services;services.loop=&loop;
  services.skipPoetryHistory=[&]{
    calls.push_back(Json::array({"skip",input.value("skip",false)?Json("1"):Json(nullptr)}));
    return input.value("skip",false);};
  services.registerCleanup=[&](auto callback){
    calls.push_back(Json::array({"registerCleanup"}));cleanup=std::move(callback);};
  services.getConfigHomeDir=[&]{calls.push_back(Json::array({"getConfigHome"}));
    return std::string("/cfg");};
  services.join=[&](const std::string& a,const std::string& b){
    calls.push_back(Json::array({"join",a,b}));return a+"/"+b;};
  services.writeFile=[&](const std::string& path,const std::string& text,
      const Json& options){
    calls.push_back(Json::array({"writeFile",path,text,options}));
    if(input.value("holdWrite",false)&&writeCount++==0){
      heldWrite.emplace(loop);return *heldWrite;}
    if(failAt=="writeFile")return failed<event_loop::Unit>(loop,"writeFile failed");
    return ready(loop,event_loop::Unit{});};
  services.lock=[&](const std::string& path,const Json& options){
    calls.push_back(Json::array({"lock",path,options}));
    if(failAt=="lock")return failed<HistoryRelease>(loop,"lock failed");
    HistoryRelease release=[&]{calls.push_back(Json::array({"release"}));
      if(failAt=="release")return failed<event_loop::Unit>(loop,"release failed");
      return ready(loop,event_loop::Unit{});};
    return ready(loop,release);};
  services.appendFile=[&](const std::string& path,const std::string& text,
      const Json& options){
    calls.push_back(Json::array({"appendFile",path,text,options}));
    if(failAt=="appendFile")return failed<event_loop::Unit>(loop,"appendFile failed");
    return ready(loop,event_loop::Unit{});};
  services.jsonStringify=[&](const Json& value){
    calls.push_back(Json::array({"jsonStringify",value::property(value,"display")}));
    if(failAt=="jsonStringify")throw std::runtime_error("jsonStringify failed");
    return value::stringify(value);};
  services.logForDebugging=[&](const std::string& message){
    calls.push_back(Json::array({"debug",message}));
    if(failAt=="debug"||input.value("failDebug",false))
      throw std::runtime_error("debug failed");};
  services.sleep=[&](double ms){calls.push_back(Json::array({"sleep",ms}));
    if(failAt=="sleep"||input.value("failSleep",false))
      return failed<event_loop::Unit>(loop,"sleep failed");
    return ready(loop,event_loop::Unit{});};
  services.hashPastedText=[&](const std::string& value){
    calls.push_back(Json::array({"hash",value::stringLength(value)}));
    return "hash-"+std::to_string(value::stringLength(value));};
  services.storePastedText=[&](const std::string& hash,const std::string& value){
    calls.push_back(Json::array({"store",hash,value::stringLength(value)}));
    return ready(loop,event_loop::Unit{});};
  services.nowMs=[&]{calls.push_back(Json::array({"now"}));return 111.0;};
  services.getProjectRoot=[&]{calls.push_back(Json::array({"project"}));
    return std::string("/p");};
  services.getFlightId=[&]{calls.push_back(Json::array({"session"}));
    return std::string("self");};
  services.errorToString=[](const std::exception_ptr& error){
    try{std::rethrow_exception(error);}catch(const std::exception& value){
      return std::string("Error: ")+value.what();}
  };
  HistoryWriter writer(state,std::move(services));
  std::string status="fulfilled";
  auto snapshotState=[&]{
    Json pending=Json::array();
    for(const auto& entry:state->pendingEntries)pending.push_back(*entry);
    Json skipped=Json::array();
    for(const auto timestamp:state->skippedTimestamps)skipped.push_back(timestamp);
    return Json::object({{"pending",pending},{"writing",state->isWriting},
      {"cleanupRegistered",state->cleanupRegistered},
      {"last",state->lastAddedEntry?
        value::property(*state->lastAddedEntry,"display"):Json(nullptr)},
      {"skipped",skipped}});
  };
  try{
    const auto method=input.at("method").get<std::string>();
    if(method=="immediate")wait(loop,writer.immediateFlushHistory());
    else if(method=="flush")wait(loop,writer.flushPoetryHistory(
      input.value("retries",0)));
    else if(method=="addPrompt"){
      wait(loop,writer.addToPoetryHistory(input.at("command")));
      if(state->currentFlushPromise)wait(loop,*state->currentFlushPromise);
    }else if(method=="addPublic"||method=="cleanup"){
      writer.addToHistory(input.at("command"));
      if(method=="cleanup"&&cleanup)wait(loop,cleanup());
      else if(state->currentFlushPromise)wait(loop,*state->currentFlushPromise);
    }else if(method=="raceRemove"||method=="raceAdd"){
      writer.addToHistory("first");
      const auto firstFlush=*state->currentFlushPromise;
      if(method=="raceRemove")writer.removeLastFromHistory();
      else writer.addToHistory("second");
      heldWrite->resolve({});
      wait(loop,firstFlush);
    }else if(method=="clear")writer.clearPendingHistoryEntries();
    else if(method=="remove")writer.removeLastFromHistory();
  }catch(const std::exception& error){status="rejected";
    calls.push_back(Json::array({"caught",error.what()}));}
  Json output=Json::object({{"status",status},{"calls",calls},
    {"state",snapshotState()}});
  std::cout<<value::stringify(output)<<'\n';
}
