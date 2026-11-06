#include "application/HostShellSnapshot.hpp"
#include "runtime/IndigenousPath.hpp"
#include "runtime/Platform.hpp"
#include "shell/SnapshotRun.hpp"
#include <cstdlib>
#include <map>
#include <utility>
#ifdef _WIN32
#include "runtime/IndigenousUnicode.hpp"
#include <cwchar>
#include <windows.h>
#else
extern char** environ;
#endif

namespace batbox::application {
namespace {
std::map<std::string,std::string> processEnvironment(){
  std::map<std::string,std::string> values;
#ifdef _WIN32
  auto* block=GetEnvironmentStringsW();
  if(!block)return values;
  for(const wchar_t* item=block;*item;item+=std::wcslen(item)+1){
    const std::wstring text(item);
    if(text.empty()||text.front()==L'=')continue;
    const auto equal=text.find(L'=');
    if(equal!=std::wstring::npos)
      values[runtime::wideToUtf8(text.substr(0,equal))]=runtime::wideToUtf8(text.substr(equal+1));
  }
  FreeEnvironmentStringsW(block);
#else
  for(char** item=environ;item&&*item;++item){
    const std::string text=*item;const auto equal=text.find('=');
    if(equal!=std::string::npos)values[text.substr(0,equal)]=text.substr(equal+1);
  }
#endif
  return values;
}
std::string variable(const char* name){
  const auto values=processEnvironment();
  const auto found=values.find(name);
  return found==values.end()?std::string():found->second;
}
}
void addExitStep(ExitSteps& exit,std::function<void()> step){
  std::lock_guard lock(exit.mutex);
  exit.steps.push_back(std::move(step));
}
void runExitSteps(ExitSteps& exit){
  std::vector<std::function<void()>> steps;
  {std::lock_guard lock(exit.mutex);steps.swap(exit.steps);}
  for(auto& step:steps)try{step();}catch(...){}
}
std::function<std::shared_future<std::optional<std::string>>(const std::string&)>
    makeShellSnapshot(std::filesystem::path stateDirectory,std::shared_ptr<ExitSteps> exit,
      std::function<void(const std::string&)> log){
  struct Started {
    std::once_flag once;
    std::shared_future<std::optional<std::string>> result;
  };
  auto started=std::make_shared<Started>();
  return [started,stateDirectory=std::move(stateDirectory),exit=std::move(exit),
      log=std::move(log)](const std::string& shellPath){
    std::call_once(started->once,[&]{
      shell_snapshot::RunServices services;
      services.content.ripgrepCommand=[]{return shell_snapshot::RipgrepCommand{"rg",{},{}};};
      services.content.hasEmbeddedSearchGadgets=[]{return false;};
      services.content.embeddedSearchGadgetsBinaryPath=[]{return std::string();};
      services.content.pathValue=[]{return variable("PATH");};
      services.content.cygwinPathValue=[]()->std::optional<std::string>{return std::nullopt;};
#ifdef _WIN32
      services.content.windows=true;
#endif
      services.configHome=[directory=runtime::pathToUtf8(stateDirectory)]{return directory;};
      services.homeDirectory=[]{
        auto home=variable("HOME");
        return home.empty()?variable("USERPROFILE"):home;};
      services.nowMillis=[]{return static_cast<std::int64_t>(platform::nowMillis());};
      services.randomId=[]{return platform::randomUUID().substr(0,8);};
      services.subprocessEnvironment=processEnvironment;
      services.registerCleanup=[exit](std::function<void()> step){
        addExitStep(*exit,std::move(step));};
      services.diagnostic=log;
      started->result=std::async(std::launch::async,[shellPath,services,log]{
        auto path=shell_snapshot::createAndSaveSnapshot(shellPath,services);
        if(path)log("Shell snapshot ready: "+*path);
        return path;
      }).share();
      // An exit while the snapshot is still being written waits for it, so
      // the file is removed either way.
      addExitStep(*exit,[result=started->result]{
        if(const auto path=result.get()){std::error_code ignored;
          std::filesystem::remove(runtime::pathFromUtf8(*path),ignored);}});
    });
    return started->result;
  };
}
}
