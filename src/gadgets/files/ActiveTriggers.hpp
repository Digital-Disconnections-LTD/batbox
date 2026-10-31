#pragma once
#include "gadgets/files/FileRead.hpp"
#include "gadgets/files/FileWrite.hpp"

namespace batbox::gadgets::files {
inline void recordNestedMemoryTrigger(FileReadServices& services,
  const std::string& path){
  if(services.activeNestedMemoryAttachmentTriggers)
    services.activeNestedMemoryAttachmentTriggers->insert(path);
  else if(services.nestedMemoryAttachmentTriggers)
    services.nestedMemoryAttachmentTriggers->insert(path);
}
template<class Services>void recordDynamicSkillDir(Services& services,
  const std::string& dir){
  if(services.activeDynamicSkillDirTriggers)
    services.activeDynamicSkillDirTriggers->insert(dir);
  else if(services.dynamicSkillDirTriggers)
    services.dynamicSkillDirTriggers->insert(dir);
}
} // namespace batbox::gadgets::files
