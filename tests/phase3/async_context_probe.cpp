#include "runtime/AsyncLocalStorage.hpp"
#include "event_loop/Deferred.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
using namespace batbox;
struct Data {std::string name;int count=0;};
using Store=runtime::AsyncLocalStorage<Data>;
using DataRef=std::shared_ptr<Data>;
DataRef data(std::string name){return std::make_shared<Data>(Data{std::move(name),0});}
Json name(const Store& store){const auto value=store.getStore();return value?Json(value->name):Json(nullptr);}
int main(){Json all=Json::array();
 {Store a;const auto one=data("one"),two=data("two");Json trace=Json::array();auto value=data("result");const auto returned=a.run(one,[&]{trace.push_back(name(a));try{a.run(two,[&]{trace.push_back(name(a));throw std::runtime_error("stop");});}catch(const std::exception& e){trace.push_back(e.what());}trace.push_back(name(a));return value;});all.push_back(Json{{"kind","nested"},{"trace",trace},{"same",returned==value},{"outside",name(a)}});}
 {Store a;event_loop::Loop loop;event_loop::Deferred<event_loop::Unit> p(loop);Json trace=Json::array();a.run(data("A"),[&]{p.then([&](const auto*,const auto&){trace.push_back(name(a));});});a.run(data("B"),[&]{p.then([&](const auto*,const auto&){trace.push_back(name(a));});});a.run(data("resolver"),[&]{p.resolve({});});trace.push_back("sync");loop.runReady();all.push_back(Json{{"kind","registration"},{"trace",trace},{"outside",name(a)}});}
 {Store a;event_loop::Loop loop;event_loop::Deferred<event_loop::Unit> p(loop);p.resolve({});Json trace=Json::array();a.run(data("late"),[&]{p.then([&](const auto*,const auto&){trace.push_back(name(a));});});p.then([&](const auto*,const auto&){trace.push_back(name(a));});loop.runReady();all.push_back(Json{{"kind","settled"},{"trace",trace}});}
 {Store a,b;event_loop::Loop loop;Json rows=Json::object();a.run(data("A"),[&]{loop.queueMicrotask([&]{rows["micro"]=name(a);loop.queueMicrotask([&]{rows["nested"]=name(a);});});loop.postMission([&]{rows["task"]=name(a);});b.run(data("B"),[&]{loop.setTimeout(1,[&]{rows["timer"]=Json::array({name(a),name(b)});});});});loop.run();all.push_back(Json{{"kind","queues"},{"rows",rows}});}
 {Store a;const auto fn=a.run(data("A"),[&]{return [&]{return name(a);};});all.push_back(Json{{"kind","returnedFunction"},{"value",fn()}});}
 {Store a;event_loop::Loop loop;const auto value=data("A");event_loop::Deferred<event_loop::Unit> p(loop);bool same=false;int count=0;a.run(value,[&]{p.then([&](const auto*,const auto&){same=a.getStore()==value;count=++a.getStore()->count;});});value->count=4;p.resolve({});loop.runReady();all.push_back(Json{{"kind","identity"},{"same",same},{"count",count},{"original",value->count}});}
 {Store a;event_loop::Loop loop;Json result=Json::object();for(const auto* label:{"A","B","C","D"}){const std::string id=label;a.run(data(id),[&,id]{loop.queueMicrotask([&,id]{event_loop::Deferred<event_loop::Unit> p(loop);p.resolve({});p.then([&,id](const auto*,const auto&){loop.setTimeout(1,[&,id]{result[id]=name(a);});});});});}loop.run();all.push_back(Json{{"kind","fourAgents"},{"result",result}});}
 std::cout<<value::stringify(all)<<'\n';}
