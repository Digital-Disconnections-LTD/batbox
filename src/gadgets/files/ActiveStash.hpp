#pragma once
#include "bats/ForkedBat.hpp"
#include "gadgets/files/FileRead.hpp"

namespace batbox::gadgets::files {
template<class Services>Services withActiveStash(const Services& source,
  const GadgetCallContext& context){
  Services active=source;
  auto stash=context.readFileState;
  if(!stash&&context.batContext)
    stash=context.batContext->query.readFileState;
  if(stash){
    active.readFileStateOwner=std::move(stash);
    active.readFileState=active.readFileStateOwner.get();
  }
  auto dynamic=context.dynamicSkillDirTriggers;
  if(!dynamic&&context.batContext)
    dynamic=context.batContext->query.dynamicSkillDirTriggers;
  active.activeDynamicSkillDirTriggers=std::move(dynamic);
  if constexpr(requires{active.activeNestedMemoryAttachmentTriggers;}){
    auto nested=context.nestedMemoryAttachmentTriggers;
    if(!nested&&context.batContext)
      nested=context.batContext->query.nestedMemoryAttachmentTriggers;
    active.activeNestedMemoryAttachmentTriggers=std::move(nested);
  }
  return active;
}
} // namespace batbox::gadgets::files
