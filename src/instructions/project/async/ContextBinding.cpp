#include "instructions/project/async/ContextBinding.hpp"
#include <utility>

namespace batbox::instructions::project::async {
void bindMemoryContextServices(context::AsyncServices& context,
  MemoryDiscoveryStash& stash,Services services){
  context.getMemoryFiles=[&stash,services=std::move(services)]{
    return stash.getMemoryFiles(services);
  };
}
} // namespace batbox::instructions::project::async
