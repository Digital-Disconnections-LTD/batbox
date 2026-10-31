#include "interactive/MessageSynthetic.hpp"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
using batbox::Json;
namespace ix=batbox::interactive;
int main(int argc,char**argv){if(argc!=3||std::string(argv[1])!="--cases")return 2;
 std::ifstream input(argv[2]);const Json cases=Json::parse(input);Json rows=Json::array();
 for(const auto&spec:cases){std::size_t uuidIndex=0;batbox::messages::Services services;
  services.randomUUID=[&]{std::ostringstream o;o<<"00000000-0000-4000-8000-"<<std::hex<<std::setfill('0')<<std::setw(12)<<++uuidIndex;return o.str();};
  services.nowISO=[]{return "2026-03-31T12:34:56.000Z";};
  ix::MessageRefs normalized;for(const auto&m:spec.at("normalized"))normalized.push_back(std::make_shared<Json>(m));
  std::vector<ix::StreamingGadgetUse> streaming;
  for(const auto&gadget:spec.at("streaming"))streaming.push_back({nullptr,gadget.at("contentBlock"),""});
  ix::GadgetIDs inProgress;for(const auto&id:spec.at("inProgress"))inProgress.insert(id.get<std::string>());
  const auto result=ix::syntheticStreamingGadgetUseMessages(streaming,inProgress,normalized,services);
  Json values=Json::array();for(const auto&m:result)values.push_back(*m);
  rows.push_back(Json{{"name",spec.at("name")},{"result",values},{"uuidCalls",uuidIndex}});
 }
 std::cout<<rows.dump()<<'\n';
}
