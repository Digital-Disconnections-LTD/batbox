#include "fs_operations/NodeFsBytes.hpp"
#include "event_loop/Loop.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
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
template<class T>event_loop::Deferred<T> failure(event_loop::Loop& loop,
    const std::string& message){
  event_loop::Deferred<T> result(loop);
  result.reject(std::make_exception_ptr(std::runtime_error(message)));return result;
}
int main(){
  Json input;std::cin>>input;
  event_loop::Loop loop;Json calls=Json::array();
  const auto source=input.value("bytes",std::string{});
  const ByteBuffer bytes(source.begin(),source.end());
  const auto failAt=input.value("failAt",std::string{});
  int readCount=0;
  NodeFsBytesImports imports;imports.loop=&loop;
  imports.toNumber=[](const Json& value){
    if(value.is_null())return 0.0;
    if(value.is_boolean())return value.get<bool>()?1.0:0.0;
    if(value.is_string())return std::stod(value.get<std::string>());
    return value.get<double>();};
  imports.allocUnsafe=[](double count){
    if(count<0||!std::isfinite(count))throw std::range_error("Invalid buffer size");
    return std::make_shared<ByteBuffer>(static_cast<std::size_t>(count));};
  imports.readFile=[&](const std::string&){
    calls.push_back(Json::array({"readFile"}));
    if(failAt=="readFile")return failure<ByteBuffer>(loop,"readFile failed");
    return ready(loop,bytes);
  };
  imports.open=[&](const std::string& path,const std::string& mode){
    calls.push_back(Json::array({"open",path,mode}));
    if(failAt=="open")return failure<std::shared_ptr<AsyncByteHandle>>(
      loop,"open failed");
    auto handle=std::make_shared<AsyncByteHandle>();
    handle->stat=[&]{
      calls.push_back(Json::array({"stat"}));
      if(failAt=="stat")return failure<Json>(loop,"stat failed");
      return ready(loop,Json::object({{"size",input.value("sizeOverride",
        static_cast<int>(bytes.size()))}}));
    };
    handle->read=[&](const std::shared_ptr<ByteBuffer>& buffer,
        std::size_t offset,std::size_t length,std::size_t position){
      calls.push_back(Json::array({"read",offset,length,position}));
      if(failAt=="read"&&readCount++==input.value("failReadIndex",0))
        return failure<Json>(loop,"read failed");
      const auto amount=std::min({length,
        static_cast<std::size_t>(input.value("readChunk",static_cast<int>(length))),
        position<bytes.size()?bytes.size()-position:std::size_t(0)});
      std::copy_n(bytes.begin()+std::min(position,bytes.size()),amount,
        buffer->begin()+offset);
      return ready(loop,Json::object({{"bytesRead",amount}}));
    };
    handle->close=[&]{
      calls.push_back(Json::array({"close"}));
      if(failAt=="close"||input.value("failClose",false))
        return failure<event_loop::Unit>(loop,"close failed");
      return ready(loop,event_loop::Unit{});
    };
    return ready(loop,handle);
  };
  Json output;output["status"]="fulfilled";
  try{
    NodeFsBytes wrapper(std::move(imports));
    auto max=input.value("hasMax",false)?std::optional<Json>(input.at("maxBytes")):
      std::nullopt;
    const auto result=wait(loop,wrapper.readFileBytes("fixture",max));
    output["result"]=Json::array();
    for(const auto value:result)output["result"].push_back(value);
  }catch(const std::exception& error){
    output["status"]="rejected";output["result"]=error.what();
  }
  output["calls"]=calls;
  std::cout<<value::stringify(output)<<'\n';
}
