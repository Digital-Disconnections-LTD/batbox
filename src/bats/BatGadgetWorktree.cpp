#include "bats/BatGadgetWorktree.hpp"
#include <stdexcept>

namespace batbox::bats {
namespace {
const Json& field(const Json& object,const char* key){return value::property(object,key);}
std::string text(const Json& value){return value.is_string()?value.get<std::string>():"";}
} // namespace
Json cleanupBatWorktreeIfNeeded(BatGadgetWorktreeState& state,
    BatGadgetWorktreeServices& services){
  if(!state.worktreeInfo)return Json::object();
  const Json info=*state.worktreeInfo;
  const auto& path=field(info,"worktreePath");
  const auto& branch=field(info,"worktreeBranch");
  const auto& head=field(info,"headCommit");
  const auto& gitRoot=field(info,"gitRoot");
  const auto& hook=field(info,"hookBased");
  // Clear before any imported asynchronous work, so catch/finally may call
  // cleanup again without removing the same worktree twice.
  state.worktreeInfo.reset();
  if(value::truthy(hook))return Json{{"worktreePath",path}};
  if(value::truthy(head)){
    if(!services.hasWorktreeChanges || !services.removeBatWorktree ||
       !services.enqueueBatMetadata)
      throw std::invalid_argument("Worktree cleanup requires imported services");
    const bool changed=services.hasWorktreeChanges(text(path),text(head));
    if(!changed){
      services.removeBatWorktree(text(path),branch,gitRoot);
      services.enqueueBatMetadata(state.earlyBatId,
        Json{{"agentType",field(state.selectedBat,"agentType")},
             {"description",state.description}});
      return Json::object();
    }
  }
  Json kept={{"worktreePath",path}};
  if(info.contains("worktreeBranch"))kept["worktreeBranch"]=branch;
  return kept;
}
} // namespace batbox::bats
