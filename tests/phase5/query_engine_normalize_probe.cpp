#include "query_engine/NormalizeBindings.hpp"
#include <iostream>
using namespace batbox;
int main(){std::string line;while(std::getline(std::cin,line)){
  const auto c=Json::parse(line);double now=0;int uid=0;
  auto messages=std::make_shared<messages::Services>();
  messages->randomUUID=[&]{return "generated-"+std::to_string(++uid);};
  messages->nowISO=[]{return "2026-03-31T12:34:56.000Z";};
  auto services=query_engine::makeNormalizeServices(messages,[]{return "session";},
    [&](const std::string& key)->std::optional<std::string>{
      if(key=="BATBOX_CODE_REMOTE"&&c.value("remote",false))return "1";
      if(key=="BATBOX_CODE_CONTAINER_ID"&&c.contains("container"))return c.at("container").get<std::string>();
      return {};
    },[&]{return now;});
  auto state=std::make_shared<query_engine::NormalizeState>();Json out=Json::array();
  for(const auto& op:c.at("ops")){
    now=op.value("now",0.);Json values=Json::array();
    auto cursor=query_engine::normalizeMessage(std::make_shared<Json>(op.at("message")),state,services);
    while(auto value=cursor.next())values.push_back(std::move(*value));
    out.push_back(std::move(values));
  }
  std::cout<<out.dump()<<'\n';
}}
