#include "gadgets/bash/AsyncBashImage.hpp"
#include "runtime/JsonValue.hpp"
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

using namespace batbox;
using namespace batbox::bash_gadget;
int main(int argc,char** argv){
  if(argc!=2)return 2;
  std::ifstream file(argv[1]);
  auto cases=value::parse(std::string(std::istreambuf_iterator<char>{file},{}));
  Json output=Json::array();
  for(const auto& item:cases){
    event_loop::Loop loop;
    Json events=Json::array(),record=Json::object();
    AsyncBashImageOps ops;
    ops.stat=[&](const std::string&){
      events.push_back("stat");event_loop::Deferred<std::uintmax_t> result(loop);
      if(item.value("error",std::string())=="stat")
        result.reject(std::make_exception_ptr(std::runtime_error("stat failed")));
      else result.resolve(item.at("file").get<std::string>().size());
      return result;
    };
    ops.readFile=[&](const std::string&){
      events.push_back("read");event_loop::Deferred<std::string> result(loop);
      if(item.value("error",std::string())=="read")
        result.reject(std::make_exception_ptr(std::runtime_error("read failed")));
      else result.resolve(item.at("file").get<std::string>());
      return result;
    };
    ops.resizeBuffer=[&](const std::string& bytes,const std::string& ext){
      events.push_back("resize:"+std::to_string(bytes.size())+":"+ext);
      event_loop::Deferred<BashImageTransform> result(loop);
      if(item.value("error",std::string())=="resize")
        result.reject(std::make_exception_ptr(std::runtime_error("resize failed")));
      else result.resolve(BashImageTransform{bytes+"!","jpeg"});
      return result;
    };
    std::optional<std::string> path;
    std::optional<std::uintmax_t> size;
    if(item.contains("path"))path=item["path"].get<std::string>();
    if(item.contains("size"))size=item["size"].get<std::uintmax_t>();
    auto result=resizeShellImageOutputAsync(loop,item.at("stdout").get<std::string>(),path,size,ops);
    result.then([&](const std::optional<std::string>* value,const std::exception_ptr& error){
      if(error){try{std::rethrow_exception(error);}catch(const std::exception& e){record["error"]=e.what();}}
      else record["result"]=value->has_value()?Json(**value):Json();
    });
    loop.run();record["events"]=events;output.push_back(record);
  }
  std::cout<<value::stringify(output)<<'\n';
}
