#include "git/Stash.hpp"
#include <memory>
#include <stdexcept>
#include <utility>

namespace batbox::git {
namespace {
struct Stash:std::enable_shared_from_this<Stash> {
  event_loop::Loop& loop;
  StashImports imports;
  event_loop::Deferred<bool> output;
  std::string message;
  Stash(event_loop::Loop& owner,StashImports values,std::string text):
    loop(owner),imports(std::move(values)),output(owner),message(std::move(text)){}
  void run(std::vector<std::string> args,std::function<void(int)> next){
    try{
      process::ExecOptions options;options.preserveOutputOnError=false;
      auto pending=imports.commands.execFileNoThrow(imports.commands.gitExe(),args,options);
      pending.then([self=shared_from_this(),next=std::move(next)](
        const process::ExecResult* result,const std::exception_ptr& error){
        if(error){self->output.resolve(false);return;}
        try{next(result->code);}catch(...){self->output.resolve(false);}
      });
    }catch(...){output.resolve(false);}
  }
  void begin(){
    try{
      auto pending=imports.getFileStatus();
      pending.then([self=shared_from_this()](const GitFileStatus* value,
        const std::exception_ptr& error){
        if(error){self->output.resolve(false);return;}
        try{
          if(!value->untracked.empty()){
            std::vector<std::string> args{"add"};
            args.insert(args.end(),value->untracked.begin(),value->untracked.end());
            self->run(std::move(args),[self](int code){
              if(code!=0){self->output.resolve(false);return;}
              self->stash();
            });
          }else self->stash();
        }catch(...){self->output.resolve(false);}
      });
    }catch(...){output.resolve(false);}
  }
  void stash(){
    run({"stash","push","--message",message},
      [self=shared_from_this()](int code){self->output.resolve(code==0);});
  }
};
}
event_loop::Deferred<bool> stashToCleanState(event_loop::Loop& loop,
  const StashImports& imports,std::optional<std::string> message){
  if(!loop.isOwnerThread())throw std::logic_error("Git stash owner required");
  event_loop::Deferred<bool> output(loop);
  try{
    if(!imports.getFileStatus||!imports.commands.gitExe||
      !imports.commands.execFileNoThrow||
      ((!message||message->empty())&&!imports.nowIso))
      throw std::invalid_argument("Git stash imports required");
    // Choose the message or formatted current time before the first await.
    const auto stashMessage=message&&!message->empty()?*message:
      "Batbox auto-stash - "+imports.nowIso();
    auto state=std::make_shared<Stash>(loop,imports,stashMessage);
    state->begin();return state->output;
  }catch(...){output.resolve(false);return output;}
}
} // namespace batbox::git
