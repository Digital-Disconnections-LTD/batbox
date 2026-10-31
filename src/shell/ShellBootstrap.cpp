#include "shell/ShellBootstrap.hpp"
#include <memory>
#include <stdexcept>
#include <vector>

namespace batbox::shell {
namespace {
bool supported(const std::string& path){
  return path.find("bash")!=std::string::npos||path.find("zsh")!=std::string::npos;
}
}
event_loop::Deferred<std::string> findSuitableShellAsync(event_loop::Loop& loop,
  const DiscoveryServices& services){
  if(!loop.isOwnerThread())throw std::logic_error("shell discovery requires loop owner");
  if(!services.isExecutable||!services.which||!services.debug||!services.error)
    throw std::invalid_argument("shell discovery imports required");
  event_loop::Deferred<std::string> result(loop);
  try{
    if(services.shellOverride&&!services.shellOverride->empty()){
      const auto& override=*services.shellOverride;
      if(supported(override)&&services.isExecutable(override)){
        services.debug("Using shell override: "+override);
        result.resolve(override);return result;
      }
      services.debug("BATBOX_SHELL=\""+override+
        "\" is not a valid bash/zsh path, falling back to detection");
    }
    // Start both lookups in order, then resume the asynchronous
    // function when both have settled. Selection retains its exact unshift/
    // push order, including the fallback locations and SHELL preference.
    const auto zsh=services.which("zsh");
    const auto bash=services.which("bash");
    struct Wait {
      bool zshDone=false,bashDone=false,failed=false;
      std::optional<std::string> zsh,bash;
    };
    auto state=std::make_shared<Wait>();
    auto resume=[&loop,services,state,result]{
      if(state->failed||!state->zshDone||!state->bashDone)return;
      loop.queueMicrotask([services,state,result]{
        try{
          const auto preferred=services.preferredShell.value_or("");
          const bool preferBash=preferred.find("bash")!=std::string::npos;
          const bool envSupported=supported(preferred);
          const std::vector<std::string> locations={"/bin","/usr/bin","/usr/local/bin","/opt/homebrew/bin"};
          std::vector<std::string> candidates;
          const auto names=preferBash?std::vector<std::string>{"bash","zsh"}:
            std::vector<std::string>{"zsh","bash"};
          for(const auto& name:names)for(const auto& location:locations)
            candidates.push_back(location+"/"+name);
          if(preferBash){
            if(state->bash)candidates.insert(candidates.begin(),*state->bash);
            if(state->zsh)candidates.push_back(*state->zsh);
          }else{
            if(state->zsh)candidates.insert(candidates.begin(),*state->zsh);
            if(state->bash)candidates.push_back(*state->bash);
          }
          if(envSupported&&services.isExecutable(preferred))
            candidates.insert(candidates.begin(),preferred);
          for(const auto& candidate:candidates){
            if(!candidate.empty()&&services.isExecutable(candidate)){
              result.resolve(candidate);return;
            }
          }
          const auto message="No suitable shell found. Batbox CLI requires a Posix shell environment. "
            "Please ensure you have a valid shell installed and the SHELL environment variable set.";
          services.error(message);result.reject(std::make_exception_ptr(std::runtime_error(message)));
        }catch(...){result.reject(std::current_exception());}
      });
    };
    zsh.then([state,resume,result](const std::optional<std::string>* value,
      const std::exception_ptr& error){
      if(error){state->failed=true;result.reject(error);return;}
      state->zsh=*value;state->zshDone=true;resume();
    });
    bash.then([state,resume,result](const std::optional<std::string>* value,
      const std::exception_ptr& error){
      if(error){state->failed=true;result.reject(error);return;}
      state->bash=*value;state->bashDone=true;resume();
    });
  }catch(...){result.reject(std::current_exception());}
  return result;
}
} // namespace batbox::shell
