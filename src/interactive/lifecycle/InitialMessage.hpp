#pragma once
#include "app_state/State.hpp"
#include "interactive/QueryTurn.hpp"
#include "interactive/lifecycle/LoadingState.hpp"

namespace batbox::interactive::lifecycle {
struct InitialMessage {
  TurnMessageRef message;
  bool clearContext=false;
  std::optional<std::string> mode;
  Json allowedPoetryEntries=Json(Json::value_t::discarded);
};
struct InitialMessageServices {
  event_loop::Loop& owner;
  std::shared_ptr<app_state::AppStateStore> store;
  InitialMessageServices(event_loop::Loop& loop,
    std::shared_ptr<app_state::AppStateStore> appStore):
    owner(loop),store(std::move(appStore)){}
  std::function<event_loop::Deferred<event_loop::Unit>()> clearConversation;
  std::function<std::optional<std::string>()> getPlanSlug;
  std::function<std::string()> getFlightId;
  std::function<void(const std::string&,const std::string&)> setPlanSlug;
  std::function<void()> resetHaikuTitle,clearBashGadgets;
  std::function<Json(const std::string&,const Json&)> buildPermissionUpdates;
  std::function<value::RecordRef(const value::RecordRef&,const Json&)>
    applyPermissionUpdates;
  std::function<value::RecordRef(const value::RecordRef&)>
    stripDangerousPermissionsForAutoMode;
  std::function<bool(const std::string&)> feature;
  std::function<bool()> fileHistoryEnabled;
  using HistoryUpdate=std::function<value::ValueRef(const value::ValueRef&)>;
  using HistorySetter=std::function<void(const HistoryUpdate&)>;
  std::function<event_loop::Deferred<event_loop::Unit>(HistorySetter,
    const std::string&)> fileHistoryMakeSnapshot;
  std::function<event_loop::Deferred<event_loop::Unit>()> awaitPendingHooks;
  std::function<event_loop::Deferred<event_loop::Unit>(const std::string&)> onSubmit;
  std::function<event_loop::Deferred<event_loop::Unit>(TurnRequest)> onQuery;
  std::function<void(std::shared_ptr<TurnAbort>)> setAbortController;
  std::function<std::string()> mainLoopModel;
  std::function<void(std::exception_ptr)> onDetachedError;
};

// Owns the one-shot effect guard and its delayed release. The pending message
// is an explicit live ref so a query receives the original message object.
class InitialMessageController:public std::enable_shared_from_this<InitialMessageController>{
public:
  explicit InitialMessageController(InitialMessageServices services);
  void observe(const std::optional<InitialMessage>& pending,bool isLoading);
  bool processing()const{return processing_;}
private:
  InitialMessageServices services_;
  bool processing_=false;
  event_loop::Loop::TimerHandle release_;
  void process(InitialMessage initial);
  void afterClear(InitialMessage initial);
  void afterHooks(InitialMessage initial);
  void detached(std::exception_ptr error);
};
} // namespace batbox::interactive::lifecycle
