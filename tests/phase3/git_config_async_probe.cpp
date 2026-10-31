#include "git_filesystem/ConfigParser.hpp"
#include "runtime/JsonValue.hpp"
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
using namespace batbox;
int main(int argc,char** argv){
  if(argc!=2)return 2;std::ifstream file(argv[1]);
  const auto cases=value::parse(std::string(std::istreambuf_iterator<char>{file},{}));
  Json output=Json::array();for(const auto& item:cases){
    event_loop::Loop loop;Json events=Json::array(),record;
    git_filesystem::Services services;
    services.readFile=[&](const std::string& path){
      events.push_back("read:"+path);event_loop::Deferred<std::string> result(loop);
      if(item.contains("error"))result.reject(std::make_exception_ptr(std::runtime_error(
        item["error"].get<std::string>())));
      else result.resolve(item["content"].get<std::string>());
      return result;
    };
    auto result=git_filesystem::parseGitConfigValue(loop,services,item.at("dir"),
      item.value("section",std::string("remote")),std::string("origin"),"url");
    result.then([&](const std::optional<std::string>* value,const std::exception_ptr& error){
      if(error)std::rethrow_exception(error);
      record={{"events",events},{"value",*value?Json(**value):Json()}};
    });
    loop.run();output.push_back(record);
  }
  std::cout<<value::stringify(output)<<'\n';
}
