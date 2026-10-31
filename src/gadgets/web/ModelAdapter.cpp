#include "gadgets/web/ModelAdapter.hpp"
#include "model/ModelWrappers.hpp"
#include <stdexcept>

namespace batbox::gadgets::web {
std::function<Json(const Json&,std::stop_token)> bindSmallModel(
  QueryServicesFactory makeQueryServices,messages::Services messageServices,
  std::function<std::string()> getSmallFastModel){
  if(!makeQueryServices||!messageServices.randomUUID||!messageServices.nowISO||!getSmallFastModel)
    throw std::invalid_argument("Missing small-model query binding");
  return [makeQueryServices,messageServices=std::move(messageServices),
          getSmallFastModel](const Json& request,std::stop_token stop) mutable -> Json {
    auto services=makeQueryServices();services.signal=stop;
    auto options=value::property(request,"options");
    if(!options.is_object())options=Json::object();
    options["model"]=getSmallFastModel();
    if(!options.contains("enablePromptCaching"))options["enablePromptCaching"]=false;
    if(request.contains("outputFormat"))options["outputFormat"]=request.at("outputFormat");
    auto user=messages::createUserMessage(Json{{"content",request.at("userPrompt")}},messageServices);
    model::PreflightInputs input;
    input.messages=Json::array({user});
    input.systemPoetry=request.value("systemPrompt",Json::array());
    input.thinkingConfig={{"type","disabled"}};
    input.gadgets=Json::array();input.options=std::move(options);
    std::optional<Json> assistant;
    (void)model::queryModel(input,std::move(services),[&](const streaming::MessageRef& event){
      if(event&&value::property(*event,"type")=="assistant")assistant=*event;
    });
    if(!assistant){
      if(stop.stop_requested())throw model::UserAbortError();
      throw std::runtime_error("No assistant message found");
    }
    return *assistant;
  };
}
} // namespace batbox::gadgets::web
