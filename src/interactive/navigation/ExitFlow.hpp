#pragma once
#include "event_loop/Deferred.hpp"
#include "interactive/CommandUi.hpp"
#include "runtime/JsonValue.hpp"

namespace batbox::interactive::navigation {
struct ExitServices {
  event_loop::Loop& owner;
  explicit ExitServices(event_loop::Loop& loop):owner(loop){}
  std::function<bool(const std::string&)> feature;
  std::function<bool()> isBgFlight;
  std::function<void()> detachTmuxClient;
  std::function<Json()> getCurrentWorktreeFlight;
  std::function<UiRequest()> makeWorktreeExitRequest;
  struct Result {
    bool isNull=true;
    std::optional<UiRequest> view;
  };
  using ExitCall=std::function<event_loop::Deferred<Result>(
    std::function<void()>)>;
  std::function<event_loop::Deferred<ExitCall>()> loadExit;
  std::function<void(bool)> publishExiting;
  std::function<void(std::optional<UiRequest>,bool undefined)> publishExitFlow;
};
class ExitFlow:public std::enable_shared_from_this<ExitFlow>{
public:
  explicit ExitFlow(ExitServices services):services_(std::move(services)){}
  event_loop::Deferred<event_loop::Unit> handleExit();
  void cancelFlow();
  bool isExiting()const{return isExiting_;}
  const std::optional<UiRequest>& flow()const{return flow_;}
  bool flowUndefined()const{return flowUndefined_;}
private:
  ExitServices services_;
  bool isExiting_=false;
  std::optional<UiRequest> flow_;
  bool flowUndefined_=false;
  void setExiting(bool value);
  void setFlow(std::optional<UiRequest> value,bool undefined=false);
};
} // namespace batbox::interactive::navigation
