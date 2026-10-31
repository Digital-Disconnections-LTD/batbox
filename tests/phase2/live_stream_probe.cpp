#include "model/LiveStream.hpp"
#include "model/LiveStreamAdapter.hpp"
#include "transport/Errors.hpp"
#include <chrono>
#include <iostream>

using batbox::Json;

int main(int argc,char** argv) {
  if(argc<4)return 2;
  batbox::transport::ClientConfig config;
  config.baseUrl=argv[1];
  config.messagesPath=argv[2];
  config.requestTimeout=std::chrono::seconds(8);
  config.streamIdleTimeout=std::chrono::seconds(6);
  batbox::transport::Client client(config);
  const std::string mode=argv[3];
  if(mode=="reduce" || mode=="partial") {
    auto opened=batbox::model::openLiveStream(client,Json{{"model","batbox-model"},{"stream",true}},{});
    batbox::streaming::ReducerConfig reducerConfig;
    reducerConfig.requestId=opened.requestId.value_or("");
    reducerConfig.nowMillis=[]{return std::int64_t{100};};
    reducerConfig.randomUUID=[]{return "uuid-1";};
    reducerConfig.timestamp=[]{return "2026-10-28T12:00:00.000Z";};
    reducerConfig.calculateUSDCost=[](const std::string&,const Json&){return 0.0;};
    reducerConfig.addToTotalFlightCost=[](double,const Json&,const std::string&){return 0.0;};
    reducerConfig.getErrorMessageIfRefusal=[](const Json&,const std::string&)->std::optional<Json>{return std::nullopt;};
    reducerConfig.createAssistantAPIErrorMessage=[](const std::string&,const std::string&,const std::string&){return Json::object();};
    batbox::streaming::Reducer reducer(std::move(reducerConfig));
    Json emissions=Json::array();
    auto state=[&]{
      Json messages=Json::array(),blocks=Json::array();
      for(const auto& message:reducer.messages())messages.push_back(*message);
      for(const auto& block:reducer.contentBlocks())blocks.push_back(block?*block:Json(nullptr));
      return Json{{"messages",messages},{"blocks",blocks}};
    };
    try {
      batbox::model::consumeLiveStream(opened,reducer,[&](const auto& item){emissions.push_back(*item);});
      std::cout<<Json{{"requestId",opened.requestId.value_or("")},{"emissions",emissions},{"state",state()}}.dump()<<'\n';
    } catch(const std::exception& error) {
      std::cout<<Json{{"requestId",opened.requestId.value_or("")},{"emissions",emissions},
                      {"state",state()},{"error",error.what()}}.dump()<<'\n';
    }
    return 0;
  }
  batbox::model::LiveStream stream(client,Json{{"model","batbox-model"},{"stream",true}});
  Json events=Json::array();
  Json headers;
  long status=0;
  try {
    const auto response=stream.open();
    headers=response.headers;
    status=response.status;
    while(auto event=stream.next()) {
      events.push_back(*event);
      if(mode=="cancel" && events.size()==1)stream.abort();
    }
    std::cout<<Json{{"status",status},{"headers",headers},{"events",events}}.dump()<<'\n';
  }catch(const batbox::transport::APIError& error){
    Json output={{"events",events},{"errorName",error.kind()},{"errorMessage",error.what()}};
    if(status)output["status"]=status;
    if(!headers.is_null())output["headers"]=headers;
    if(error.status)output["errorStatus"]=*error.status;
    std::cout<<output.dump()<<'\n';
  }catch(const std::exception& error){
    Json output={{"events",events},{"errorName","Error"},{"errorMessage",error.what()}};
    if(status)output["status"]=status;
    if(!headers.is_null())output["headers"]=headers;
    std::cout<<output.dump()<<'\n';
  }
}
