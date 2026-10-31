#include "query/AsyncEntry.hpp"
#include <iostream>
#include <stdexcept>

using namespace batbox;
int main(int argc,char** argv){
  const std::string scenario=argc>1?argv[1]:"complete";
  event_loop::Loop loop;
  Json trace=Json::array();
  auto event=std::make_shared<Json>(Json{{"type","assistant"},
    {"message",{{"content","hello"}}}});
  std::vector<std::string>* consumed=nullptr;
  int index=0,created=0;
  auto ready=[&](query::AsyncEngineStep step){
    event_loop::Deferred<query::AsyncEngineStep> pending(loop);
    pending.resolve(std::move(step));return pending;
  };
  query::AsyncQueryImports imports;
  imports.queryLoop=[&](query::Params,std::vector<std::string>& uuids){
    trace.push_back("create");++created;consumed=&uuids;
    if(scenario=="create_error")throw std::runtime_error("create failed");
    if(scenario!="empty")uuids={"a",scenario=="duplicates"?"a":"b"};
    return query::AsyncQueryCursor{
      [&]{
        trace.push_back("next");
        if(index++==0)return ready({false,event,{}});
        if(scenario=="next_error")throw std::runtime_error("next failed");
        return ready({true,{},Json{{"reason","complete"}}});
      },
      [&](const Json& value){
        trace.push_back(Json::array({"return",value}));
        return scenario=="return_yield"?ready({false,event,{}}):ready({true,{},value});
      },
      [&](std::exception_ptr error){
        try{std::rethrow_exception(error);}
        catch(const std::exception& ex){trace.push_back(Json::array({"throw",ex.what()}));}
        if(scenario=="throw_recovered")return ready({true,{},Json{{"reason","recovered"}}});
        if(scenario=="throw_yield")return ready({false,event,{}});
        std::rethrow_exception(error);
      }};
  };
  imports.notifyCommandLifecycle=[&](const std::string& uuid,const std::string& state){
    trace.push_back(Json::array({"notify",uuid,state}));
    if(scenario=="notify_error")throw std::runtime_error("notify failed");
    if(scenario=="append_notify"&&uuid=="a")consumed->push_back("c");
  };
  auto iterator=query::makeAsyncQuery(loop,query::Params{},std::move(imports));
  auto observe=[&](event_loop::Deferred<query::AsyncEngineStep> pending){
    pending.then([&](const query::AsyncEngineStep* step,const std::exception_ptr& error){
      if(error){
        try{std::rethrow_exception(error);}
        catch(const std::exception& ex){trace.push_back(Json::array({"error",ex.what()}));}
      }else{
        trace.push_back(Json::array({"result",step->done,
          step->done?step->terminal.value_or(Json()):*step->event}));
        if(!step->done)trace.push_back(Json::array({"identity",step->event==event}));
      }
    });
  };
  const auto error=std::make_exception_ptr(std::runtime_error("caller throw"));
  if(scenario=="return_before_start")observe(iterator.returnEarly("closed"));
  else if(scenario=="throw_before_start")observe(iterator.throwEarly(error));
  else if(scenario=="queued_return"){
    observe(iterator.next());observe(iterator.returnEarly("closed"));
  }else{
    observe(iterator.next());loop.runReady();
    if(scenario.starts_with("return_"))observe(iterator.returnEarly("closed"));
    else if(scenario.starts_with("throw_"))observe(iterator.throwEarly(error));
    else observe(iterator.next());
    loop.runReady();
    if(scenario=="return_yield"||scenario=="throw_yield")observe(iterator.next());
  }
  loop.runReady();observe(iterator.next());loop.runReady();
  std::cout<<Json{{"trace",trace},{"created",created}}.dump()<<'\n';
}
