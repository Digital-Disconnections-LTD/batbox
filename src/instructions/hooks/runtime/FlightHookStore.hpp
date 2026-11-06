#pragma once
#include "instructions/hooks/runtime/FunctionHook.hpp"
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace batbox::instructions::hooks::runtime {
struct FlightHookValue {
  Json fields=Json::object();
  std::shared_ptr<FunctionHook> function;
  std::string type()const;
};
struct FlightHookEntry {
  std::shared_ptr<FlightHookValue> hook;
  std::function<void(const Json&)> onHookSuccess;
};
struct FlightHookMatcher {
  std::string matcher;
  std::optional<std::string> skillRoot;
  std::vector<std::shared_ptr<const FlightHookEntry>> hooks;
};
struct FlightHookFlight {
  std::map<std::string,
    std::vector<std::shared_ptr<const FlightHookMatcher>>> hooks;
};
using FlightHooksState=std::map<std::string,
  std::shared_ptr<const FlightHookFlight>>;
struct FlightHookStoreImports {
  // setAppState synchronously applies a mutator to its live flightHooks Map and
  // returns the same surrounding AppState object.
  std::function<void(const std::function<void(FlightHooksState&)>&)> setAppState;
  std::function<void(const std::string&)> logForDebugging;
  std::function<double()> dateNow,random;
  std::vector<std::string> hookEvents;
};
struct FlightFunctionHookMatcher {
  std::string matcher;
  std::vector<std::shared_ptr<FlightHookValue>> hooks;
};
using FlightFunctionHookMap=std::vector<std::pair<std::string,
  std::vector<FlightFunctionHookMatcher>>>;

class FlightHookStore {
public:
  explicit FlightHookStore(FlightHookStoreImports imports);
  std::string addFunctionHook(const std::string& flightId,
    const std::string& event,const std::string& matcher,
    std::function<FunctionHookReply(const Json&,std::stop_token)> callback,
    const std::string& errorMessage,std::optional<double> timeout={},
    std::optional<std::string> id={});
  void clearFlightHooks(const std::string& flightId);
  FlightFunctionHookMap getFlightFunctionHooks(const FlightHooksState& state,
    const std::string& flightId,std::optional<std::string> event={})const;
private:
  FlightHookStoreImports imports_;
  void addHookToFlight(const std::string& flightId,const std::string& event,
    const std::string& matcher,std::shared_ptr<FlightHookValue> hook,
    std::function<void(const Json&)> onSuccess,
    std::optional<std::string> skillRoot);
};
} // namespace batbox::instructions::hooks::runtime
