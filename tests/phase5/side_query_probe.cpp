#include "side_query/SideQuery.hpp"
#include "model_config/ModelConfig.hpp"
#include <iostream>

using namespace batbox;
Json run(const Json& c){
  event_loop::Loop loop;Json events=Json::array(),last=nullptr;
  double tick=1000;std::stop_source stop;
  if(c.value("abort",false))stop.request_stop();
  auto response=std::make_shared<Json>(Json{{"content",Json::array({
    Json{{"type","text"},{"text","chosen"}}})},
    {"usage",{{"input_tokens",3},{"output_tokens",1}}}});
  const auto fail=[&](const std::string& where){
    if(c.value("fail","")==where)throw std::runtime_error(where+" failed");
  };
  side_query::Services services;
  services.getClient=[&](side_query::ClientOptions opts){
    events.push_back({"client",{{"maxRetries",opts.maxRetries},{"model",opts.model},{"source",opts.source}}});
    event_loop::Deferred<std::shared_ptr<side_query::Client>> result(loop);
    try{
      fail("client");auto client=std::make_shared<side_query::Client>();
      client->create=[&](Json body,std::stop_token signal){
        events.push_back({"create",body,signal==stop.get_token(),signal.stop_requested()});
        event_loop::Deferred<side_query::ResponseRef> out(loop);
        try{fail("create");out.resolve(response);}catch(...){out.reject(std::current_exception());}
        return out;
      };
      result.resolve(client);
    }catch(...){result.reject(std::current_exception());}
    return result;
  };
  services.getModelBetas=[&](const std::string& model){
    events.push_back({"betas",model});return c.value("betas",std::vector<std::string>{});
  };
  services.supportsStructuredOutputs=[&](const std::string& model){
    events.push_back({"structured",model});return c.value("supports",true);
  };
  services.systemPoetryPrefix=[&](const Json& opts){
    events.push_back({"prefix",opts});return "Batbox prefix";
  };
  services.normalizeModel=[&](const std::string& model){
    events.push_back({"normalize",model});fail("normalize");
    return model_config::normalizeModelStringForAPI(model);
  };
  services.nowMillis=[&]{events.push_back({"now",tick});return tick++;};
  services.setLastApiCompletionTimestamp=[&](double value){
    events.push_back({"complete",value});last=value;fail("complete");
  };
  Json result=Json::object();bool settled=false;
  auto pending=side_query::sideQuery(loop,c.at("options"),stop.get_token(),services);
  pending.then([&](const side_query::ResponseRef* value,const std::exception_ptr& error){
    if(error){try{std::rethrow_exception(error);}catch(const std::exception& e){result["error"]=e.what();}}
    else {result["result"]=**value;result["same"]=*value==response;}
    settled=true;
  });
  loop.runReady();if(!settled)throw std::runtime_error("side query did not settle");
  result["events"]=events;result["last"]=last;return result;
}
int main(){for(std::string line;std::getline(std::cin,line);)std::cout<<run(Json::parse(line))<<'\n';}
