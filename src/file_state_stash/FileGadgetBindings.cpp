#include "file_state_stash/Bindings.hpp"
#include "gadgets/files/FileRead.hpp"
#include "gadgets/files/FileWrite.hpp"
#include <stdexcept>

namespace batbox::file_state_stash {
void bindReadStash(gadgets::files::FileReadServices& services,
  std::shared_ptr<FileStateStash> stash){
  if(!stash)throw std::invalid_argument("Read cache binding requires cache");
  services.readFileStateOwner=std::move(stash);
  services.readFileState=services.readFileStateOwner.get();
}
void bindWriteStash(gadgets::files::FileWriteServices& services,
  std::shared_ptr<FileStateStash> stash){
  if(!stash)throw std::invalid_argument("Write cache binding requires cache");
  services.readFileStateOwner=std::move(stash);
  services.readFileState=services.readFileStateOwner.get();
}
} // namespace batbox::file_state_stash
