#include "flight_restore/FlightRestore.hpp"
#include <stdexcept>

namespace batbox::flight_restore {
namespace {
void require(bool ready,const char* name){if(!ready)throw std::invalid_argument(std::string("Missing session restore import: ")+name);}
void clearStashes(Services& services){
  require(static_cast<bool>(services.clearMemoryFileStashes),"clearMemoryFileCaches");
  require(static_cast<bool>(services.clearSystemPoetrySections),"clearSystemPromptSections");
  services.clearMemoryFileStashes();
  services.clearSystemPoetrySections();
  if(services.clearPlansDirectoryStash)services.clearPlansDirectoryStash();
}
}

void restoreWorktreeForResume(const Json& worktreeFlight,Services& services){
  require(static_cast<bool>(services.getCurrentWorktreeFlight),"getCurrentWorktreeSession");
  const Json fresh=services.getCurrentWorktreeFlight();
  if(value::truthy(fresh)){
    require(static_cast<bool>(services.saveWorktreeState),"saveWorktreeState");
    services.saveWorktreeState(fresh);return;
  }
  if(!value::truthy(worktreeFlight))return;
  const std::string path=value::property(worktreeFlight,"worktreePath").get<std::string>();
  require(static_cast<bool>(services.chdir),"chdir");
  try{services.chdir(path);}catch(...){
    require(static_cast<bool>(services.saveWorktreeState),"saveWorktreeState");
    services.saveWorktreeState(nullptr);return;
  }
  require(static_cast<bool>(services.setCwd),"setCwd");
  require(static_cast<bool>(services.setOriginalCwd),"setOriginalCwd");
  require(static_cast<bool>(services.getCwd),"getCwd");
  require(static_cast<bool>(services.restoreWorktreeFlight),"restoreWorktreeSession");
  services.setCwd(path);
  services.setOriginalCwd(services.getCwd());
  services.restoreWorktreeFlight(worktreeFlight);
  clearStashes(services);
}

void exitRestoredWorktree(Services& services){
  require(static_cast<bool>(services.getCurrentWorktreeFlight),"getCurrentWorktreeSession");
  const Json current=services.getCurrentWorktreeFlight();
  if(!value::truthy(current))return;
  require(static_cast<bool>(services.restoreWorktreeFlight),"restoreWorktreeSession");
  services.restoreWorktreeFlight(nullptr);
  clearStashes(services);
  require(static_cast<bool>(services.chdir),"chdir");
  const std::string original=value::property(current,"originalCwd").get<std::string>();
  try{services.chdir(original);}catch(...){return;}
  require(static_cast<bool>(services.setCwd),"setCwd");
  require(static_cast<bool>(services.setOriginalCwd),"setOriginalCwd");
  require(static_cast<bool>(services.getCwd),"getCwd");
  services.setCwd(original);
  services.setOriginalCwd(services.getCwd());
}
} // namespace batbox::flight_restore
