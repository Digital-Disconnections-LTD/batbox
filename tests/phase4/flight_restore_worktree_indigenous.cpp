#include <unistd.h>
#include "flight_restore/IndigenousBinding.hpp"
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <cstdlib>

using namespace batbox;
using namespace batbox::flight_restore;
namespace fs=std::filesystem;

int main(){
  const fs::path prior=fs::current_path();
  std::string name=(fs::canonical(fs::temp_directory_path())/"batbox-session-restore-native-XXXXXX").string();
  const char* created=::mkdtemp(name.data());
  if(!created)throw std::runtime_error("mkdtemp failed");
  const fs::path root=fs::canonical(created);
  const fs::path base=root/"base";
  const fs::path tree=root/"tree";
  fs::create_directories(base);
  fs::create_directories(tree);
  auto cleanup=[&]{fs::current_path(prior);fs::remove_all(root);};
  try{
    fs::current_path(base);
    auto cwdState=std::make_shared<cwd::State>();
    cwdState->cwd=base.string();
    cwdState->originalCwd=base.string();
    cwdState->projectRoot=base.string();
    auto worktreeState=std::make_shared<WorktreeFlightState>();
    auto flightState=std::make_shared<flights::FlightState>();
    flights::MetadataServices metadataServices;
    metadataServices.project=flightState.get();
    metadataServices.getFlightId=[]{return "resume-session";};
    auto metadata=std::make_shared<flights::MetadataOperations>(metadataServices);
    Services services;
    IndigenousComponents parts;
    parts.metadata=metadata;
    parts.cwdState=cwdState;
    parts.worktreeState=worktreeState;
    bindIndigenousComponents(services,parts);
    int stashClears=0;
    services.clearMemoryFileStashes=[&]{++stashClears;};
    services.clearSystemPoetrySections=[&]{++stashClears;};
    services.clearPlansDirectoryStash=[&]{++stashClears;};
    const Json resumed={{"worktreePath",tree.string()},
      {"originalCwd",base.string()},{"worktreeName","tree"}};
    services.restoreFlightMetadata(Json{{"worktreeSession",resumed},
      {"agentName","resumed-agent"}});
    if(flightState->currentFlightWorktree!=resumed||
       flightState->currentFlightBatName!="resumed-agent")
      throw std::runtime_error("native metadata restore mismatch");
    restoreWorktreeForResume(resumed,services);
    if(fs::current_path()!=tree||cwdState->cwd!=tree.string()||
       cwdState->originalCwd!=tree.string()||cwdState->projectRoot!=base.string()||
       getCurrentWorktreeFlight(*worktreeState)!=resumed||stashClears!=3)
      throw std::runtime_error("native worktree resume state mismatch");
    exitRestoredWorktree(services);
    if(fs::current_path()!=base||cwdState->cwd!=base.string()||
       cwdState->originalCwd!=base.string()||
       !getCurrentWorktreeFlight(*worktreeState).is_null()||stashClears!=6)
      throw std::runtime_error("native worktree exit state mismatch");

    restoreWorktreeFlight(*worktreeState,resumed);
    const Json stale={{"worktreePath","/stale"},{"originalCwd","/stale"}};
    restoreWorktreeForResume(stale,services);
    if(!flightState->currentFlightWorktree||
       nlohmann::json(*flightState->currentFlightWorktree)!=nlohmann::json(resumed)||
       fs::current_path()!=base||stashClears!=6)
      throw std::runtime_error("fresh worktree precedence mismatch");

    restoreWorktreeFlight(*worktreeState,nullptr);
    restoreWorktreeForResume(stale,services);
    if(!flightState->currentFlightWorktree||
       !flightState->currentFlightWorktree->is_null()||
       fs::current_path()!=base||stashClears!=6)
      throw std::runtime_error("missing worktree recovery mismatch");
    cleanup();
    std::cout<<"session_restore native worktree: passed\n";
  }catch(...){cleanup();throw;}
}
