#include "mailbox/Mailbox.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <stdexcept>

using namespace batbox;
namespace mb=batbox::mailbox;
namespace {
mb::MessageRef message(const std::string& id){
  return std::make_shared<mb::Message>(mb::Message{id,"user",id,"now",{}, {}});
}
Json sendPoll(){
  event_loop::Loop loop;mb::Mailbox box(loop);Json trace=Json::array();
  auto listener=std::make_shared<bootstrap::Signal<>::Listener>([&]{
    trace.push_back("notify");});
  box.subscribe(listener);
  box.send(message("a"));box.send(message("b"));
  const auto selected=box.poll([](const auto& value){return value->id=="b";});
  const auto length=box.length(),revision=box.revision();
  const auto remaining=box.poll();
  return Json{{"kind","sendPoll"},{"trace",trace},{"length",length},
    {"revision",revision},{"polled",selected->id},{"remaining",remaining->id}};
}
Json receiveReady(){
  event_loop::Loop loop;mb::Mailbox box(loop);Json trace=Json::array();
  auto listener=std::make_shared<bootstrap::Signal<>::Listener>([&]{
    trace.push_back("notify");});box.subscribe(listener);
  box.send(message("a"));
  auto pending=box.receive();
  pending.then([&](const mb::MessageRef* value,const std::exception_ptr&){
    trace.push_back("then:"+(*value)->id);});
  trace.push_back("sync");loop.runReady();
  return Json{{"kind","receiveReady"},{"trace",trace},
    {"length",box.length()},{"revision",box.revision()}};
}
Json waiters(){
  event_loop::Loop loop;mb::Mailbox box(loop);Json trace=Json::array();
  auto listener=std::make_shared<bootstrap::Signal<>::Listener>([&]{
    trace.push_back("notify");});box.subscribe(listener);
  auto first=box.receive([](const auto& value){return value->id=="a";});
  auto second=box.receive([](const auto& value){return value->id=="b";});
  first.then([&](const mb::MessageRef* value,const std::exception_ptr&){
    trace.push_back("first:"+(*value)->id);});
  second.then([&](const mb::MessageRef* value,const std::exception_ptr&){
    trace.push_back("second:"+(*value)->id);});
  box.send(message("b"));box.send(message("a"));
  trace.push_back("sync");loop.runReady();
  return Json{{"kind","waiters"},{"trace",trace},
    {"length",box.length()},{"revision",box.revision()}};
}
Json reentrantPoll(){
  event_loop::Loop loop;mb::Mailbox box(loop);Json trace=Json::array();
  auto listener=std::make_shared<bootstrap::Signal<>::Listener>([&]{
    trace.push_back("notify");});box.subscribe(listener);
  box.send(message("a"));box.send(message("b"));
  auto selected=box.poll([&](const auto& value){
    if(value->id=="a"){trace.push_back("predicate");box.poll();return true;}
    return false;});
  return Json{{"kind","reentrantPoll"},{"trace",trace},
    {"selected",selected->id},{"length",box.length()},
    {"revision",box.revision()}};
}
Json signalMutation(){
  event_loop::Loop loop;mb::Mailbox box(loop);Json trace=Json::array();
  std::function<void()> cancelSecond=[]{};
  auto first=std::make_shared<bootstrap::Signal<>::Listener>([&]{
    trace.push_back("one");cancelSecond();
    box.subscribe(std::make_shared<bootstrap::Signal<>::Listener>([&]{
      trace.push_back("three");}));});
  box.subscribe(first);
  auto second=std::make_shared<bootstrap::Signal<>::Listener>([&]{
    trace.push_back("two");});
  cancelSecond=box.subscribe(second);
  box.send(message("a"));
  return Json{{"kind","signalMutation"},{"trace",trace},
    {"length",box.length()},{"revision",box.revision()}};
}
Json predicateThrow(){
  event_loop::Loop loop;mb::Mailbox box(loop);Json trace=Json::array();
  auto listener=std::make_shared<bootstrap::Signal<>::Listener>([&]{
    trace.push_back("notify");});box.subscribe(listener);
  (void)box.receive([&](const auto&)->bool{
    trace.push_back("predicate");throw std::runtime_error("predicate failed");});
  std::string error;
  try{box.send(message("a"));}
  catch(const std::exception& failure){error=failure.what();}
  return Json{{"kind","predicateThrow"},{"trace",trace},
    {"error",error},{"length",box.length()},
    {"revision",box.revision()}};
}
}
int main(){
  std::cout<<Json::array({sendPoll(),receiveReady(),waiters(),
    reentrantPoll(),signalMutation(),predicateThrow()}).dump()<<'\n';
}
