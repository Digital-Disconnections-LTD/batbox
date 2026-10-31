#include "side_query/SideQuery.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace batbox::side_query {
namespace {
void need(const auto& value,const char* name){
  if(!value)throw std::invalid_argument(std::string("Side query import required: ")+name);
}
Json request(const Json& options,const Services& services){
  const auto model=options.at("model").get<std::string>();
  need(services.getModelBetas,"getModelBetas");
  auto betas=services.getModelBetas(model);
  const auto& format=value::property(options,"output_format");
  if(value::truthy(format)){
    need(services.supportsStructuredOutputs,"supportsStructuredOutputs");
    if(services.supportsStructuredOutputs(model)&&
       std::find(betas.begin(),betas.end(),services.structuredOutputsBeta)==betas.end())
      betas.push_back(services.structuredOutputsBeta);
  }
  Json system=Json::array();
  if(!value::truthy(value::property(options,"skipSystemPromptPrefix"))){
    need(services.systemPoetryPrefix,"systemPromptPrefix");
    system.push_back({{"type","text"},{"text",services.systemPoetryPrefix(
      Json{{"isNonInteractive",false},{"hasAppendSystemPrompt",false}})}});
  }
  const auto& supplied=value::property(options,"system");
  if(supplied.is_array())for(const auto& block:supplied)system.push_back(block);
  else if(value::truthy(supplied))system.push_back({{"type","text"},{"text",supplied}});
  const Json maximum=options.contains("max_tokens")?options["max_tokens"]:Json(1024);
  Json thinking(Json::value_t::discarded);
  if(options.contains("thinking")){
    const auto& value=options["thinking"];
    if(value==Json(false))thinking={{"type","disabled"}};
    else thinking={{"type","enabled"},{"budget_tokens",
      std::min(value::numberOr(value),value::numberOr(maximum)-1)}};
  }
  need(services.normalizeModel,"normalizeModel");
  Json body={{"model",services.normalizeModel(model)},{"max_tokens",maximum},
    {"system",std::move(system)},{"messages",options.at("messages")}};
  for(const auto* key:{"tools","tool_choice"})
    if(value::truthy(value::property(options,key)))body[key]=options[key];
  if(value::truthy(format))body["output_config"]={{"format",format}};
  if(options.contains("temperature"))body["temperature"]=options["temperature"];
  if(value::truthy(value::property(options,"stop_sequences")))
    body["stop_sequences"]=options["stop_sequences"];
  if(!thinking.is_discarded())body["thinking"]=std::move(thinking);
  if(!betas.empty())body["betas"]=std::move(betas);
  return body;
}
}
event_loop::Deferred<ResponseRef> sideQuery(event_loop::Loop& loop,Json options,
    std::stop_token signal,Services services){
  event_loop::Deferred<ResponseRef> output(loop);
  try{
    need(services.getClient,"getClient");
    ClientOptions clientOptions;
    clientOptions.model=options.at("model").get<std::string>();
    if(options.contains("maxRetries"))clientOptions.maxRetries=options["maxRetries"].get<int>();
    auto pending=services.getClient(std::move(clientOptions));
    if(!loop.dispatcher().sameLoop(pending.dispatcher()))
      throw std::invalid_argument("Side query client belongs to another event loop");
    pending.then([output,options=std::move(options),signal,services=std::move(services)](
        const std::shared_ptr<Client>* client,const std::exception_ptr& error){
      try{
        if(error)std::rethrow_exception(error);
        if(!client||!*client)throw std::runtime_error("Side query client required");
        auto body=request(options,services);
        need(services.nowMillis,"nowMillis");
        (void)services.nowMillis();
        need((*client)->create,"client.create");
        auto response=(*client)->create(std::move(body),signal);
        if(!output.dispatcher().sameLoop(response.dispatcher()))
          throw std::invalid_argument("Side query response belongs to another event loop");
        response.then([output,services](const ResponseRef* value,
            const std::exception_ptr& failure){
          try{
            if(failure)std::rethrow_exception(failure);
            if(!value||!*value)throw std::runtime_error("Side query response required");
            const double now=services.nowMillis();
            need(services.setLastApiCompletionTimestamp,"setLastApiCompletionTimestamp");
            services.setLastApiCompletionTimestamp(now);
            output.resolve(*value);
          }catch(...){output.reject(std::current_exception());}
        });
      }catch(...){output.reject(std::current_exception());}
    });
  }catch(...){output.reject(std::current_exception());}
  return output;
}
} // namespace batbox::side_query
