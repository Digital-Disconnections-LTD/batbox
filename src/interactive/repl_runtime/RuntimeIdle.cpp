#include "interactive/repl_runtime/Runtime.hpp"
#include <algorithm>
#include <stdexcept>

namespace batbox::interactive::repl_runtime {
namespace {
template<class T>void need(const T& value,const char* name){
  if(!value)throw std::invalid_argument(std::string("REPL idle binding required: ")+name);
}
}
void Runtime::cancelIdleEffects(){
  idlePoetryTimer_.cancel();
  idleReturnTimer_.cancel();
  idlePoetryTimer_={};
  idleReturnTimer_={};
  if(idleReturnEffectActive_){
    idleReturnEffectActive_=false;
    if(idle_.removeIdleReturnHint)idle_.removeIdleReturnHint();
  }
}
void Runtime::observeIdleEffects(double lastQueryCompletionTime,bool isLoading,
    bool hasLocalUi,std::size_t submitCount){
  if(!services_.owner.isOwnerThread())
    throw std::logic_error("Idle effects owner required");
  const IdleSnapshot next{lastQueryCompletionTime,isLoading,
    hasLocalUi,submitCount};
  const bool poetryChanged=!idlePoetrySnapshot_||
    idlePoetrySnapshot_->completion!=next.completion||
    idlePoetrySnapshot_->loading!=next.loading||
    idlePoetrySnapshot_->localUi!=next.localUi||
    idlePoetrySnapshot_->submitCount!=next.submitCount;
  if(poetryChanged){
    idlePoetrySnapshot_=next;
    idlePoetryTimer_.cancel();
    idlePoetryTimer_={};
    if(!isLoading&&submitCount>0&&lastQueryCompletionTime!=0){
      need(idle_.messageIdleThresholdMs,"messageIdleThresholdMs");
      const double delay=idle_.messageIdleThresholdMs();
      auto weak=weak_from_this();
      idlePoetryTimer_=services_.owner.setTimeout(delay,
        [weak,lastQueryCompletionTime,isLoading,hasLocalUi]{
          if(auto self=weak.lock()){
            try{
              const auto& idle=self->idle_;
              need(idle.getLastInteractionTime,"getLastInteractionTime");
              if(idle.getLastInteractionTime()>lastQueryCompletionTime)return;
              need(idle.nowMs,"nowMs");
              need(idle.messageIdleThresholdMs,"messageIdleThresholdMs");
              need(idle.focusedDialogPresent,"focusedDialogPresent");
              if(!isLoading&&!hasLocalUi&&!idle.focusedDialogPresent()&&
                  idle.nowMs()-lastQueryCompletionTime>=
                    idle.messageIdleThresholdMs()){
                need(idle.sendIdlePoetry,"sendIdlePrompt");
                idle.sendIdlePoetry("Batbox is waiting for your input",
                  "idle_prompt");
              }
            }catch(...){self->reportDetached(std::current_exception());}
          }
        });
    }
  }
  const bool returnChanged=!idleReturnSnapshot_||
    idleReturnSnapshot_->completion!=next.completion||
    idleReturnSnapshot_->loading!=next.loading;
  if(!returnChanged)return;
  idleReturnSnapshot_=next;
  idleReturnTimer_.cancel();
  idleReturnTimer_={};
  if(idleReturnEffectActive_){
    idleReturnEffectActive_=false;
    if(idle_.removeIdleReturnHint)idle_.removeIdleReturnHint();
  }
  if(lastQueryCompletionTime==0||isLoading)return;
  need(idle_.idleReturnMode,"idleReturnMode");
  const auto mode=idle_.idleReturnMode();
  if(mode!="hint"&&mode!="hint_v2")return;
  need(idle_.idleReturnDismissed,"idleReturnDismissed");
  if(idle_.idleReturnDismissed())return;
  need(idle_.getTotalInputTokens,"getTotalInputTokens");
  need(idle_.idleTokenThreshold,"idleTokenThreshold");
  if(idle_.getTotalInputTokens()<idle_.idleTokenThreshold())return;
  need(idle_.idleMinutesThreshold,"idleMinutesThreshold");
  need(idle_.nowMs,"nowMs");
  const double threshold=idle_.idleMinutesThreshold()*60000;
  const double remaining=threshold-(idle_.nowMs()-lastQueryCompletionTime);
  idleReturnEffectActive_=true;
  auto weak=weak_from_this();
  idleReturnTimer_=services_.owner.setTimeout(std::max(0.0,remaining),
    [weak,mode]{
      if(auto self=weak.lock()){
        try{
          if(self->services_.messages.messages()->empty())return;
          const auto& idle=self->idle_;
          need(idle.getTotalInputTokens,"getTotalInputTokens");
          need(idle.formatTokens,"formatTokens");
          need(idle.addIdleReturnHint,"addIdleReturnHint");
          idle.addIdleReturnHint(mode,
            idle.formatTokens(idle.getTotalInputTokens()));
        }catch(...){self->reportDetached(std::current_exception());}
      }
    });
}
} // namespace batbox::interactive::repl_runtime
