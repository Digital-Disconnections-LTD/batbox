#include "instructions/project/async/MemoryRun.hpp"
#include <stdexcept>

namespace batbox::instructions::project::async {
void MemoryRun::autoMemory(){
  try{
    if(!services.sync.autoMemoryEnabled)
      throw std::invalid_argument("Auto-memory feature import required");
    if(!services.sync.autoMemoryEnabled()){finish();return;}
    if(!services.sync.autoMemoryEntrypoint)
      throw std::invalid_argument("Auto-memory path import required");
    safelyReadMemoryFile(loop,services.sync.autoMemoryEntrypoint(),"AutoMem",
      std::nullopt,services).then([self=shared_from_this()](
      const ParsedMemoryContent* parsed,const std::exception_ptr& error){
      if(error){self->fail(error);return;}
      try{
        if(parsed->info){
          const auto normalized=normalizePathForComparison(parsed->info->path,
            self->services.sync);
          if(self->processed->insert(normalized).second)
            self->result.push_back(*parsed->info);
        }
        self->finish();
      }catch(...){self->fail(std::current_exception());}
    });
  }catch(...){fail(std::current_exception());}
}
} // namespace batbox::instructions::project::async
