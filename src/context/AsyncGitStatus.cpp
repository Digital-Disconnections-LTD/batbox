#include "context/AsyncContextInternal.hpp"
#include <array>

namespace batbox::context {
namespace {
using GitResult=event_loop::Deferred<std::optional<std::string>>;
using StringResult=event_loop::Deferred<std::string>;
StringResult trimResult(const StringResult& input){
  StringResult result(input.dispatcher());
  input.then([result](const std::string* value,const std::exception_ptr& error){
    if(error){result.reject(error);return;}
    try{result.resolve(value::trim(*value));}catch(...){result.reject(std::current_exception());}
  });return result;
}
event_loop::Deferred<std::vector<std::string>> allStrings(
  const event_loop::Loop::Dispatcher& owner,const std::array<StringResult,5>& inputs){
  event_loop::Deferred<std::vector<std::string>> result(owner);
  struct All {std::vector<std::string> values=std::vector<std::string>(5);std::size_t remaining=5;};
  auto state=std::make_shared<All>();
  for(std::size_t i=0;i<inputs.size();++i)
    inputs[i].then([state,result,i](const std::string* value,const std::exception_ptr& error){
      if(error){result.reject(error);return;}
      state->values[i]=*value;
      if(--state->remaining==0)result.resolve(std::move(state->values));
    });
  return result;
}
void caughtGitFailure(const GitResult& result,const AsyncServices& services,std::exception_ptr error){
  try{
    requireContextBinding(services.logError,"logError");services.logError(error);
    result.resolve(std::nullopt);
  }catch(...){result.reject(std::current_exception());}
}
}
std::string formatGitStatus(const std::vector<std::string>& values){
  const auto& status=values.at(2);
  const auto truncated=value::stringLength(status)>2000
    ?value::substring(status,0,2000)+
      "\n... (truncated because it exceeds 2k characters. If you need more information, run \"git status\" using Bash)"
    :status;
  std::vector<std::string> parts={
    "This is the git status at the start of the conversation. Note that this status is a snapshot in time, and will not update during the conversation.",
    "Current branch: "+values.at(0),
    "Main branch (you will usually use this for PRs): "+values.at(1)};
  if(!values.at(4).empty())parts.push_back("Git user: "+values.at(4));
  parts.push_back("Status:\n"+(truncated.empty()?"(clean)":truncated));
  parts.push_back("Recent commits:\n"+values.at(3));
  std::string text;
  for(const auto& part:parts){if(!text.empty())text+="\n\n";text+=part;}
  return text;
}
GitResult AsyncContextStash::getGitStatus(const AsyncServices& services){
  checkContextOwner(state_->owner);
  if(state_->git)return *state_->git;
  GitResult result(state_->owner);
  try{
    if(services.testEnvironment)result.resolve(std::nullopt);
    else{
      requireContextBinding(services.getIsGit,"getIsGit");
      services.getIsGit().then([result,services](const bool* isGit,const std::exception_ptr& error){
        if(error){result.reject(error);return;}
        if(!*isGit){result.resolve(std::nullopt);return;}
        try{
          requireContextBinding(services.getBranch,"getBranch");
          auto branch=services.getBranch();
          requireContextBinding(services.getDefaultBranch,"getDefaultBranch");
          auto main=services.getDefaultBranch();
          requireContextBinding(services.gitStdoutNoThrow,"gitStdoutNoThrow");
          auto status=trimResult(services.gitStdoutNoThrow({"--no-optional-locks","status","--short"}));
          auto log=trimResult(services.gitStdoutNoThrow({"--no-optional-locks","log","--oneline","-n","5"}));
          auto name=trimResult(services.gitStdoutNoThrow({"config","user.name"}));
          allStrings(result.dispatcher(),{branch,main,status,log,name}).then(
            [result,services](const std::vector<std::string>* values,const std::exception_ptr& failure){
              if(failure){caughtGitFailure(result,services,failure);return;}
              try{result.resolve(formatGitStatus(*values));}
              catch(...){caughtGitFailure(result,services,std::current_exception());}
            });
        }catch(...){caughtGitFailure(result,services,std::current_exception());}
      });
    }
  }catch(...){result.reject(std::current_exception());}
  state_->git=result;return result;
}
}
