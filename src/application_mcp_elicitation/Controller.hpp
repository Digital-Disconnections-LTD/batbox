#pragma once

#include "event_loop/Deferred.hpp"
#include "interactive/CommandUi.hpp"
#include "application_mcp_elicitation/DateTimeParser.hpp"
#include "runtime/JsonValue.hpp"
#include <deque>
#include <memory>
#include <stop_token>
#include <vector>

namespace batbox::application_mcp_elicitation {

struct Hooks {
  // Null means no hook decision. A response object may accept, decline or
  // cancel; errors in before/after hooks fall back to the interactive/raw flow.
  std::function<event_loop::Deferred<Json>(const std::string&,const Json&,
    std::stop_token)> before;
  std::function<event_loop::Deferred<Json>(const std::string&,const Json&,
    const Json&,std::stop_token)> after;
};
struct Inputs {
  event_loop::Loop& owner;
  bool interactive=true;
  std::function<void(const std::string&)> openUrl;
  std::function<event_loop::Deferred<DateTimeParseResult>(
    const std::string&,const std::string&,std::stop_token)> parseNaturalDateTime;
  std::function<void(const std::string&)> diagnostic;
  explicit Inputs(event_loop::Loop& loop):owner(loop){}
};

class Controller final:public std::enable_shared_from_this<Controller> {
public:
  static std::shared_ptr<Controller> create(Inputs inputs);
  void attachUi(std::shared_ptr<interactive::CommandUi> ui);
  event_loop::Deferred<Json> request(const std::string& server,
    const Json& rpcRequest,std::stop_token signal={},Hooks hooks={});
  void complete(const std::string& server,const std::string& elicitationId);
  void cancelAll();
private:
  explicit Controller(Inputs inputs):inputs_(std::move(inputs)){}
  struct Pending;
  Inputs inputs_;
  std::shared_ptr<interactive::CommandUi> ui_;
  std::deque<std::shared_ptr<Pending>> queue_;
  std::vector<std::weak_ptr<Pending>> all_;
  event_loop::Loop::TimerHandle retry_;
  bool showing_=false;
  void enqueue(const std::shared_ptr<Pending>& pending);
  void showNext();
  void show(const std::shared_ptr<Pending>& pending);
  void showForm(const std::shared_ptr<Pending>& pending);
  void showOptions(const std::shared_ptr<Pending>& pending);
  void showSummary(const std::shared_ptr<Pending>& pending);
  void showUrl(const std::shared_ptr<Pending>& pending);
  void showWaiting(const std::shared_ptr<Pending>& pending);
  void decide(const std::shared_ptr<Pending>& pending,
    const interactive::UiAction& action);
  void advanceField(const std::shared_ptr<Pending>& pending,Json value);
  void resolve(const std::shared_ptr<Pending>& pending,Json raw);
  void finish(const std::shared_ptr<Pending>& pending,Json result);
  void dismiss(const std::shared_ptr<Pending>& pending);
  void cancelOne(const std::shared_ptr<Pending>& pending);
};
} // namespace batbox::application_mcp_elicitation
