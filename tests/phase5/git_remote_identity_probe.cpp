#include "git/RemoteIdentity.hpp"
#include "runtime/JsonValue.hpp"
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <stdexcept>

using namespace batbox;
int main(int argc,char** argv){
  if(argc!=2)return 2;std::ifstream stream(argv[1]);
  const std::string data(std::istreambuf_iterator<char>{stream},{});
  Json rows=Json::array();
  for(const auto& item:value::parse(data)){
    event_loop::Loop loop;Json logs=Json::array();
    const auto url=item.at("url").is_null()?std::nullopt:
      std::optional<std::string>(item.at("url").get<std::string>());
    auto hash=git::getRepoRemoteHash(loop,[&]{logs.push_back("getRemoteUrl");
      event_loop::Deferred<std::optional<std::string>> value(loop);
      value.resolve(url);return value;});
    std::optional<std::optional<std::string>> result;
    hash.then([&](const std::optional<std::string>* value,const std::exception_ptr& error){
      if(error)std::rethrow_exception(error);result=*value;});
    loop.runReady();if(!result)throw std::runtime_error("hash pending");
    const auto normalized=url?git::normalizeGitRemoteUrl(*url):std::nullopt;
    rows.push_back({{"name",item.at("name")},
      {"normalized",normalized?Json(*normalized):Json(nullptr)},
      {"hash",*result?Json(**result):Json(nullptr)},{"logs",logs}});
  }
  std::cout<<value::stringify(rows)<<'\n';
}
