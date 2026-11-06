#include "instructions/hooks/runtime/FlightHookStore.hpp"
#include "runtime/JsonValue.hpp"
#include <algorithm>
#include <stdexcept>

namespace batbox::instructions::hooks::runtime {
namespace {
const Json& field(const Json& value,const char* name){
  return value::property(value,name);
}
std::string str(const Json& value){
  return value.is_string()?value.get<std::string>():std::string{};
}
}
std::string FlightHookValue::type()const{return str(field(fields,"type"));}
FlightHookStore::FlightHookStore(FlightHookStoreImports imports):
  imports_(std::move(imports)){
  if(!imports_.setAppState||!imports_.logForDebugging)
    throw std::invalid_argument("Session hook app-state and local-debug imports required");
}
void FlightHookStore::addHookToFlight(const std::string& flightId,
  const std::string& event,const std::string& matcher,
  std::shared_ptr<FlightHookValue> hook,
  std::function<void(const Json&)> onSuccess,
  std::optional<std::string> skillRoot){
  if(!hook)throw std::invalid_argument("Session hook identity required");
  imports_.setAppState([&](FlightHooksState& state){
    auto current=state.find(flightId);
    FlightHookFlight store=current==state.end()?FlightHookFlight{}:
      *current->second;
    auto matchers=store.hooks[event];
    auto found=std::find_if(matchers.begin(),matchers.end(),
      [&](const std::shared_ptr<const FlightHookMatcher>& item){
        return item->matcher==matcher&&item->skillRoot==skillRoot;
      });
    auto entry=std::make_shared<FlightHookEntry>(
      FlightHookEntry{std::move(hook),std::move(onSuccess)});
    if(found!=matchers.end()){
      auto replacement=std::make_shared<FlightHookMatcher>(**found);
      replacement->hooks.push_back(std::move(entry));
      *found=std::move(replacement);
    }else{
      auto added=std::make_shared<FlightHookMatcher>();
      added->matcher=matcher;added->skillRoot=std::move(skillRoot);
      added->hooks.push_back(std::move(entry));
      matchers.push_back(std::move(added));
    }
    store.hooks[event]=std::move(matchers);
    state[flightId]=std::make_shared<FlightHookFlight>(std::move(store));
  });
  imports_.logForDebugging("Added session hook for event "+event+
    " in session "+flightId);
}
std::string FlightHookStore::addFunctionHook(const std::string& flightId,
  const std::string& event,const std::string& matcher,
  std::function<FunctionHookReply(const Json&,std::stop_token)> callback,
  const std::string& errorMessage,std::optional<double> timeout,
  std::optional<std::string> id){
  if(!callback)throw std::invalid_argument("Function hook callback required");
  std::string selected=id.value_or("");
  if(selected.empty()){
    if(!imports_.dateNow||!imports_.random)
      throw std::invalid_argument("Function hook clock and random imports required");
    const auto now=value::numberToString(imports_.dateNow());
    const auto random=value::numberToString(imports_.random());
    selected="function-hook-"+now+"-"+random;
  }
  const double actualTimeout=timeout&&*timeout!=0?*timeout:5000;
  auto hook=std::make_shared<FlightHookValue>();
  hook->fields={{"type","function"},{"id",selected},
    {"timeout",actualTimeout},{"errorMessage",errorMessage}};
  hook->function=std::make_shared<FunctionHook>();
  hook->function->fields=hook->fields;
  hook->function->timeout=actualTimeout;
  hook->function->errorMessage=errorMessage;
  hook->function->callback=std::move(callback);
  addHookToFlight(flightId,event,matcher,std::move(hook),{},{});
  return selected;
}
void FlightHookStore::clearFlightHooks(const std::string& flightId){
  imports_.setAppState([&](FlightHooksState& state){state.erase(flightId);});
  imports_.logForDebugging("Cleared all session hooks for session "+flightId);
}
} // namespace batbox::instructions::hooks::runtime
