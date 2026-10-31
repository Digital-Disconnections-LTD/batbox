#include "interactive/lifecycle/LoadingState.hpp"
#include "interactive/lifecycle/BashGadgets.hpp"
#include <cmath>
#include <stdexcept>

namespace batbox::interactive::lifecycle {
namespace {
template<class T>void need(const T& value,const char* name){
  if(!value)throw std::invalid_argument(std::string("Loading binding required: ")+name);
}
}
LoadingState::LoadingState(event_loop::Loop& owner,QueryGuard& guard,
    PresentationState& presentation,MessageState& messages,
    LoadingServices services):owner_(owner),guard_(guard),
      presentation_(presentation),messages_(messages),
      services_(std::move(services)){
  need(services_.store,"store");
  need(services_.nowMs,"nowMs");
}
LoadingState::~LoadingState(){if(unsubscribe_)unsubscribe_();}
void LoadingState::changed(){
  ++presentation_.revision;
  if(services_.requestRedraw)services_.requestRedraw();
}
void LoadingState::start(){
  if(!owner_.isOwnerThread())throw std::logic_error("Loading owner required");
  if(unsubscribe_)return;
  auto weak=weak_from_this();
  unsubscribe_=guard_.subscribe(
    std::make_shared<bootstrap::Signal<>::Listener>([weak]{
      if(auto self=weak.lock())self->observeQueryActive();
    }));
  observeQueryActive();
}
void LoadingState::resetTimingRefs(){
  loadingStartTime_=services_.nowMs();
  totalPausedMs_=0;
  pauseStartTime_.reset();
}
void LoadingState::observePermissionPause(bool paused,double nowMs){
  if(paused&&!pauseStartTime_)pauseStartTime_=nowMs;
  else if(!paused&&pauseStartTime_){
    totalPausedMs_+=nowMs-*pauseStartTime_;
    pauseStartTime_.reset();
  }
}
void LoadingState::observeQueryActive(){
  const bool active=guard_.isActive();
  if(active&&!wasQueryActive_)resetTimingRefs();
  wasQueryActive_=active;
  presentation_.loading=isLoading();
  changed();
}
void LoadingState::setExternalLoading(bool value){
  externalLoading_=value;
  if(value)resetTimingRefs();
  presentation_.loading=isLoading();
  changed();
}
void LoadingState::setResponseLength(
    const std::function<double(double)>& update){
  need(update,"response length updater");
  const double previous=responseLength_;
  responseLength_=update(previous);
  if(std::isfinite(responseLength_)&&responseLength_>=0)
    presentation_.responseLengthUtf16=
      static_cast<std::size_t>(responseLength_);
  if(responseLength_>previous&&!presentation_.apiMetrics.empty()){
    auto& last=presentation_.apiMetrics.back();
    last.lastTokenTime=services_.nowMs();
    last.endResponseLength=presentation_.responseLengthUtf16;
  }
  changed();
}
void LoadingState::syncResponseLengthFromPresentation(){
  responseLength_=static_cast<double>(presentation_.responseLengthUtf16);
}
void LoadingState::onStreamingText(TextUpdate update){
  if(!presentation_.showStreamingText)return;
  need(update,"streaming text updater");
  presentation_.streamingText=update(presentation_.streamingText);
  changed();
}
void LoadingState::setShowStreamingText(bool visible){
  presentation_.showStreamingText=visible;
  changed();
}
void LoadingState::tipError(std::exception_ptr error){
  need(services_.onDetachedError,"onDetachedError");
  services_.onDetachedError(error);
}
void LoadingState::pickNewSpinnerTip(){
  if(tipPickedThisTurn_)return;
  tipPickedThisTurn_=true;
  const auto current=messages_.messages();
  for(const auto& gadget:extractBashGadgetsFromMessages(
      current,bashGadgetsProcessedIndex_))bashGadgets_->insert(gadget);
  bashGadgetsProcessedIndex_=current->size();
  need(services_.getTipToShowOnSpinner,"getTipToShowOnSpinner");
  const auto pending=services_.getTipToShowOnSpinner(
    TipContext{services_.theme,services_.readFileState,bashGadgets_});
  if(!owner_.dispatcher().sameLoop(pending.dispatcher()))
    throw std::invalid_argument("Spinner tip Promise crossed owner loop");
  auto self=shared_from_this();
  pending.then([self](const TipRef* tip,const std::exception_ptr& error){
    if(error){self->tipError(error);return;}
    if(tip&&*tip){
      try{
        need((*tip)->content,"tip.content");
        auto content=(*tip)->content(Json{{"theme",self->services_.theme}});
        if(!self->owner_.dispatcher().sameLoop(content.dispatcher()))
          throw std::invalid_argument("Tip content Promise crossed owner loop");
        content.then([self,selected=*tip](const std::string* value,
            const std::exception_ptr& error){
          if(error){self->tipError(error);return;}
          if(!value){self->tipError(std::make_exception_ptr(
            std::logic_error("Tip content missing")));return;}
          self->services_.store->setState([value](const app_state::AppStateRef& old){
            auto next=std::make_shared<app_state::AppState>(old->shallowCopy());
            next->fields->set("spinnerTip",*value);
            return next;
          });
          need(self->services_.recordShownTip,"recordShownTip");
          self->services_.recordShownTip(selected);
        });
      }catch(...){self->tipError(std::current_exception());}
    }else{
      self->services_.store->setState([](const app_state::AppStateRef& old){
        const auto existing=old->fieldRef("spinnerTip");
        if(!existing||existing->is_discarded())return old;
        auto next=std::make_shared<app_state::AppState>(old->shallowCopy());
        next->fields->setRef("spinnerTip",{});
        return next;
      });
    }
  });
}
void LoadingState::resetLoadingState(){
  externalLoading_=false;
  messages_.setUserInputOnProcessing({});
  responseLength_=0;
  presentation_.responseLengthUtf16=0;
  presentation_.apiMetrics.clear();
  presentation_.streamingText.reset();
  presentation_.streamingGadgetUses.clear();
  spinnerMessage_.reset();spinnerColor_.reset();spinnerShimmerColor_.reset();
  presentation_.loading=isLoading();
  pickNewSpinnerTip();
  need(services_.endInteractionSpan,"endInteractionSpan");
  services_.endInteractionSpan();
  need(services_.clearSpeculativeChecks,"clearSpeculativeChecks");
  services_.clearSpeculativeChecks();
  changed();
}
void LoadingState::clearBashGadgets(){
  bashGadgets_->clear();
  bashGadgetsProcessedIndex_=0;
}
} // namespace batbox::interactive::lifecycle
