#include "query/AsyncGadgetTurnInternal.hpp"
#include "query/MemoryAttachmentFilter.hpp"

namespace batbox::query::async_gadget_turn {
void Run::startAttachments(){
  bool sleepRan=false;
  for(const Json& block:modelResult->gadgetUseBlocks)
    if(field(block,"name")==services.sync.sleepGadgetName){sleepRan=true;break;}
  const bool mainThread=params.querySource.starts_with("repl_main_thread")||
    params.querySource=="sdk";
  const Json& batId=field(prepared->gadgetUseContext.data,"agentId");
  need(services.sync.getCommandsByMaxPriority,"getCommandsByMaxPriority");
  need(services.sync.isSlashCommand,"isSlashCommand");
  const Json queued=services.sync.getCommandsByMaxPriority(sleepRan?"later":"next");
  queuedCommands=Json::array();
  if(queued.is_array())for(const Json& command:queued){
    if(services.sync.isSlashCommand(command))continue;
    if(mainThread){
      if(!command.is_object()||!command.contains("agentId"))
        queuedCommands.push_back(command);
    }else if(field(command,"mode")=="task-notification"&&
             field(command,"agentId")==batId)queuedCommands.push_back(command);
  }
  Json history=appendMessages(prepared->messagesForQuery,
    modelResult->assistantMessages);
  history=appendMessages(history,modelResult->gadgetResults);
  need(services.getAttachmentMessages,"getAttachmentMessages");
  attachmentCursor=services.getAttachmentMessages(queuedCommands,updatedContext,
    history,params.querySource);
  need(attachmentCursor.next,"getAttachmentMessages cursor.next");
  pullAttachment();
}
void Run::pullAttachment(){
  auto pending=attachmentCursor.next();
  if(!loop.dispatcher().sameLoop(pending.dispatcher()))
    throw std::invalid_argument("Async attachments belong to another event loop");
  stage=Stage::AttachmentAwait;
  auto self=shared_from_this();
  pending.then([self](const std::optional<model::MessageRef>* value,
      const std::exception_ptr& error){
    if(self->stage==Stage::Done)return;
    if(error){self->fail(error);return;}
    try{
      if(!*value){self->stage=Stage::MemoryStart;self->pump();return;}
      self->currentAttachment=**value;
      self->emit(self->currentAttachment,Stage::AfterAttachmentYield);
    }catch(...){self->fail(std::current_exception());}
  });
}
void Run::afterAttachmentYield(){
  modelResult->gadgetResults.push_back(*currentAttachment);
  currentAttachment.reset();pullAttachment();
}
void Run::startMemory(){
  if(!memoryPrefetch||memoryPrefetch->consumedOnIteration!=-1||
     !memoryPrefetch->isSettled||!memoryPrefetch->isSettled()){
    stage=Stage::SkillStart;return;
  }
  need(services.readMemoryPrefetch,"readMemoryPrefetch");
  auto pending=services.readMemoryPrefetch(*memoryPrefetch);
  if(!loop.dispatcher().sameLoop(pending.dispatcher()))
    throw std::invalid_argument("Async memory prefetch belongs to another event loop");
  stage=Stage::MemoryAwait;
  auto self=shared_from_this();
  pending.then([self](const Json* value,const std::exception_ptr& error){
    if(self->stage==Stage::Done)return;
    if(error){self->fail(error);return;}
    try{
      if(self->prepared->gadgetUseContext.readFileState)
        self->memoryAttachments=filterDuplicateMemoryAttachments(*value,
          *self->prepared->gadgetUseContext.readFileState);
      else{
        need(self->services.sync.filterDuplicateMemoryAttachments,
          "filterDuplicateMemoryAttachments");
        self->memoryAttachments=self->services.sync.filterDuplicateMemoryAttachments(
          *value,field(self->prepared->gadgetUseContext.data,"readFileState"));
      }
      if(!self->memoryAttachments.is_array())
        throw std::invalid_argument("Memory attachments must be an array");
      self->attachmentIndex=0;self->stage=Stage::MemoryYield;self->pump();
    }catch(...){self->fail(std::current_exception());}
  });
}
void Run::yieldMemory(){
  if(attachmentIndex<memoryAttachments.size()){
    need(services.sync.createAttachmentMessage,"createAttachmentMessage");
    currentAttachment=std::make_shared<Json>(services.sync.createAttachmentMessage(
      memoryAttachments[attachmentIndex++]));
    emit(currentAttachment,Stage::AfterMemoryYield);
  }else{
    memoryPrefetch->consumedOnIteration=state.turnCount-1;
    stage=Stage::SkillStart;
  }
}
void Run::afterMemoryYield(){
  modelResult->gadgetResults.push_back(*currentAttachment);
  currentAttachment.reset();stage=Stage::MemoryYield;
}
void Run::startSkills(){
  need(services.sync.feature,"feature");
  if(!services.sync.feature("EXPERIMENTAL_SKILL_SEARCH")||
     !value::truthy(prepared->pendingSkillPrefetch)){
    stage=Stage::Commands;return;
  }
  need(services.collectSkillDiscoveryPrefetch,"collectSkillDiscoveryPrefetch");
  auto pending=services.collectSkillDiscoveryPrefetch(prepared->pendingSkillPrefetch);
  if(!loop.dispatcher().sameLoop(pending.dispatcher()))
    throw std::invalid_argument("Async skill prefetch belongs to another event loop");
  stage=Stage::SkillAwait;
  auto self=shared_from_this();
  pending.then([self](const Json* value,const std::exception_ptr& error){
    if(self->stage==Stage::Done)return;
    if(error){self->fail(error);return;}
    try{
      self->skillAttachments=*value;
      if(!self->skillAttachments.is_array())
        throw std::invalid_argument("Skill attachments must be an array");
      self->attachmentIndex=0;self->stage=Stage::SkillYield;self->pump();
    }catch(...){self->fail(std::current_exception());}
  });
}
void Run::yieldSkills(){
  if(attachmentIndex<skillAttachments.size()){
    need(services.sync.createAttachmentMessage,"createAttachmentMessage");
    currentAttachment=std::make_shared<Json>(services.sync.createAttachmentMessage(
      skillAttachments[attachmentIndex++]));
    emit(currentAttachment,Stage::AfterSkillYield);
  }else stage=Stage::Commands;
}
void Run::afterSkillYield(){
  modelResult->gadgetResults.push_back(*currentAttachment);
  currentAttachment.reset();stage=Stage::SkillYield;
}
} // namespace batbox::query::async_gadget_turn
