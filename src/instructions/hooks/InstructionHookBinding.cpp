#include "instructions/hooks/InstructionsLoaded.hpp"
#include <utility>

namespace batbox::instructions::hooks {
void bindInstructionsLoaded(event_loop::Loop& loop,
  project::DiscoveryServices& discovery,InstructionsLoadedImports imports){
  discovery.hasInstructionsLoadedHook=[imports]{return hasInstructionsLoadedHook(imports);};
  discovery.instructionsLoadedHook=[&loop,imports=std::move(imports)](
    const project::MemoryFileInfo& file,const std::string& reason){
    InstructionsLoadedOptions options;
    options.globs=file.globs;
    options.parentFilePath=file.parent;
    // Launch the hook operation without awaiting its result.
    (void)executeInstructionsLoadedHooks(loop,imports,file.path,file.type,reason,options);
  };
}
} // namespace batbox::instructions::hooks
