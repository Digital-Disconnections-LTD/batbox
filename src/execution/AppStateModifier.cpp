#include "execution/GadgetExecution.hpp"

namespace batbox::execution {
Json effectiveAppState(const Context& context,const Json& base){
  return context.appStateModifier?context.appStateModifier(base):base;
}
Context contextWithoutConversation(const Context& context){
  Context copy;
  if(context.data.is_object()){
    for(auto it=context.data.begin();it!=context.data.end();++it)
      if(it.key()!="messages")copy.data[it.key()]=it.value();
  }else copy.data=context.data;
  copy.gadgets=context.gadgets;
  copy.baseGadgets=context.baseGadgets;
  copy.gadgetSnapshotOwner=context.gadgetSnapshotOwner;
  copy.stop=context.stop;
  copy.abortReason=context.abortReason;
  copy.appStateModifier=context.appStateModifier;
  copy.typedAppStateModifier=context.typedAppStateModifier;
  copy.typedAppStateModifierComplete=context.typedAppStateModifierComplete;
  copy.contentReplacementState=context.contentReplacementState;
  copy.readFileState=context.readFileState;
  copy.nestedMemoryAttachmentTriggers=context.nestedMemoryAttachmentTriggers;
  copy.loadedNestedMemoryPaths=context.loadedNestedMemoryPaths;
  copy.dynamicSkillDirTriggers=context.dynamicSkillDirTriggers;
  copy.discoveredSkillNames=context.discoveredSkillNames;
  return copy;
}
void appendAppStateModifier(Context& context,
    const std::function<Json(const Json&)>& next){
  if(!next)return;
  context.typedAppStateModifierComplete=false;
  if(!context.appStateModifier){context.appStateModifier=next;return;}
  auto previous=context.appStateModifier;
  context.appStateModifier=[previous=std::move(previous),next](const Json& base){
    return next(previous(base));
  };
}
} // namespace batbox::execution
