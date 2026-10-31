#include "history/Reader.hpp"
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
  auto shared=std::make_shared<HistoryState>();
  for(const auto& entry:input.value("pending",Json::array()))
    shared->pendingEntries.push_back(std::make_shared<Json>(entry));
  for(const auto& timestamp:input.value("skipped",Json::array()))
    shared->skippedTimestamps.insert(timestamp.get<double>());
  HistoryReaderImports imports;imports.loop=&loop;
  imports.getFlightId=[&]{calls.push_back(Json::array({"getSessionId"}));
    return input.value("session",std::string("session"));};
  imports.getConfigHomeDir=[&]{calls.push_back(Json::array({"getConfigHome"}));
    return std::string("/cfg");};
  imports.join=[&](const std::string& a,const std::string& b){
    calls.push_back(Json::array({"join",a,b}));return a+"/"+b;};
  imports.readLinesReverse=[&](const std::string& path){
    calls.push_back(Json::array({"readLinesReverse",path}));
    const auto lines=input.value("lines",Json::array());
    auto index=std::make_shared<int>(0);
    auto closed=std::make_shared<bool>(false);
    HistoryLineCursor cursor;
    cursor.next=[&,lines,index,closed]{
      if(*index>=static_cast<int>(lines.size())){
        if(!*closed){calls.push_back(Json::array({"closeLines"}));*closed=true;}
        return ready(loop,std::optional<std::string>{});
      }
      calls.push_back(Json::array({"line",*index}));
      if(input.value("failLine",-1)==*index){
        ++*index;
        if(!*closed){calls.push_back(Json::array({"closeLines"}));*closed=true;}
        event_loop::Deferred<std::optional<std::string>> result(loop);
        result.reject(std::make_exception_ptr(std::runtime_error("read failed")));
        return result;
      }
      auto line=lines[(*index)++].get<std::string>();
      return ready(loop,std::optional<std::string>(std::move(line)));
    };
    cursor.close=[&,closed]{
      if(!*closed){calls.push_back(Json::array({"closeLines"}));*closed=true;}
      return ready(loop,event_loop::Unit{});
    };
    return cursor;
  };
  imports.jsonParse=[&](const std::string& line){
    calls.push_back(Json::array({"jsonParse",line}));
    if(line=="BAD")throw std::runtime_error("bad json");
    return value::parse(line);
  };
  imports.logForDebugging=[&](const std::string& message){
    calls.push_back(Json::array({"debug",message}));
    if(input.value("failLog",false))throw std::runtime_error("log failed");};
  imports.getErrnoCode=[&](const std::exception_ptr& error)->std::optional<std::string>{
    std::string code=input.value("failCode",std::string("EIO"));
    try{std::rethrow_exception(error);}catch(const std::exception& value){
      if(std::string(value.what())=="log failed")
        code=input.value("failLogCode",std::string("EIO"));}
    calls.push_back(Json::array({"getErrnoCode",code}));return code;};
  imports.errorToString=[](const std::exception_ptr& error){
    try{std::rethrow_exception(error);}catch(const std::exception& ex){
      return std::string("Error: ")+ex.what();}
  };
  Json result=Json::array();std::string status="fulfilled";
  try{
    HistoryLogReader reader(shared,std::move(imports));bool done=false;
    for(int i=0;i<input.value("take",100);++i){
      calls.push_back(Json::array({"next"}));
      auto item=wait(loop,reader.next());
      if(!item){done=true;break;}
      result.push_back(*item?**item:Json(nullptr));
      if(i==0&&input.contains("replacePending")){
        shared->pendingEntries.clear();
        for(const auto& entry:input["replacePending"])
          shared->pendingEntries.push_back(std::make_shared<Json>(entry));
      }
    }
    if(!done){calls.push_back(Json::array({"return"}));wait(loop,reader.close());}
  }catch(const std::exception& error){status="rejected";
    result=Json::array({error.what()});}
  std::cout<<value::stringify(Json::object({{"status",status},
    {"result",result},{"calls",calls}}))<<'\n';
}
