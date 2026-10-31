#include "fs_operations/FileRanges.hpp"
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
  const auto text=input.value("bytes",std::string{});
  const ByteBuffer bytes(text.begin(),text.end());
  const auto failAt=input.value("failAt",std::string{});
  int readCount=0;
  NodeFsBytesImports imports;imports.loop=&loop;
  imports.toNumber=[](const Json& value){return value.get<double>();};
  imports.allocUnsafe=[](double count){
    if(count<0)throw std::range_error("Invalid buffer size");
    return std::make_shared<ByteBuffer>(static_cast<std::size_t>(count));};
  imports.open=[&](const std::string& file,const std::string& mode){
    calls.push_back(Json::array({"open",file,mode}));
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
      if(failAt=="read"&&readCount++==input.value("failReadIndex",0))
        return failed<Json>(loop,"read failed");
      const auto amount=std::min({length,
        static_cast<std::size_t>(input.value("readChunk",static_cast<int>(length))),
        position<bytes.size()?bytes.size()-position:std::size_t(0)});
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
    if(input.value("method",std::string{})=="range"){
      const auto result=wait(loop,readFileRange(imports,"fixture",
        input.at("offset").get<std::size_t>(),input.at("maxBytes").get<std::size_t>()));
      output["result"]=result?*result:Json(nullptr);
    }else output["result"]=wait(loop,tailFile(imports,"fixture",
      input.at("maxBytes").get<std::size_t>()));
  }catch(const std::exception& error){
    output["status"]="rejected";output["result"]=error.what();}
  output["calls"]=calls;
  std::cout<<value::stringify(output)<<'\n';
}
