#include "history/Views.hpp"
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
int main(){
  Json input;std::cin>>input;event_loop::Loop loop;Json calls=Json::array();
  HistoryViewImports imports;imports.loop=&loop;
  imports.getProjectRoot=[&]{calls.push_back(Json::array({"project"}));
    return input.value("project",std::string("/p"));};
  imports.getFlightId=[&]{calls.push_back(Json::array({"session"}));
    return input.value("session",std::string("self"));};
  imports.makeLogEntryReader=[&]{
    auto entries=input.value("entries",Json::array());
    auto index=std::make_shared<std::size_t>(0);
    auto started=std::make_shared<bool>(false);
    auto closed=std::make_shared<bool>(false);
    LogEntryCursor cursor;
    cursor.next=[&,entries,index,started,closed]{
      if(!*started){calls.push_back(Json::array({"readerStart"}));*started=true;}
      if(*index>=entries.size()){
        if(!*closed){calls.push_back(Json::array({"readerClose"}));*closed=true;}
        return ready(loop,std::optional<LogEntryRef>{});
      }
      calls.push_back(Json::array({"entry",*index}));
      auto value=std::make_shared<Json>(entries[(*index)++]);
      return ready(loop,std::optional<LogEntryRef>(std::move(value)));
    };
    cursor.close=[&,started,closed]{
      if(*started&&!*closed){calls.push_back(Json::array({"readerClose"}));*closed=true;}
      return ready(loop,event_loop::Unit{});
    };
    return cursor;
  };
  imports.logEntryToHistoryEntry=[&](const LogEntryRef& entry){
    const auto display=value::property(*entry,"display");
    calls.push_back(Json::array({"resolve",display}));
    event_loop::Deferred<Json> result(loop);
    if(input.contains("failResolve")&&display==input["failResolve"]){
      result.reject(std::make_exception_ptr(std::runtime_error("resolve failed")));
    }else result.resolve(Json::object({{"display",display},
      {"pastedContents",entry->value("pastedContents",Json::object())}}));
    return result;
  };
  Json output;output["status"]="fulfilled";Json results=Json::array();
  try{
    const auto method=input.at("method").get<std::string>();
    const auto limit=input.value("take",200);
    if(method=="makeHistoryReader"){
      HistorySimpleCursor cursor(imports);bool done=false;
      for(int i=0;i<limit;++i){
        calls.push_back(Json::array({"next"}));auto item=wait(loop,cursor.next());
        if(!item){done=true;break;}results.push_back(*item);
      }
      if(!done){calls.push_back(Json::array({"return"}));wait(loop,cursor.close());}
    }else if(method=="getTimestampedHistory"){
      TimestampedHistoryCursor cursor(imports);bool done=false;
      for(int i=0;i<limit;++i){
        calls.push_back(Json::array({"next"}));auto item=wait(loop,cursor.next());
        if(!item){done=true;break;}
        results.push_back(Json::object({{"display",item->display},
          {"timestamp",item->timestamp}}));
        if(input.value("resolveFirst",false)&&i==0){
          calls.push_back(Json::array({"invokeResolve"}));wait(loop,item->resolve());}
      }
      if(!done){calls.push_back(Json::array({"return"}));wait(loop,cursor.close());}
    }else{
      GroupedHistoryCursor cursor(imports);bool done=false;
      for(int i=0;i<limit;++i){
        calls.push_back(Json::array({"next"}));auto item=wait(loop,cursor.next());
        if(!item){done=true;break;}results.push_back(*item);
      }
      if(!done){calls.push_back(Json::array({"return"}));wait(loop,cursor.close());}
    }
  }catch(const std::exception& error){output["status"]="rejected";
    results=Json::array({error.what()});}
  output["result"]=std::move(results);output["calls"]=calls;
  std::cout<<value::stringify(output)<<'\n';
}
