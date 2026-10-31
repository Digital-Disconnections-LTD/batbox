#include "fs_operations/ReverseLines.hpp"
#include "event_loop/Loop.hpp"
#include <algorithm>
#include <cassert>
#include <iostream>
#include <optional>
#include <string>

using namespace batbox;
using namespace batbox::fs_operations;
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
  const auto content=input.value("content",std::string{});
  const ByteBuffer bytes(content.begin(),content.end());
  const auto failAt=input.value("failAt",std::string{});
  NodeFsBytesImports imports;imports.loop=&loop;
  imports.toNumber=[](const Json& value){return value.get<double>();};
  imports.open=[&](const std::string& name,const std::string& mode){
    calls.push_back(Json::array({"open",name,mode}));
    if(failAt=="open")return failed<std::shared_ptr<AsyncByteHandle>>(
      loop,"open failed");
    auto handle=std::make_shared<AsyncByteHandle>();
    handle->stat=[&]{
      calls.push_back(Json::array({"stat"}));
      if(failAt=="stat")return failed<Json>(loop,"stat failed");
      return ready(loop,Json::object({{"size",input.value("sizeOverride",
        static_cast<int>(bytes.size()))}}));};
    handle->read=[&](const std::shared_ptr<ByteBuffer>& buffer,
        std::size_t offset,std::size_t length,std::size_t position){
      calls.push_back(Json::array({"read",offset,length,position}));
      if(failAt=="read")return failed<Json>(loop,"read failed");
      const auto amount=std::min(length,
        position<bytes.size()?bytes.size()-position:std::size_t(0));
      std::copy_n(bytes.begin()+std::min(position,bytes.size()),amount,
        buffer->begin()+offset);
      return ready(loop,Json::object({{"bytesRead",amount}}));};
    handle->close=[&]{
      calls.push_back(Json::array({"close"}));
      if(failAt=="close")return failed<event_loop::Unit>(loop,"close failed");
      return ready(loop,event_loop::Unit{});};
    return ready(loop,handle);
  };
  Json output;output["status"]="fulfilled";
  try{
    ReverseLinesCursor cursor(std::move(imports),"fixture");
    Json lines=Json::array();bool done=false;
    if(input.value("concurrentReturn",false)){
      calls.push_back(Json::array({"next"}));auto first=cursor.next();
      calls.push_back(Json::array({"return"}));auto closing=cursor.close();
      auto item=wait(loop,first);if(item)lines.push_back(*item);
      wait(loop,closing);output["result"]=std::move(lines);
      output["calls"]=calls;
      std::cout<<value::stringify(output)<<'\n';return 0;
    }
    for(int i=0;i<input.value("take",1000);++i){
      calls.push_back(Json::array({"next"}));
      auto item=wait(loop,cursor.next());
      if(!item){done=true;break;}
      lines.push_back(*item);
    }
    if(!done){calls.push_back(Json::array({"return"}));wait(loop,cursor.close());}
    output["result"]=std::move(lines);
  }catch(const std::exception& error){
    output["status"]="rejected";output["result"]=error.what();}
  output["calls"]=calls;
  std::cout<<value::stringify(output)<<'\n';
}
