#include "context/Context.hpp"
#include "runtime/JsonValue.hpp"
#include <stdexcept>

namespace batbox::context {
namespace {
template<class Fn> void need(const Fn& function,const char* name){
  if(!function)throw std::invalid_argument(std::string("Missing context binding: ")+name);
}
std::string joinParagraphs(const std::vector<std::string>& paragraphs){
  std::string result;
  for(std::size_t i=0;i<paragraphs.size();++i){
    if(i)result+="\n\n";
    result+=paragraphs[i];
  }
  return result;
}
}
std::optional<std::string> ContextStash::getGitStatus(const Services& services){
  if(gitStatus_)return *gitStatus_;
  if(gitError_)std::rethrow_exception(gitError_);
  try{
    if(services.testEnvironment){gitStatus_.emplace(std::nullopt);return std::nullopt;}
    need(services.getIsGit,"getIsGit");
    if(!services.getIsGit().get()){
      gitStatus_.emplace(std::nullopt);return std::nullopt;
    }
    try{
      need(services.getBranch,"getBranch");
      need(services.getDefaultBranch,"getDefaultBranch");
      need(services.gitStdoutNoThrow,"gitStdoutNoThrow");
      auto branch=services.getBranch();
      auto mainBranch=services.getDefaultBranch();
      auto status=services.gitStdoutNoThrow(
        {"--no-optional-locks","status","--short"});
      auto log=services.gitStdoutNoThrow(
        {"--no-optional-locks","log","--oneline","-n","5"});
      auto userName=services.gitStdoutNoThrow({"config","user.name"});
      const auto branchText=branch.get();
      const auto mainBranchText=mainBranch.get();
      const auto statusText=value::trim(status.get());
      const auto logText=value::trim(log.get());
      const auto userNameText=value::trim(userName.get());
      const auto truncated=value::stringLength(statusText)>2000
        ?value::substring(statusText,0,2000)+
          "\n... (truncated because it exceeds 2k characters. If you need more information, run \"git status\" using Bash)"
        :statusText;
      std::vector<std::string> paragraphs={
        "This is the git status at the start of the conversation. Note that this status is a snapshot in time, and will not update during the conversation.",
        "Current branch: "+branchText,
        "Main branch (you will usually use this for PRs): "+mainBranchText,
      };
      if(!userNameText.empty())paragraphs.push_back("Git user: "+userNameText);
      paragraphs.push_back("Status:\n"+(truncated.empty()?"(clean)":truncated));
      paragraphs.push_back("Recent commits:\n"+logText);
      gitStatus_=std::optional<std::string>(joinParagraphs(paragraphs));
      return *gitStatus_;
    }catch(...){
      if(services.logError)services.logError(std::current_exception());
      gitStatus_.emplace(std::nullopt);
      return std::nullopt;
    }
  }catch(...){gitError_=std::current_exception();throw;}
}
void ContextStash::clearGitStatusStash(){gitStatus_.reset();gitError_=nullptr;}
}
