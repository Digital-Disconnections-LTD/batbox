#include "settings/schema/IndigenousBindings.hpp"
#include <stdexcept>
using namespace batbox;
namespace {
void need(bool okay,const char* message){
  if(!okay)throw std::runtime_error(message);
}
}
int main(){
  const auto validate=settings::schema::bindIndigenousSettingsSchema({},{});
  const auto valid=validate(value::parse(R"SETTINGS({"model":"fast","env":{"COUNT":12,"FLAG":true},"permissions":{"allow":["Read"]},"hooks":{"Stop":[{"hooks":[{"type":"http","url":"https://EXAMPLE.org:443/a b"}]}]}})SETTINGS"));
  need(valid.success,"valid settings rejected");
  need(valid.data.at("model")=="fast","model not kept");
  need(valid.data.at("env").at("COUNT")=="12","env number not coerced");
  need(valid.data.at("env").at("FLAG")=="true","env boolean not coerced");
  need(valid.data.at("permissions").at("allow")==Json::array({"Read"}),
    "permission rules not kept");
  need(valid.data.at("hooks").at("Stop")[0].at("hooks")[0].at("url")==
    "https://example.org/a%20b","hook URL not canonical");
  const auto invalid=validate(Json{{"model",12}});
  need(!invalid.success&&!invalid.issues.empty(),"invalid model accepted");
  const auto unchecked=validate(value::parse(R"SETTINGS({"autoMode":7,"minSleepDurationMs":"soon","defaultView":3,"voiceEnabled":"yes","xaaIdp":1,"permissions":{"disableAutoMode":5}})SETTINGS"));
  need(unchecked.success,"keys the schema does not describe were rejected");
  need(unchecked.data.at("autoMode")==7&&unchecked.data.at("minSleepDurationMs")=="soon"&&
    unchecked.data.at("defaultView")==3&&unchecked.data.at("voiceEnabled")=="yes"&&
    unchecked.data.at("xaaIdp")==1,"undescribed keys were not passed through as written");
  need(unchecked.data.at("permissions").at("disableAutoMode")==5,
    "an undescribed permission key was not passed through as written");
  const auto autoMode=validate(Json{{"permissions",{{"defaultMode","auto"}}}});
  need(!autoMode.success,"the auto permission mode was accepted");
}
