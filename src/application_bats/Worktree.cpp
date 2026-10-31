#include "application_bats/Worktree.hpp"
#include "application/Async.hpp"
#include "application_hooks/Provider.hpp"
#include "git/CanonicalRoot.hpp"
#include "git_filesystem/RootFinder.hpp"
#include "runtime/JsonValue.hpp"
#include "runtime/IndigenousPath.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace batbox::application_bats {
namespace {
namespace fs=std::filesystem;
std::string text(const Json& value){return value.is_string()?value.get<std::string>():"";}
std::string flatten(std::string slug){
  std::replace(slug.begin(),slug.end(),'/','+');return slug;
}
void validateSlug(const std::string& slug){
  if(slug.size()>64)throw std::invalid_argument(
    "Invalid worktree name: must be 64 characters or fewer (got "+
    std::to_string(slug.size())+")");
  std::size_t begin=0;
  for(;;){
    const auto end=slug.find('/',begin);
    const auto segment=slug.substr(begin,end==std::string::npos?
      std::string::npos:end-begin);
    if(segment=="."||segment=="..")throw std::invalid_argument(
      "Invalid worktree name \""+slug+"\": must not contain \".\" or \"..\" path segments");
    if(segment.empty()||!std::all_of(segment.begin(),segment.end(),[](unsigned char c){
      return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||
        (c>='0'&&c<='9')||c=='.'||c=='_'||c=='-';
    }))throw std::invalid_argument("Invalid worktree name \""+slug+
      "\": each \"/\"-separated segment must be non-empty and contain only letters, digits, dots, underscores, and dashes");
    if(end==std::string::npos)break;
    begin=end+1;
  }
}
std::string trimmed(const std::string& value){return value::trim(value);}
} // namespace

WorktreeProvider::WorktreeProvider(WorktreeInputs inputs):inputs_(std::move(inputs)){
  if(inputs_.config.cwd.empty()||!inputs_.getSettings||
      !inputs_.enqueueBatMetadata)
    throw std::invalid_argument("Agent worktree provider imports incomplete");
}
process::ExecResult WorktreeProvider::git(const std::string& cwd,
    const std::vector<std::string>& args,bool noPoetry)const{
  if(inputs_.owner.isOwnerThread())throw std::logic_error(
    "Agent worktree git command requires tool worker");
  return application::awaitOnOwner<process::ExecResult>(inputs_.owner.dispatcher(),
    [this,cwd,args,noPoetry]{
      process::ExecImports imports;
      imports.execa=process::indigenousExeca(inputs_.owner);
      process::ExecOptions options;
      options.cwd=cwd;options.stdin="ignore";
      if(noPoetry)options.env=std::map<std::string,std::string>{
        {"GIT_TERMINAL_PROMPT","0"},{"GIT_ASKPASS",""}};
      return process::execFileNoThrowWithCwd(inputs_.owner,"git",args,
        options,imports);
    });
}
Json WorktreeProvider::create(const std::string& slug){
  validateSlug(slug);
  if(inputs_.hooks&&application::onOwnerAndWait<bool>(
      inputs_.owner.dispatcher(),[hooks=inputs_.hooks]{
        return hooks->hasHooksFor("WorktreeCreate");})){
    const auto path=application::awaitOnOwner<std::string>(
      inputs_.owner.dispatcher(),[hooks=inputs_.hooks,slug]{
        return hooks->createWorktreeHook(slug);});
    return Json{{"worktreePath",path},{"hookBased",true}};
  }
  auto finder=git_filesystem::indigenousRootFinder();
  auto canonical=git::indigenousCanonicalRootImports();
  canonical.findGitRoot=[finder](const std::string& start){
    return finder->find(start);};
  git::CanonicalRoot rootFinder(std::move(canonical));
  const auto root=rootFinder.find(runtime::pathToUtf8(inputs_.config.cwd));
  if(!root)throw std::runtime_error(
    "Cannot create agent worktree: not in a git repository and no WorktreeCreate hooks are configured.");
  const fs::path rootPath=runtime::pathFromUtf8(*root);
  const auto flat=flatten(slug);
  const auto branch="worktree-"+flat;
  const auto directory=rootPath/".batbox"/"worktrees";
  const auto path=directory/runtime::pathFromUtf8(flat);
  const auto pathUtf8=runtime::pathToUtf8(path);
  std::error_code error;
  // An existing worktree has a .git pointer file. A nested ordinary repo has
  // a .git directory and must not take the fast-resume path.
  if(fs::is_regular_file(path/".git",error)){
    std::ifstream pointer(path/".git",std::ios::binary);
    std::string firstLine;
    if(pointer&&std::getline(pointer,firstLine)&&
        trimmed(firstLine).rfind("gitdir:",0)==0){
      const auto existing=git(pathUtf8,{"rev-parse","HEAD"});
      if(existing.code==0&&!trimmed(existing.stdoutText).empty()){
        fs::last_write_time(path,fs::file_time_type::clock::now(),error);
        return Json{{"worktreePath",pathUtf8},{"worktreeBranch",branch},
          {"headCommit",trimmed(existing.stdoutText)},{"gitRoot",*root}};
      }
    }
  }
  const auto settings=parseWorktreeSettings(application::onOwnerAndWait<Json>(
    inputs_.owner.dispatcher(),inputs_.getSettings));
  fs::create_directories(directory);
  std::string defaultBranch="main";
  const auto remoteHead=git(*root,{"symbolic-ref","--quiet","--short",
    "refs/remotes/origin/HEAD"});
  const auto remoteName=trimmed(remoteHead.stdoutText);
  if(remoteHead.code==0&&remoteName.rfind("origin/",0)==0&&
      remoteName.size()>7)
    defaultBranch=remoteName.substr(7);
  else if(git(*root,{"rev-parse","--verify","origin/main"}).code!=0&&
      git(*root,{"rev-parse","--verify","origin/master"}).code==0)
    defaultBranch="master";
  std::string base="origin/"+defaultBranch;
  auto resolved=git(*root,{"rev-parse","--verify",base});
  if(resolved.code!=0){
    const auto fetched=git(*root,{"fetch","origin",defaultBranch},true);
    if(fetched.code==0)resolved=git(*root,{"rev-parse",base});
    else base="HEAD";
  }
  if(base=="HEAD")resolved=git(*root,{"rev-parse","HEAD"});
  if(resolved.code!=0||trimmed(resolved.stdoutText).empty())
    throw std::runtime_error("Failed to resolve base branch \""+base+"\": git rev-parse failed");
  const auto head=trimmed(resolved.stdoutText);
  std::vector<std::string> addArgs={"worktree","add"};
  if(!settings.sparsePaths.empty())addArgs.push_back("--no-checkout");
  addArgs.insert(addArgs.end(),{"-B",branch,pathUtf8,base});
  const auto created=git(*root,addArgs);
  if(created.code!=0)throw std::runtime_error(
    "Failed to create worktree: "+trimmed(created.stderrText));
  if(!settings.sparsePaths.empty()){
    auto failSparse=[this,&root,&pathUtf8](const std::string& error){
      const auto removed=git(*root,{"worktree","remove","--force",pathUtf8});
      if(removed.code!=0&&inputs_.diagnostic)
        inputs_.diagnostic("Failed to remove incomplete sparse worktree: "+
          trimmed(removed.stderrText));
      throw std::runtime_error(error);
    };
    std::vector<std::string> sparseArgs={"sparse-checkout","set","--cone","--"};
    sparseArgs.insert(sparseArgs.end(),settings.sparsePaths.begin(),
      settings.sparsePaths.end());
    const auto sparse=git(pathUtf8,sparseArgs);
    if(sparse.code!=0)failSparse("Failed to configure sparse-checkout: "+
      trimmed(sparse.stderrText));
    const auto checkout=git(pathUtf8,{"checkout","HEAD"});
    if(checkout.code!=0)failSparse("Failed to checkout sparse worktree: "+
      trimmed(checkout.stderrText));
  }
  try{postCreationSetup(*root,pathUtf8,settings);}
  catch(const std::exception& failure){
    if(inputs_.diagnostic)inputs_.diagnostic(
      "Agent worktree post-creation setup failed: "+
      std::string(failure.what()));
  }
  return Json{{"worktreePath",pathUtf8},{"worktreeBranch",branch},
    {"headCommit",head},{"gitRoot",*root}};
}
bool WorktreeProvider::hasChanges(const std::string& path,
    const std::string& head)const{
  try{
    const auto status=git(path,{"status","--porcelain"});
    if(status.code!=0||!trimmed(status.stdoutText).empty())return true;
    const auto commits=git(path,{"rev-list","--count",head+"..HEAD"});
    if(commits.code!=0)return true;
    return std::stoll(trimmed(commits.stdoutText))>0;
  }catch(...){return true;}
}
void WorktreeProvider::remove(const std::string& path,
    const Json& branch,const Json& root)const{
  const auto repo=text(root);
  if(repo.empty()){
    if(inputs_.diagnostic)
      inputs_.diagnostic("Cannot remove agent worktree: no git root provided");
    return;
  }
  try{
    const auto removed=git(repo,{"worktree","remove","--force",path});
    if(removed.code!=0){if(inputs_.diagnostic)
      inputs_.diagnostic("Failed to remove agent worktree: "+
        trimmed(removed.stderrText));return;}
    const auto name=text(branch);
    if(!name.empty()){
      const auto deleted=git(repo,{"branch","-D",name});
      if(deleted.code!=0&&inputs_.diagnostic)
        inputs_.diagnostic("Could not delete agent worktree branch: "+
          trimmed(deleted.stderrText));
    }
  }catch(const std::exception& error){
    if(inputs_.diagnostic)inputs_.diagnostic(error.what());
  }
}
void WorktreeProvider::enqueueMetadata(const std::string& id,
    const Json& data)const{inputs_.enqueueBatMetadata(id,data);}
void bindBatWorktree(bats::BatGadgetCallServices& services,
    WorktreeInputs inputs){
  auto provider=std::make_shared<WorktreeProvider>(std::move(inputs));
  services.launch.createBatWorktree=[provider](const std::string& slug){
    return provider->create(slug);};
  bats::BatGadgetWorktreeServices cleanup;
  cleanup.hasWorktreeChanges=[provider](const std::string& path,
      const std::string& head){return provider->hasChanges(path,head);};
  cleanup.removeBatWorktree=[provider](const std::string& path,
      const Json& branch,const Json& root){provider->remove(path,branch,root);};
  cleanup.enqueueBatMetadata=[provider](const std::string& id,
      const Json& data){provider->enqueueMetadata(id,data);};
  services.foreground.final.worktree=cleanup;
  services.async.worktree=std::move(cleanup);
}
} // namespace batbox::application_bats
