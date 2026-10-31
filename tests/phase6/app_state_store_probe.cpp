#include "app_state/Store.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace batbox;
namespace {
struct Object {std::string id;};
using Ref=std::shared_ptr<Object>;
Ref make(const char* id){return std::make_shared<Object>(Object{id});}
Json row(std::string name,Json trace,const std::string& state){
  return {{"name",std::move(name)},{"trace",std::move(trace)},{"state",state}};
}
std::string format(double value){
  if(std::isnan(value))return "NaN";
  if(value==0&&std::signbit(value))return "-0";
  return value::numberToString(value);
}
}
int main(){
  Json rows=Json::array();
  {
    Json trace=Json::array();auto first=make("first"),sameShape=make("first");
    auto store=app_state::createStore<Ref>(first,[&](const auto& change){
      trace.push_back(Json::array({"change",change.oldState->id,
        change.newState->id}));
    });
    auto listener=std::make_shared<app_state::Store<Ref>::Listener>([&]{
      trace.push_back(Json::array({"listen",store.getState()->id}));
    });
    store.subscribe(listener);store.subscribe(listener);
    store.setState([&](const Ref&){return first;});
    store.setState([&](const Ref&){return sameShape;});
    rows.push_back(row("identity",trace,store.getState()->id));
  }
  {
    Json trace=Json::array();auto store=app_state::createStore<Ref>(make("zero"));
    auto b=std::make_shared<app_state::Store<Ref>::Listener>([&]{
      trace.push_back(Json::array({"B"}));});
    auto c=std::make_shared<app_state::Store<Ref>::Listener>([&]{
      trace.push_back(Json::array({"C"}));});
    std::function<void()> removeB;
    auto a=std::make_shared<app_state::Store<Ref>::Listener>([&]{
      trace.push_back(Json::array({"A"}));removeB();store.subscribe(c);
    });
    auto removeA=store.subscribe(a);removeB=store.subscribe(b);
    store.setState([&](const Ref&){return make("one");});
    store.setState([&](const Ref&){return make("two");});
    removeA();store.setState([&](const Ref&){return make("three");});
    rows.push_back(row("live-set",trace,store.getState()->id));
  }
  {
    Json trace=Json::array();
    std::shared_ptr<app_state::Store<Ref>> store;
    store=std::make_shared<app_state::Store<Ref>>(make("zero"),
      [&](const auto& change){
        trace.push_back(Json::array({"change",change.newState->id}));
        if(change.newState->id=="outer")
          store->setState([&](const Ref&){return make("inner");});
      });
    store->subscribe(std::make_shared<app_state::Store<Ref>::Listener>([&]{
      trace.push_back(Json::array({"listen",store->getState()->id}));}));
    store->setState([&](const Ref&){return make("outer");});
    rows.push_back(row("reentrant",trace,store->getState()->id));
  }
  {
    Json trace=Json::array();
    auto store=app_state::createStore<Ref>(make("zero"),
      [&](const auto& change){
        trace.push_back(Json::array({"change",change.newState->id}));
        if(change.newState->id=="bad")
          throw std::runtime_error("change failed");
      });
    store.subscribe(std::make_shared<app_state::Store<Ref>::Listener>([&]{
      trace.push_back(Json::array({"first"}));
      throw std::runtime_error("listener failed");}));
    store.subscribe(std::make_shared<app_state::Store<Ref>::Listener>([&]{
      trace.push_back(Json::array({"second"}));}));
    try{store.setState([&](const Ref&)->Ref{
      throw std::runtime_error("updater failed");});}
    catch(const std::exception& error){
      trace.push_back(Json::array({"caught",error.what()}));}
    try{store.setState([&](const Ref&){return make("bad");});}
    catch(const std::exception& error){
      trace.push_back(Json::array({"caught",error.what()}));}
    try{store.setState([&](const Ref&){return make("good");});}
    catch(const std::exception& error){
      trace.push_back(Json::array({"caught",error.what()}));}
    rows.push_back(row("exceptions",trace,store.getState()->id));
  }
  {
    Json trace=Json::array();
    auto store=app_state::createStore<double>(
      std::numeric_limits<double>::quiet_NaN(),[&](const auto& change){
        trace.push_back(Json::array({"change",format(change.oldState),
          format(change.newState)}));
      });
    store.subscribe(std::make_shared<app_state::Store<double>::Listener>([&]{
      trace.push_back(Json::array({"listen",format(store.getState())}));
    }));
    store.setState([](double){return std::numeric_limits<double>::quiet_NaN();});
    store.setState([](double){return +0.0;});
    store.setState([](double){return -0.0;});
    store.setState([](double){return -0.0;});
    store.setState([](double){return +0.0;});
    rows.push_back(row("object-is",trace,format(store.getState())));
  }
  std::cout<<rows.dump()<<'\n';
}
