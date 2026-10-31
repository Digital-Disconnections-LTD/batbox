#pragma once
#include "interactive/CommandUi.hpp"
#include "app_state/State.hpp"
#include "gadgets/bash/BashGadget.hpp"
#include <deque>
#include <future>
#include <memory>
#include <mutex>
#include <set>

namespace batbox::application_permissions {

// Permission decisions share one boundary for interactive and headless Flights.
class PermissionProvider final:public std::enable_shared_from_this<PermissionProvider>{
public:
  struct Inputs {
    event_loop::Loop& owner;
    std::shared_ptr<interactive::CommandUi> ui;
    bool interactive=true;
    std::function<void(const std::string&)> persistAllowRule;
    explicit Inputs(event_loop::Loop& loop):owner(loop){}
  };
  static std::shared_ptr<PermissionProvider> create(Inputs inputs);
  void attachUi(std::shared_ptr<interactive::CommandUi> ui);
  void bindBash(bash_gadget::Services& services);
  event_loop::Deferred<Json> checkBypassPermissions(const Json& permissionContext,
    const std::shared_ptr<app_state::AppStateStore>& store);
  std::future<Json> check(const std::string& gadgetName,const Json& input,
    const Json& permissionContext,bool forceAsk=false);
  bool deniedByRule(const std::string& gadgetName,const std::string& content,
    const Json& permissionContext)const;
  std::optional<Json> matchingRule(const std::string& gadgetName,
    const std::string& content,const Json& permissionContext,
    const std::string& behavior)const;
  std::optional<Json> ruleDecision(const std::string& gadgetName,
    const Json& input,const Json& permissionContext)const;
  void cancelAll();
private:
  explicit PermissionProvider(Inputs inputs):inputs_(std::move(inputs)){}
  struct Pending;
  void accept(std::shared_ptr<Pending> pending);
  void showNext();
  void decide(const std::shared_ptr<Pending>& pending,const interactive::UiAction& action);
  Inputs inputs_;
  std::deque<std::shared_ptr<Pending>> queue_;
  std::set<std::string> flightAllowRules_;
  std::mutex flightRulesMutex_;
  event_loop::Loop::TimerHandle retry_;
  bool showing_=false;
};
} // namespace batbox::application_permissions
