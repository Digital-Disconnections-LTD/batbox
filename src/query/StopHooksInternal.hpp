#pragma once
#include "query/StopHooks.hpp"
#include <stdexcept>

namespace batbox::query::stop_internal {
const Json& field(const Json& object,const char* name);
std::string stringField(const Json& object,const char* name);
Json concatMessages(const Json& a,const Json& b);
template<class T>void need(const T& callback,const char* name){
  if(!callback)throw std::invalid_argument(std::string("Unbound stop hook import: ")+name);
}
struct MainState {
  Json blockingErrors=Json::array();
  Json hookErrors=Json::array();
  Json hookInfos=Json::array();
  std::string gadgetUseID;
  int hookCount=0;
  bool prevented=false;
  std::string stopReason;
  bool hasOutput=false;
  bool aborted=false;
};
bool consumeStopResult(const Json& result,const execution::Context& context,
  MainState& state,StopHookServices& services,const Yield& yield);
Json runPartnerHooks(const Json& permissionMode,const execution::Context& context,
  StopHookServices& services,const Yield& yield);
} // namespace batbox::query::stop_internal
