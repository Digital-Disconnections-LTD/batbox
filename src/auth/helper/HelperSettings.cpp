#include "auth/helper/Helper.hpp"
#include <cmath>
#include <stdexcept>

namespace batbox::auth::helper {
namespace {
Json property(const Json& object,const char* key){
  return object.is_object()&&object.contains(key)?object[key]:Json(nullptr);
}
}
Stash::Stash(event_loop::Loop& loop,Services services):
    loop_(loop),services_(std::move(services)){
  if(!services_.configuredHelper||!services_.settingsForSource||
      !services_.environment||!services_.nowMs||!services_.trustAccepted||
      !services_.trustError||!services_.consoleError||!services_.debug||
      !services_.executeShell||services_.feedbackChannel.empty())
    throw std::invalid_argument("Missing API key helper binding");
}
bool Stash::isHelperFromProjectOrLocalSettings()const{
  const auto helper=services_.configuredHelper();
  if(!value::truthy(helper))return false;
  const auto project=services_.settingsForSource("projectSettings");
  const auto local=services_.settingsForSource("localSettings");
  return property(project,"apiKeyHelper")==helper||
    property(local,"apiKeyHelper")==helper;
}
double Stash::calculateTTL()const{
  const auto env=services_.environment(services_.ttlEnvironment);
  if(value::truthy(env)){
    const auto text=env.is_string()?env.get<std::string>():value::stringify(env);
    const auto value=value::parseInt10(text);
    if(!std::isnan(value)&&value>=0)return value;
    services_.debug("Found "+services_.ttlEnvironment+
      " env var, but it was not a valid number. Got "+text,"error");
  }
  return 5*60*1000;
}
double Stash::elapsedMs()const{
  if(!inflight_||!inflight_->startedAt||!*inflight_->startedAt)return 0;
  return services_.nowMs()-*inflight_->startedAt;
}
std::optional<std::string> Stash::getStashed()const{
  return stashed_?std::optional(stashed_->value):std::nullopt;
}
void Stash::clear(){++epoch_;stashed_.reset();inflight_.reset();}
void Stash::prefetchIfSafe(bool isNonInteractiveFlight){
  if(isHelperFromProjectOrLocalSettings()&&!services_.trustAccepted())return;
  (void)get(isNonInteractiveFlight);
}
}
