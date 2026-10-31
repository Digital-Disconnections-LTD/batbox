#include "flight_restore/IndigenousBinding.hpp"
#include "cwd/Override.hpp"
#include "cwd/SetCwd.hpp"
#include <filesystem>

namespace batbox::flight_restore {
void bindIndigenousComponents(Services& services,const IndigenousComponents& parts){
  if(parts.project){
    services.resetFlightFilePointer=[project=parts.project,loop=services.loop]{
      project->resetFlightFile();
      event_loop::Deferred<event_loop::Unit> done(*loop);
      done.resolve(event_loop::Unit{});return done;
    };
    services.recordContentReplacement=[project=parts.project,loop=services.loop](const Json& replacements){
      event_loop::Deferred<event_loop::Unit> done(*loop);
      project->insertContentReplacement(replacements,std::nullopt).then(
        [done](const event_loop::Unit*,const std::exception_ptr& error){
          if(error)done.reject(error);else done.resolve(event_loop::Unit{});
        });
      return done;
    };
    services.adoptResumedFlightFile=[project=parts.project]{project->adoptResumedFlightFile();};
  }
  if(parts.metadata){
    services.restoreFlightMetadata=[metadata=parts.metadata](const Json& value){
      metadata->restoreFlightMetadata(value);};
    services.saveMode=[metadata=parts.metadata](const std::string& mode){
      metadata->saveMode(mode);};
    services.saveWorktreeState=[metadata=parts.metadata](const Json& worktree){
      metadata->saveWorktreeState(worktree);};
  }
  if(parts.concurrent){
    services.updateFlightName=[roster=parts.concurrent](std::optional<std::string> name){
      (void)roster->updateFlightName(std::move(name));};
  }
  if(parts.memoryStash){
    services.clearMemoryFileStashes=[stash=parts.memoryStash]{stash->clearMemoryFileStashes();};
  }
  if(parts.poetrySections){
    services.clearSystemPoetrySections=[stash=parts.poetrySections,
      clear=parts.clearBetaHeaderLatches]{stash->clearSystemPoetrySections(clear);};
  }
  if(parts.cwdState){
    services.setCwd=[state=parts.cwdState](const std::string& path){
      cwd::setCwd(*state,path,std::nullopt,cwd::indigenousSetCwdServices());};
    services.setOriginalCwd=[state=parts.cwdState](const std::string& path){
      cwd::setOriginalCwd(*state,path);};
    services.getCwd=[state=parts.cwdState]{return cwd::getCwd(*state);};
  }
  if(parts.worktreeState){
    services.getCurrentWorktreeFlight=[state=parts.worktreeState]{
      return getCurrentWorktreeFlight(*state);};
    services.restoreWorktreeFlight=[state=parts.worktreeState](const Json& flight){
      restoreWorktreeFlight(*state,flight);};
  }
  services.chdir=[](const std::string& path){std::filesystem::current_path(path);};
  services.dirname=[](const std::string& path){
    return std::filesystem::path(path).parent_path().string();};
}
} // namespace batbox::flight_restore
