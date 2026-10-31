#include "interactive/message_view/RetainedViews.hpp"
#include <stdexcept>

namespace batbox::interactive::message_view {
RetainedViews::RetainedViews(event_loop::Loop& loop,std::function<double()> nowMs,
    std::function<void()> requestRedraw):loop_(loop),nowMs_(std::move(nowMs)),
    requestRedraw_(std::move(requestRedraw)){
  if(!nowMs_||!requestRedraw_)throw std::invalid_argument("Retained view clock/redraw imports required");
}
std::shared_ptr<RetainedViews> RetainedViews::create(event_loop::Loop& loop,
    std::function<double()> nowMs,std::function<void()> requestRedraw){
  return std::shared_ptr<RetainedViews>(new RetainedViews(loop,std::move(nowMs),std::move(requestRedraw)));
}
RetainedViews::~RetainedViews(){unmountAll();}
std::string RetainedViews::key(const MessagePlan& message,const BlockPlan* block)const{
  auto value=message.item.message->at("uuid").get<std::string>()+"-"+conversationId_;
  if(block)value+=':'+std::to_string(block->original.index);
  return value;
}
void RetainedViews::beginFrame(std::string conversationId){
  conversationId_=std::move(conversationId);effects_.clear();
  activeSuccesses_.clear();activeCollapsed_.clear();
}
ProjectionServices RetainedViews::bind(ProjectionServices base,RetainedViewServices services){
  auto self=shared_from_this();auto resultFactory=base.gadgetResult;
  base.gadgetResult=[self,resultFactory,providers=services.result](const MessagePlan& message,
      const BlockPlan& block,const MessageLookups& lookups,const gadgets::Gadgets& gadgets){
    if(!resultFactory)throw std::invalid_argument("Gadget result protocol/view imports required");
    cards_gadget_binding::GadgetChildBindingInputs input;
    input.stableCardKey=self->key(message,&block);input.userMessage=*message.item.message;
    for(const auto& progress:message.options.progressMessages)input.progressMessages.push_back(*progress);
    input.lookups=&lookups;input.gadgets=&gadgets;input.terminalColumns=message.options.columns;
    if(message.options.style.is_string())input.style=message.options.style.get<std::string>();
    input.verbose=block.verbose;input.transcriptMode=block.transcriptMode;
    auto binding=cards_gadget_binding::bindGadgetResultChildren(self->successes_,input,providers,
      resultFactory(message,block,lookups,gadgets));
    auto renderSuccess=binding.imports.renderSuccess;
    binding.imports.renderSuccess=[self,stableKey=input.stableCardKey,renderSuccess](
        const cards_gadget_result::GadgetResultCard& route,const Json& param){
      self->activeSuccesses_.insert(stableKey);return renderSuccess(route,param);
    };
    self->effects_.push_back(std::move(binding.committed));
    return binding.imports;
  };
  base.collapsed=[self,services=std::move(services)](const MessagePlan& message,
      const MessageLookups& lookups,const gadgets::Gadgets& gadgets){
    const auto stableKey=self->key(message);self->activeCollapsed_.insert(stableKey);
    auto& retained=self->collapsed_[stableKey];
    if(!retained){
      retained=std::make_shared<CollapsedEntry>();
      retained->hint=cards_collapsed::MinHintDisplay::create(self->loop_,self->nowMs_,self->requestRedraw_);
    }
    auto imports=services.collapsed;auto hint=retained->hint;
    imports.minimumDisplayTime=[self,hint](const std::optional<std::string>& value,int minimum){
      self->effects_.push_back([hint,value,minimum]{hint->committed(value,minimum);});
      return hint->render(value,minimum);
    };
    cards_collapsed::CollapsedInputs input;
    input.message=materializeRenderItem(message.item);input.inProgressGadgetUseIDs=&message.options.inProgressGadgetUseIDs;
    input.shouldAnimate=message.options.shouldAnimate;
    input.verbose=message.options.verbose||message.options.transcriptMode;
    input.active=message.options.isActiveCollapsedGroup;input.gadgets=&gadgets;input.lookups=&lookups;
    const auto card=cards_collapsed::classifyCollapsedReadSearch(retained->state,input,imports);
    return cards_collapsed::projectCollapsedWithIndigenousGadgets(card,gadgets,lookups,
      message.options.inProgressGadgetUseIDs,services.verboseGadgets,
      services.verboseProjection,services.collapsedProjection);
  };
  return base;
}
void RetainedViews::committed(){
  auto effects=std::move(effects_);effects_.clear();
  for(const auto& effect:effects)effect();
  for(const auto& key:previousSuccesses_)if(!activeSuccesses_.contains(key))successes_.unmount(key);
  previousSuccesses_=activeSuccesses_;
  for(auto it=collapsed_.begin();it!=collapsed_.end();)
    if(!activeCollapsed_.contains(it->first)){it->second->hint->cancel();it=collapsed_.erase(it);}else ++it;
}
void RetainedViews::unmountAll(){
  effects_.clear();
  for(auto& [key,value]:collapsed_)value->hint->cancel();
  collapsed_.clear();
  for(const auto& key:previousSuccesses_)successes_.unmount(key);
  for(const auto& key:activeSuccesses_)successes_.unmount(key);
  previousSuccesses_.clear();activeSuccesses_.clear();activeCollapsed_.clear();
}
} // namespace batbox::interactive::message_view
