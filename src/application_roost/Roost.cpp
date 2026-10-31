#include "application_roost/Roost.hpp"
#include "runtime/IndigenousPath.hpp"
#include <stdexcept>
#include <system_error>

namespace batbox::application_roost {
namespace {
std::string stringField(const Json& value,const char* key){
  const auto& field=value::property(value,key);
  return field.is_string()?field.get<std::string>():std::string{};
}
std::filesystem::path directory(const std::filesystem::path& value){
  if(value.empty())throw std::invalid_argument("Workspace directory is empty");
  const auto absolute=std::filesystem::absolute(value);
  if(!std::filesystem::is_directory(absolute))
    throw std::filesystem::filesystem_error("Workspace directory is unavailable",
      absolute,std::make_error_code(std::errc::not_a_directory));
  return std::filesystem::canonical(absolute);
}
bool equal(const Snapshot& left,const Snapshot& right){
  return left.cwd==right.cwd&&left.projectRoot==right.projectRoot&&
    left.worktreeFlight==right.worktreeFlight;
}
}
Roost::Roost(event_loop::Loop& owner,std::filesystem::path initialCwd):
    owner_(owner){
  requireOwner();
  current_.cwd=directory(initialCwd);
  current_.projectRoot=current_.cwd;
}
void Roost::requireOwner()const{
  if(!owner_.isOwnerThread())
    throw std::logic_error("Workspace transition requires owner loop");
}
const Snapshot& Roost::snapshot()const{requireOwner();return current_;}
std::filesystem::path Roost::cwd()const{requireOwner();return current_.cwd;}
std::filesystem::path Roost::projectRoot()const{
  requireOwner();return current_.projectRoot;
}
Plan Roost::planForResume(const Json& log,const std::string& entrypoint)const{
  requireOwner();
  if(!log.is_object())throw std::invalid_argument("Workspace resume log required");
  Plan plan{current_,false};
  if(entrypoint=="fork")return plan;
  const auto& savedWorktree=value::property(log,"worktreeSession");
  if(!savedWorktree.is_null()&&!savedWorktree.is_object())
    throw std::invalid_argument("Saved worktree state must be an object");
  auto base=stringField(log,"projectPath");
  if(savedWorktree.is_object()){
    const auto original=stringField(savedWorktree,"originalCwd");
    if(!original.empty())base=original;
  }
  if(!base.empty()){
    std::error_code error;
    if(std::filesystem::is_directory(runtime::pathFromUtf8(base),error))
      plan.target.projectRoot=directory(runtime::pathFromUtf8(base));
    else if(!savedWorktree.is_object())
      throw std::filesystem::filesystem_error(
        "Resumed project directory is unavailable",base,
        std::make_error_code(std::errc::not_a_directory));
    // A recorded worktree can still exist after its original project path
    // disappears. Enter that worktree and leave projectRoot
    // stable, so preserve the currently available root in this case.
  }
  if(savedWorktree.is_object()){
    const auto savedPath=stringField(savedWorktree,"worktreePath");
    if(savedPath.empty())
      throw std::invalid_argument("Saved worktree has no path");
    std::error_code error;
    const bool available=std::filesystem::is_directory(
      runtime::pathFromUtf8(savedPath),error);
    if(available){
      plan.target.cwd=directory(runtime::pathFromUtf8(savedPath));
      plan.target.worktreeFlight=savedWorktree;
    }else{
      // Clear stale worktree metadata when its directory has
      // disappeared, then continue in the original project directory.
      plan.target.cwd=plan.target.projectRoot;
      plan.target.worktreeFlight=Json();
      plan.missingWorktree=true;
    }
  }else{
    plan.target.cwd=plan.target.projectRoot;
    plan.target.worktreeFlight=Json();
  }
  return plan;
}
bool Roost::needsRebind(const Plan& plan)const{
  requireOwner();return !equal(current_,plan.target);
}
void Roost::activate(const Plan& plan,const Rebind& rebind){
  requireOwner();
  if(!needsRebind(plan))return;
  if(!rebind)throw std::invalid_argument(
    "Workspace switch requires application provider rebinding");
  const Snapshot previous=current_;
  const Snapshot next=plan.target;
  // Re-check at use time: the directory can disappear after preflight.
  const auto target=directory(next.cwd);
  if(target!=next.cwd)throw std::runtime_error(
    "Workspace directory changed during resume");
  if(target!=previous.cwd)std::filesystem::current_path(target);
  current_=next;
  try{
    rebind(previous,next);
  }catch(...){
    const auto originalError=std::current_exception();
    std::error_code rollbackError;
    if(target!=previous.cwd)
      std::filesystem::current_path(previous.cwd,rollbackError);
    current_=previous;
    try{rebind(next,previous);}catch(...){
      throw std::runtime_error("Workspace provider rebinding failed and rollback failed");
    }
    if(rollbackError)
      throw std::filesystem::filesystem_error("Workspace cwd rollback failed",
        previous.cwd,rollbackError);
    std::rethrow_exception(originalError);
  }
}
void Roost::restore(const Snapshot& previous,const Rebind& rebind){
  requireOwner();activate(Plan{previous,false},rebind);
}
} // namespace batbox::application_roost
