#include "terminal_events/Emitter.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <stdexcept>

using namespace batbox;
namespace te=batbox::terminal_events;
namespace {
using Listener=te::EventEmitter::Listener;
using Ref=te::EventEmitter::ListenerRef;
Ref make(Listener callback){return std::make_shared<Listener>(std::move(callback));}
te::EventArg event(){return te::EventArg::fromEvent(std::make_shared<te::Event>());}
Json empty(){
  te::EventEmitter emitter;
  return Json{{"kind","empty"},{"result",emitter.emit("input")},
    {"max",emitter.maxListeners()}};
}
Json stop(){
  te::EventEmitter emitter;Json trace=Json::array();
  emitter.on("input",make([&](te::EventEmitter& self,const auto& args){
    trace.push_back(std::string("one:")+(&self==&emitter?"true":"false"));
    args[0].event->stopImmediatePropagation();}));
  emitter.on("input",make([&](auto&,const auto&){trace.push_back("two");}));
  const bool result=emitter.emit("input",{event()});
  return Json{{"kind","stop"},{"trace",trace},{"result",result},
    {"count",emitter.rawListenerCount("input")}};
}
Json ordinary(){
  te::EventEmitter emitter;Json trace=Json::array();
  emitter.on("input",make([&](auto&,const auto&){trace.push_back("one");}));
  emitter.on("input",make([&](auto&,const auto&){trace.push_back("two");}));
  const bool result=emitter.emit("input",{
    te::EventArg::ordinary(std::string("ordinary"))});
  return Json{{"kind","ordinary"},{"trace",trace},{"result",result}};
}
Json once(){
  te::EventEmitter emitter;Json trace=Json::array();
  auto listener=make([&](auto&,const auto&){trace.push_back("once");});
  emitter.once("input",listener);
  const auto before=emitter.rawListenerCount("input");
  emitter.emit("input",{event()});emitter.emit("input",{event()});
  return Json{{"kind","once"},{"trace",trace},{"before",before},
    {"after",emitter.rawListenerCount("input")}};
}
Json snapshotRemoval(){
  te::EventEmitter emitter;Json trace=Json::array();
  auto second=make([&](auto&,const auto&){trace.push_back("two");});
  emitter.on("input",make([&](auto&,const auto&){
    trace.push_back("one");emitter.removeListener("input",second);}));
  emitter.on("input",second);
  emitter.emit("input",{event()});
  return Json{{"kind","snapshotRemoval"},{"trace",trace},
    {"count",emitter.rawListenerCount("input")}};
}
Json errorEvent(){
  te::EventEmitter emitter;Json trace=Json::array();
  emitter.on("error",make([&](auto&,const auto& args){
    trace.push_back("one");args[0].event->stopImmediatePropagation();}));
  emitter.on("error",make([&](auto&,const auto&){trace.push_back("two");}));
  const bool result=emitter.emit("error",{event()});
  return Json{{"kind","errorEvent"},{"trace",trace},{"result",result}};
}
Json errorUnhandled(){
  te::EventEmitter emitter;std::string error;
  try{emitter.emit("error",{
    te::EventArg::ordinary(std::make_exception_ptr(
      std::runtime_error("boom")))});}
  catch(const std::exception& failure){error=failure.what();}
  return Json{{"kind","errorUnhandled"},{"error",error}};
}
Json listenerThrow(){
  te::EventEmitter emitter;Json trace=Json::array();
  emitter.on("input",make([&](auto&,const auto&){
    trace.push_back("one");throw std::runtime_error("bad listener");}));
  emitter.on("input",make([&](auto&,const auto&){trace.push_back("two");}));
  std::string error;
  try{emitter.emit("input",{event()});}
  catch(const std::exception& failure){error=failure.what();}
  return Json{{"kind","listenerThrow"},{"trace",trace},{"error",error}};
}
Json symbol(){
  te::EventEmitter emitter;Json trace=Json::array();
  auto symbol=std::make_shared<te::EventSymbol>();
  emitter.on(symbol,make([&](auto&,const auto&){trace.push_back("symbol");}));
  const bool result=emitter.emit(symbol);
  return Json{{"kind","symbol"},{"trace",trace},{"result",result},
    {"count",emitter.rawListenerCount(symbol)}};
}
}
int main(){
  std::cout<<Json::array({empty(),stop(),ordinary(),once(),snapshotRemoval(),
    errorEvent(),errorUnhandled(),listenerThrow(),symbol()}).dump()<<'\n';
}
