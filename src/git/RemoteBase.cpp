#include "git/RemoteBase.hpp"
#include "runtime/JsonValue.hpp"
#include <memory>
#include <regex>
#include <stdexcept>
#include <utility>

namespace batbox::git {
namespace {
struct Search:std::enable_shared_from_this<Search> {
  event_loop::Loop& loop;
  CommandImports imports;
  event_loop::Deferred<std::optional<std::string>> output;
  Search(event_loop::Loop& owner,CommandImports values):
    loop(owner),imports(std::move(values)),output(owner){}
  using Callback=std::function<void(const process::ExecResult&)>;
  void request(std::vector<std::string> args,Callback continuation){
    try{
      process::ExecOptions options;options.preserveOutputOnError=false;
      auto child=imports.execFileNoThrow(imports.gitExe(),args,options);
      child.then([self=shared_from_this(),continuation=std::move(continuation)](
        const process::ExecResult* value,const std::exception_ptr& error){
        if(error){self->output.reject(error);return;}
        try{continuation(*value);}catch(...){self->output.reject(std::current_exception());}
      });
    }catch(...){output.reject(std::current_exception());}
  }
  void start(){
    request({"rev-parse","--abbrev-ref","--symbolic-full-name","@{u}"},
      [self=shared_from_this()](const process::ExecResult& result){
        const auto branch=value::trim(result.stdoutText);
        if(result.code==0&&!branch.empty()){
          self->output.resolve(branch);return;
        }
        self->remoteHead();
      });
  }
  void remoteHead(){
    request({"remote","show","origin","--","HEAD"},
      [self=shared_from_this()](const process::ExecResult& result){
        if(result.code==0){
          std::smatch match;
          if(std::regex_search(result.stdoutText,match,
            std::regex(R"(HEAD branch: (\S+))"))&&match.size()>1&&
            !match[1].str().empty()){
            self->output.resolve("origin/"+match[1].str());return;
          }
        }
        self->candidate(0);
      });
  }
  void candidate(std::size_t index){
    static const std::vector<std::string> names{
      "origin/main","origin/staging","origin/master"};
    if(index>=names.size()){output.resolve(std::nullopt);return;}
    request({"rev-parse","--verify",names[index]},
      [self=shared_from_this(),index](const process::ExecResult& result){
        static const std::vector<std::string> values{
          "origin/main","origin/staging","origin/master"};
        if(result.code==0)self->output.resolve(values[index]);
        else self->candidate(index+1);
      });
  }
};
}
event_loop::Deferred<std::optional<std::string>> findRemoteBase(
  event_loop::Loop& loop,CommandImports imports){
  if(!loop.isOwnerThread())throw std::logic_error("Git remote base owner required");
  event_loop::Deferred<std::optional<std::string>> result(loop);
  if(!imports.gitExe||!imports.execFileNoThrow){
    result.reject(std::make_exception_ptr(std::invalid_argument(
      "Git remote base imports required")));return result;
  }
  auto state=std::make_shared<Search>(loop,std::move(imports));
  state->start();return state->output;
}
} // namespace batbox::git
