#include "file_state_stash/Bindings.hpp"
#include "query_engine/Ask.hpp"
#include <stdexcept>

namespace batbox::file_state_stash {
void bindAskStash(query_engine::AskServices& services){
  services.cloneFileStateStash=[](const query_engine::FileStateStashRef& stash){
    if(!stash)throw std::invalid_argument("Ask cache clone requires cache");
    return cloneFileStateStash(*stash);
  };
}
} // namespace batbox::file_state_stash
