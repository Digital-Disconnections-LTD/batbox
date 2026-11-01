#include "application_permissions/PermissionProvider.hpp"
#include "application_shell/RuleMatch.hpp"
#include "app_state/provider/PermissionHelpers.hpp"
#include "runtime/JsonValue.hpp"
#include "gadgets/web/Web.hpp"
#include "interactive/gadget_views/GadgetViewModel.hpp"
#include <stdexcept>

namespace batbox::application_permissions {
namespace {
const Json& field(const Json& value,const char* name){return value::property(value,name);}
std::string asString(const Json& value){return value.is_string()?value.get<std::string>():"";}
std::string commandFor(const std::string& gadget,const Json& input){
  if(gadget=="Bash")return asString(field(input,"command"));
  if(gadget=="Agent")return asString(field(input,"subagent_type"));
  if(gadget=="WebFetch")return gadgets::web::webFetchPermissionRuleContent(input);
  for(const char* key:{"file_path","path","url","command"}){
    const auto value=asString(field(input,key));if(!value.empty())return value;
  }
  return "";
}
// File Gadgets ask about the path itself, so the dialog can shorten it.
bool pathContent(const std::string& gadget,const Json& input,const std::string& content){
  if(content.empty()||gadget=="Bash"||gadget=="Agent"||gadget=="WebFetch")return false;
  for(const char* key:{"file_path","path"})
    if(asString(field(input,key))==content)return true;
  return false;
}
bool matchesContent(const std::string& pattern,const std::string& content){
  const auto rule=application_shell::parsePermissionRule(pattern);
  if(rule.type==application_shell::ShellRule::Type::Exact)return content==rule.content;
  if(rule.type==application_shell::ShellRule::Type::Prefix)
    return content==rule.content || content.starts_with(rule.content+" ");
  return application_shell::matchWildcardPattern(rule.content,content,false);
}
bool matchesRule(const std::string& rule,const std::string& gadget,
    const std::string& content){
  if(rule==gadget)return true;
  const std::string prefix=gadget+"(";
  if(!rule.starts_with(prefix)||!rule.ends_with(')'))return false;
  return matchesContent(rule.substr(prefix.size(),rule.size()-prefix.size()-1),content);
}
std::optional<Json> matchingRuleValue(const Json& context,const char* key,
    const std::string& gadget,const std::string& content){
  const Json& rules=field(context,key);
  auto inArray=[&](const Json& values)->std::optional<Json>{
    if(!values.is_array())return std::nullopt;
    for(const auto& value:values)
      if(value.is_string()&&matchesRule(value.get<std::string>(),gadget,content))
        return Json{{"source","settings"},{"rule",value},
          {"ruleBehavior",key}};
    return std::nullopt;
  };
  if(auto match=inArray(rules))return match;
  if(rules.is_object())for(auto it=rules.begin();it!=rules.end();++it)
    if(auto match=inArray(it.value()))return match;
  return std::nullopt;
}
bool containsMatchingRule(const Json& context,const char* key,
    const std::string& gadget,const std::string& content){
  return matchingRuleValue(context,key,gadget,content).has_value();
}
Json allow(const Json& input,const char* reason){
  return Json{{"behavior","allow"},{"updatedInput",input},
    {"decisionReason",{{"type","other"},{"reason",reason}}}};
}
Json deny(const std::string& message){
  return Json{{"behavior","deny"},{"message",message},
    {"decisionReason",{{"type","other"},{"reason",message}}}};
}
std::future<Json> ready(Json value){
  std::promise<Json> promise;auto future=promise.get_future();
  promise.set_value(std::move(value));return future;
}
}
struct PermissionProvider::Pending {
  std::string gadget,content,rule;
  Json input,context;
  std::promise<Json> promise;
};
std::shared_ptr<PermissionProvider> PermissionProvider::create(Inputs inputs){
  return std::shared_ptr<PermissionProvider>(new PermissionProvider(std::move(inputs)));
}
void PermissionProvider::attachUi(std::shared_ptr<interactive::CommandUi> ui){
  if(!inputs_.owner.isOwnerThread())
    throw std::logic_error("Permission UI attachment requires application owner");
  if(!ui)throw std::invalid_argument("Permission UI required");
  if(inputs_.ui&&inputs_.ui!=ui)
    throw std::logic_error("Permission UI is already attached");
  inputs_.ui=std::move(ui);
  showNext();
}
void PermissionProvider::bindBash(bash_gadget::Services& services){
  auto weak=weak_from_this();
  services.checkPermissions=[weak](const Json& input,const Json& context){
    if(auto self=weak.lock())return self->check("Bash",input,context);
    return ready(deny("Permission provider is unavailable"));
  };
  services.preparePermissionMatcher=[](const Json& input){
    const auto command=commandFor("Bash",input);
    std::promise<std::function<bool(const std::string&)>> promise;
    auto future=promise.get_future();
    promise.set_value([command](const std::string& pattern){
      return matchesContent(pattern,command);
    });
    return future;
  };
}
event_loop::Deferred<Json> PermissionProvider::checkBypassPermissions(
    const Json& permissionContext,
    const std::shared_ptr<app_state::AppStateStore>& store){
  if(!inputs_.owner.isOwnerThread())
    throw std::logic_error("Bypass permission check requires application owner");
  if(!store)throw std::invalid_argument("AppState store required");
  event_loop::Deferred<Json> result(inputs_.owner);
  try{
    app_state::provider::BypassPolicyServices policy;
    policy.stashedDisableGate=[]{return false;};
    policy.getSettings=[store]{
      const Json state=store->getState()->snapshot();
      return Json(field(state,"settings"));
    };
    policy.applyPermissionUpdate=[](value::RecordRef context,const Json&){
      auto next=std::make_shared<value::SharedRecord>(context->shallowCopy());
      next->set("mode","default");return next;
    };
    if(!app_state::provider::isBypassPermissionsModeDisabled(policy)){
      result.resolve(permissionContext);return result;
    }
    auto changed=app_state::provider::createDisabledBypassPermissionsContext(
      store->getState()->permissionContext,policy);
    const Json updated=changed->snapshot();
    store->setState([changed](const app_state::AppStateRef& previous)
        ->app_state::AppStateRef{
      if(previous->permissionContext->snapshot()==changed->snapshot())return previous;
      auto next=std::make_shared<app_state::AppState>(previous->shallowCopy());
      next->setPermissionContext(changed);return next;
    });
    result.resolve(updated);
  }catch(...){result.reject(std::current_exception());}
  return result;
}
std::future<Json> PermissionProvider::check(const std::string& gadget,
    const Json& input,const Json& permissionContext,bool forceAsk){
  const Json& nested=field(permissionContext,"toolPermissionContext");
  const Json& rules=nested.is_object()?nested:permissionContext;
  const auto content=commandFor(gadget,input);
  const std::string exactRule=content.empty()?gadget:gadget+"("+content+")";
  if(deniedByRule(gadget,content,permissionContext))
    return ready(deny("Permission denied by rule: "+exactRule));
  const auto mode=asString(field(rules,"mode"));
  if(mode=="plan"&&(gadget=="Bash"||gadget=="Write"||gadget=="Edit"))
    return ready(deny("Current permission mode does not allow this action"));
  bool flightAllowed=false;
  {std::lock_guard lock(flightRulesMutex_);
    flightAllowed=flightAllowRules_.contains(exactRule);}
  if(containsMatchingRule(rules,"alwaysAskRules",gadget,content)||forceAsk){}
  else if(flightAllowed||containsMatchingRule(rules,"alwaysAllowRules",gadget,content))
    return ready(allow(input,"Permission rule"));
  if(mode=="bypassPermissions")
    return ready(allow(input,"Bypass permission mode"));
  if(field(rules,"shouldAvoidPermissionPrompts")==true)
    return ready(deny("Background agent needs a permission rule for "+exactRule));
  if(mode=="dontAsk")return ready(deny("Permission prompt disabled"));
  if(!inputs_.interactive)return ready(deny(
    "Permission required for "+gadget+"; headless execution cannot ask for approval"));
  auto pending=std::make_shared<Pending>();
  pending->gadget=gadget;pending->content=content;pending->rule=exactRule;
  pending->input=input;pending->context=permissionContext;
  auto future=pending->promise.get_future();
  auto weak=weak_from_this();
  inputs_.owner.postMission([weak,pending]{
    if(auto self=weak.lock())self->accept(pending);
    else pending->promise.set_value(deny("Permission provider is unavailable"));
  });
  return future;
}
bool PermissionProvider::deniedByRule(const std::string& gadget,
    const std::string& content,const Json& permissionContext)const{
  const Json& nested=field(permissionContext,"toolPermissionContext");
  const Json& rules=nested.is_object()?nested:permissionContext;
  return containsMatchingRule(rules,"alwaysDenyRules",gadget,content);
}
std::optional<Json> PermissionProvider::matchingRule(const std::string& gadget,
    const std::string& content,const Json& permissionContext,
    const std::string& behavior)const{
  const Json& nested=field(permissionContext,"toolPermissionContext");
  const Json& rules=nested.is_object()?nested:permissionContext;
  const char* key=behavior=="deny"?"alwaysDenyRules":
    behavior=="ask"?"alwaysAskRules":
    behavior=="allow"?"alwaysAllowRules":nullptr;
  if(!key)return std::nullopt;
  return matchingRuleValue(rules,key,gadget,content);
}
std::optional<Json> PermissionProvider::ruleDecision(const std::string& gadget,
    const Json& input,const Json& permissionContext)const{
  const auto content=commandFor(gadget,input);
  if(const auto rule=matchingRule(gadget,content,permissionContext,"deny"))
    return Json{{"behavior","deny"},{"message","Permission denied by rule"},
      {"decisionReason",{{"type","rule"},{"rule",*rule}}}};
  if(const auto rule=matchingRule(gadget,content,permissionContext,"ask"))
    return Json{{"behavior","ask"},{"message","Permission required"},
      {"decisionReason",{{"type","rule"},{"rule",*rule}}}};
  if(const auto rule=matchingRule(gadget,content,permissionContext,"allow"))
    return Json{{"behavior","allow"},{"updatedInput",input},
      {"decisionReason",{{"type","rule"},{"rule",*rule}}}};
  return std::nullopt;
}
void PermissionProvider::accept(std::shared_ptr<Pending> pending){
  queue_.push_back(std::move(pending));showNext();
}
void PermissionProvider::showNext(){
  if(showing_||queue_.empty()||!inputs_.ui)return;
  if(inputs_.ui->hasActiveLocalUi()){
    auto weak=weak_from_this();
    retry_=inputs_.owner.setTimeout(50,[weak]{if(auto self=weak.lock())self->showNext();},false);
    return;
  }
  showing_=true;
  const auto pending=queue_.front();
  interactive::UiRequest request;
  request.kind=interactive::UiRequest::Kind::Select;
  request.id="permission-"+pending->rule;
  request.title="Allow "+pending->gadget+"?";
  request.body=pending->content.empty()?pending->gadget:pending->content;
  if(pathContent(pending->gadget,pending->input,pending->content)&&inputs_.workingDirectory){
    try{
      const auto home=inputs_.homeDirectory?inputs_.homeDirectory():std::string{};
      request.body=interactive::gadget_views::displayPath(pending->content,
        inputs_.workingDirectory(),home);
    }catch(const std::exception&){request.body=pending->content;}
  }
  request.bodyPreviewLines=3;
  request.choices={"Allow once",
    inputs_.persistAllowRule?"Always allow this rule":"Allow this rule for this session",
    "Deny","Cancel"};
  // Say exactly what "always" saves, and show file changes before they land.
  request.choiceInfo={{},{(inputs_.persistAllowRule?"Saves ":"Until this Flight ends: ")+
      pending->rule,{},{}},{"The model hears that you declined",{},{}},{}};
  request.hint="↑↓ choose · enter confirm · esc cancel";
  const auto text=[&](const char* key){
    const auto& value=field(pending->input,key);
    return value.is_string()?value.get<std::string>():std::string{};};
  if(pending->gadget=="Edit")
    request.preview=interactive::gadget_views::editPreview(text("file_path"),
      text("old_string"),text("new_string"),12);
  else if(pending->gadget=="Write")
    request.preview=interactive::gadget_views::writePreview(text("file_path"),
      text("content"),12);
  request.selectedIndex=0;
  request.hidePoetry=true;
  auto weak=weak_from_this();
  inputs_.ui->setGadgetOverlay(std::move(request),false,
    [weak,pending](const interactive::UiAction& action){
      if(auto self=weak.lock())self->decide(pending,action);
    });
}
void PermissionProvider::decide(const std::shared_ptr<Pending>& pending,
    const interactive::UiAction& action){
  if(queue_.empty()||queue_.front()!=pending)return;
  if(action.kind==interactive::UiAction::Kind::MoveSelection ||
     action.kind==interactive::UiAction::Kind::TextChange)return;
  const bool canceled=action.kind==interactive::UiAction::Kind::Cancel;
  const auto index=canceled?3:action.index;
  if(index==0)pending->promise.set_value(allow(pending->input,"Allowed once"));
  else if(index==1){
    try{
      if(inputs_.persistAllowRule)inputs_.persistAllowRule(pending->rule);
      {std::lock_guard lock(flightRulesMutex_);flightAllowRules_.insert(pending->rule);}
      pending->promise.set_value(allow(pending->input,"Allowed by rule"));
    }catch(const std::exception& error){
      pending->promise.set_value(deny(std::string("Could not save permission rule: ")+
        error.what()));
    }catch(...){pending->promise.set_value(deny("Could not save permission rule"));}
  }else if(index==2)pending->promise.set_value(deny("User denied "+pending->rule));
  else pending->promise.set_value(deny("Permission request canceled"));
  queue_.pop_front();showing_=false;
  inputs_.ui->setGadgetOverlay(std::nullopt);
  showNext();
}
void PermissionProvider::cancelAll(){
  inputs_.owner.postMission([weak=weak_from_this()]{if(auto self=weak.lock()){
    self->retry_.cancel();
    for(auto& pending:self->queue_)pending->promise.set_value(
      deny("Permission request canceled"));
    self->queue_.clear();self->showing_=false;
    if(self->inputs_.ui)self->inputs_.ui->setGadgetOverlay(std::nullopt);
  }});
}
} // namespace batbox::application_permissions
