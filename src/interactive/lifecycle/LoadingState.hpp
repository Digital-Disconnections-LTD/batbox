#pragma once
#include "app_state/State.hpp"
#include "file_state_stash/FileStateStash.hpp"
#include "interactive/QueryGuard.hpp"
#include "interactive/lifecycle/MessageState.hpp"

namespace batbox::interactive::lifecycle {
struct Tip {
  Json metadata;
  std::function<event_loop::Deferred<std::string>(const Json& theme)> content;
};
using TipRef=std::shared_ptr<Tip>;
struct TipContext {
  Json theme;
  std::shared_ptr<file_state_stash::FileStateStash> readFileState;
  std::shared_ptr<runtime::OrderedStringSet> bashGadgets;
};
struct LoadingServices {
  std::shared_ptr<app_state::AppStateStore> store;
  std::function<double()> nowMs;
  std::function<event_loop::Deferred<TipRef>(const TipContext&)>
    getTipToShowOnSpinner;
  std::function<void(const TipRef&)> recordShownTip;
  std::function<void()> endInteractionSpan,clearSpeculativeChecks,requestRedraw;
  std::function<void(std::exception_ptr)> onDetachedError;
  std::shared_ptr<file_state_stash::FileStateStash> readFileState;
  Json theme=Json::object();
};

class LoadingState:public std::enable_shared_from_this<LoadingState>{
public:
  LoadingState(event_loop::Loop& owner,QueryGuard& guard,
    PresentationState& presentation,MessageState& messages,
    LoadingServices services);
  ~LoadingState();
  void start();
  void observeQueryActive();
  void setExternalLoading(bool value);
  bool isLoading()const{return guard_.isActive()||externalLoading_;}
  void resetTimingRefs();
  void observePermissionPause(bool paused,double nowMs);
  void setResponseLength(const std::function<double(double)>& update);
  void syncResponseLengthFromPresentation();
  void onStreamingText(TextUpdate update);
  void setShowStreamingText(bool visible);
  void pickNewSpinnerTip();
  void resetLoadingState();
  void beginSubmit(){tipPickedThisTurn_=false;}
  void clearBashGadgets();
  double responseLength()const{return responseLength_;}
  double loadingStartTime()const{return loadingStartTime_;}
  double totalPausedMs()const{return totalPausedMs_;}
  std::optional<double> pauseStartTime()const{return pauseStartTime_;}
  bool tipPickedThisTurn()const{return tipPickedThisTurn_;}
  const runtime::OrderedStringSet& bashGadgets()const{return *bashGadgets_;}
  std::size_t bashGadgetsProcessedIndex()const{return bashGadgetsProcessedIndex_;}
private:
  event_loop::Loop& owner_;
  QueryGuard& guard_;
  PresentationState& presentation_;
  MessageState& messages_;
  LoadingServices services_;
  std::shared_ptr<runtime::OrderedStringSet> bashGadgets_=
    std::make_shared<runtime::OrderedStringSet>();
  std::function<void()> unsubscribe_;
  std::size_t bashGadgetsProcessedIndex_=0;
  double responseLength_=0,loadingStartTime_=0,totalPausedMs_=0;
  std::optional<double> pauseStartTime_;
  bool wasQueryActive_=false,externalLoading_=false,tipPickedThisTurn_=false;
  std::optional<std::string> spinnerMessage_,spinnerColor_,spinnerShimmerColor_;
  void changed();
  void tipError(std::exception_ptr error);
};
} // namespace batbox::interactive::lifecycle
