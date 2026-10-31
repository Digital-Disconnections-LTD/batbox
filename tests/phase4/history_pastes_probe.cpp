#include "history/PastedContent.hpp"
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
int main(){
  Json input;std::cin>>input;event_loop::Loop loop;Json calls=Json::array();
  PastedContentImports imports;imports.loop=&loop;
  imports.retrievePastedText=[&](const std::string& hash){
    calls.push_back(Json::array({"retrieve",hash}));
    event_loop::Deferred<std::optional<std::string>> result(loop);
    if(input.value("failHash",std::string{})==hash){
      result.reject(std::make_exception_ptr(std::runtime_error("retrieve failed")));
    }else if(input.contains("hashes")&&input["hashes"].contains(hash)&&
        input["hashes"][hash].is_string())
      result.resolve(input["hashes"][hash].get<std::string>());
    else result.resolve(std::nullopt);
    return result;
  };
  Json output;output["status"]="fulfilled";
  try{
    if(input.at("method")=="resolveStoredPastedContent"){
      auto result=wait(loop,resolveStoredPastedContent(input.at("value"),imports));
      output["result"]=result?*result:Json(nullptr);
    }else output["result"]=wait(loop,logEntryToHistoryEntry(input.at("value"),imports));
  }catch(const std::exception& error){
    output["status"]="rejected";output["result"]=error.what();}
  output["calls"]=calls;
  std::cout<<value::stringify(output)<<'\n';
}
