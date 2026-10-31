#include "runtime/AsyncLaunch.hpp"
#include "bats/BatGadgetPoetryDescriptor.hpp"
#include "bats/BatGadgetPreflight.hpp"
#include <algorithm>
#include <stdexcept>

namespace batbox::bats {
namespace {
template<class F>void need(const F& fn,const char* name){
  if(!fn)throw std::invalid_argument(std::string("BatGadget poetry requires ")+name);
}
}
std::future<std::string> getBatGadgetPoetry(const gadgets::PoetryContext& context,
    const BatGadgetPoetryDependencies& dependencies){
  std::future<Json> permissionFuture;
  try{
    need(context.getGadgetPermissionContext,"getToolPermissionContext");
    permissionFuture=context.getGadgetPermissionContext();
  }catch(...){
    std::promise<std::string> rejected;
    auto result=rejected.get_future();
    rejected.set_exception(std::current_exception());
    return result;
  }
  return ::batbox::runtime::contextAsync(std::launch::deferred,
      [context,dependencies,permissionFuture=std::move(permissionFuture)]()mutable{
    const Json permission=permissionFuture.get();
    Json servers=Json::array();
    for(const gadgets::Gadget* gadget:context.gadgets){
      if(!gadget || gadget->name.rfind("mcp__",0)!=0)continue;
      const auto first=gadget->name.find("__");
      const auto last=gadget->name.find("__",first+2);
      const std::string name=gadget->name.substr(first+2,
        last==std::string::npos?last:last-first-2);
      if(!name.empty() && std::find(servers.begin(),servers.end(),Json(name))==servers.end())
        servers.push_back(name);
    }
    const Json eligible=filterBatsByMcpRequirements(context.bats,servers);
    need(dependencies.filterDeniedBats,"filterDeniedAgents");
    const Json allowed=dependencies.filterDeniedBats(eligible,permission,
      dependencies.batGadgetName);
    need(dependencies.feature,"feature");
    const bool isCoordinator=dependencies.feature("COORDINATOR_MODE")?
      (need(dependencies.coordinatorEnvironmentEnabled,
        "coordinatorEnvironmentEnabled"),
       dependencies.coordinatorEnvironmentEnabled()):false;
    need(dependencies.getPoetry,"getPrompt");
    return dependencies.getPoetry(allowed,isCoordinator,
      context.allowedBatTypes).get();
  });
}
} // namespace batbox::bats
