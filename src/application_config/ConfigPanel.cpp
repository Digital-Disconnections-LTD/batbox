#include "application_config/ConfigPanelInternal.hpp"
#include "application/Async.hpp"
#include "runtime/JsonValue.hpp"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <limits>
#include <set>
#include <stdexcept>

namespace batbox::application_config {
std::string slug(std::string label){
  std::string id;bool dash=false;
  for(const unsigned char ch:label){
    if(std::isalnum(ch)){id+=static_cast<char>(std::tolower(ch));dash=false;}
    else if(!id.empty()&&!dash){id+='-';dash=true;}
  }
  while(id.ends_with('-'))id.pop_back();
  return id.empty()?"endpoint":id;
}
std::string lower(std::string value){
  std::transform(value.begin(),value.end(),value.begin(),[](unsigned char c){
    return static_cast<char>(std::tolower(c));});return value;
}
std::size_t parsePositive(const std::string& value){
  if(value.empty()||value[0]=='-')throw std::invalid_argument("Enter a positive token count");
  std::size_t used=0;const auto parsed=std::stoull(value,&used);
  if(used!=value.size()||parsed==0||parsed>std::numeric_limits<std::size_t>::max())
    throw std::invalid_argument("Enter a positive token count");
  return static_cast<std::size_t>(parsed);
}
std::size_t movedIndex(std::size_t current,std::size_t begin,
    std::size_t visible,std::size_t total,std::size_t local){
  if(visible==0||local>=visible)return current;
  const auto oldLocal=current-begin;
  if(local==0&&oldLocal==visible-1&&current+1<total)return current+1;
  if(local==visible-1&&oldLocal==0&&current>0)return current-1;
  return std::min(begin+local,total-1);
}
std::string credential(const Endpoint& endpoint){
  if(!endpoint.authToken.empty())return endpoint.authToken;
  if(endpoint.apiKeyEnv.empty())return {};
  const auto* value=std::getenv(endpoint.apiKeyEnv.c_str());
  return value?value:"";
}
application_model::ModelProfile runtimeProfile(const std::string& id,
    const Endpoint& endpoint,const std::string& model){
  const auto configured=inferenceProfile(endpoint,model);
  application_model::ModelProfile profile;
  profile.id=id;profile.wireModel=model;
  profile.contextTokens=static_cast<double>(configured.contextWindow);
  profile.maxOutputTokens=static_cast<double>(configured.maxOutputTokens);
  profile.poetryStashing=configured.poetryStashing;
  profile.thinking=configured.thinking;
  profile.adaptiveThinking=configured.adaptiveThinking;
  profile.structuredOutputs=configured.structuredOutputs;
  profile.effort=configured.effort;
  profile.sendDisabledThinking=configured.sendDisabledThinking;
  profile.client.emplace();
  profile.client->baseUrl=configured.baseUrl;
  profile.client->messagesPath=configured.messagesPath;
  profile.client->headers=configured.headers;
  profile.authHeader=configured.authHeader;
  profile.costs={{"inputTokens",0},{"outputTokens",0},
    {"promptCacheReadTokens",0},{"promptCacheWriteTokens",0},
    {"webSearchRequests",0}};
  return profile;
}
Panel::Panel(event_loop::Loop& owner,std::shared_ptr<application::Config> config,
      std::shared_ptr<application_model::LocalModelService> model,
      std::shared_ptr<app_state::AppStateStore> store,
      std::function<std::shared_ptr<interactive::conversation_shell::Shell>()> shell,
      Done done,Ready ready,Error error,bool modelMode,
      std::string args,std::shared_ptr<application_compaction_settings::Policy> policy):
      policy_(std::move(policy)),owner_(owner),config_(std::move(config)),
      model_(std::move(model)),store_(std::move(store)),shell_(std::move(shell)),
      done_(std::move(done)),ready_(std::move(ready)),error_(std::move(error)),
      endpoints_(config_->endpoints),modelMode_(modelMode),args_(std::move(args)){}
void Panel::start(){
    interactive::UiRequest view;view.kind=interactive::UiRequest::Kind::Progress;
    view.id="endpoint-loading";view.title=modelMode_?"Models":"Configuration";
    view.body="Loading endpoint settings…";
    publish(std::move(view),[self=shared_from_this()](const auto& event){
      if(event.kind==interactive::UiAction::Kind::Cancel)self->close();
    });
    auto self=shared_from_this();const auto path=config_->configPath;
    application::runInWorker<std::optional<std::map<std::string,Endpoint>>>(owner_,
      [path]{return readEndpoints(path);})
      .then([self](const std::optional<std::map<std::string,Endpoint>>* loaded,
          const std::exception_ptr& error){
        if(!self->active_)return;
        if(error){self->problem(error);return;}
        if(loaded&&*loaded){self->endpoints_=**loaded;self->config_->endpoints=**loaded;}
        self->openInitial();
      });
  }
void Panel::openInitial(){
    if(!modelMode_){
      if(policy_)config_->compactAt=policy_->compactAt();
      showHome();return;
    }
    const auto current=selectedProfile();
    for(const auto& [id,endpoint]:endpoints_)
      if(current.starts_with(id+"::")){endpointId_=id;break;}
    const auto requested=value::trim(args_);
    if(requested=="refresh"){
      if(endpointId_.empty()){
        notice_="Choose an endpoint to discover its models.";showHome();
      }else{returnPage_=Page::Models;refresh();}
      return;
    }
    if(!requested.empty()){
      auto target=requested=="default"?model_->config().mainModel:requested;
      if(!model_->config().profiles.contains(target)&&!endpointId_.empty()&&
          model_->config().profiles.contains(profileId(endpointId_,target)))
        target=profileId(endpointId_,target);
      if(!model_->config().profiles.contains(target)){
        notice_="Model '"+requested+"' is not configured. Refresh or enter an ID manually.";
        if(endpointId_.empty())showHome();else showModels();
        return;
      }
      closeAfterSave_="Set model to "+target;
      returnPage_=endpointId_.empty()?Page::Home:Page::Models;
      persist(endpoints_,target);return;
    }
    if(endpointId_.empty())showHome();else showModels();
  }
int Panel::pageSize()const{
    const auto shell=shell_?shell_():nullptr;
    return std::clamp(shell?shell->viewState().rows-15:9,4,22);
  }
void Panel::close(std::optional<std::string> message){
    if(!active_)return;
    active_=false;
    interactive::CommandCompletion result;
    result.options.display=message?interactive::DoneOptions::Display::System:
      interactive::DoneOptions::Display::Skip;
    result.result=std::move(message);
    done_(std::move(result));
  }
void Panel::publish(interactive::UiRequest request,
      std::function<void(const interactive::UiAction&)> action){
    if(!active_)return;
    auto self=shared_from_this();
    ready_(std::move(request),[self,action=std::move(action)](const auto& event){
      if(self->active_)action(event);
    });
  }
void Panel::problem(std::exception_ptr error){
    busy_=false;
    try{if(error)std::rethrow_exception(error);}
    catch(const std::exception& issue){notice_=issue.what();}
    catch(...){notice_="Configuration could not be saved";}
    showCurrent();
  }
std::string Panel::selectedProfile()const{
    if(store_){
      const auto state=store_->getState()->snapshot();
      const auto& value=value::property(state,"mainLoopModel");
      if(value.is_string()&&!value.get<std::string>().empty())
        return value.get<std::string>();
    }
    return config_->model;
  }
void Panel::setSelectedProfile(const std::string& value){
    if(store_)store_->setState([value](const app_state::AppStateRef& previous){
      auto next=std::make_shared<app_state::AppState>(previous->shallowCopy());
      next->fields->set("mainLoopModel",value);
      next->fields->set("mainLoopModelForSession",nullptr);
      return next;
    });
  }
bool Panel::hasProfile(const std::map<std::string,Endpoint>& endpoints,
      const std::string& name)const{
    for(const auto& [id,endpoint]:endpoints)
      for(const auto& model:endpoint.models)
        if(profileId(id,model)==name)return true;
    if(!model_->config().profiles.contains(name))return false;
    for(const auto& [endpointId,old]:endpoints_)
      for(const auto& modelId:old.models)
        if(profileId(endpointId,modelId)==name)return false;
    return true;
  }
std::string Panel::fallbackProfile(const std::map<std::string,Endpoint>& next)const{
    for(const auto& [id,endpoint]:next)
      if(!endpoint.models.empty())return profileId(id,endpoint.models.front());
    std::set<std::string> oldIds;
    for(const auto& [endpointId,old]:endpoints_)
      for(const auto& wire:old.models)
        oldIds.insert(profileId(endpointId,wire));
    for(const auto& [id,profile]:model_->config().profiles){
      if(!oldIds.contains(id))return id;
    }
    return {};
  }
void Panel::persist(std::map<std::string,Endpoint> next,std::string selection,
      Json settingEdits){
    if(busy_)return;
    try{
      for(const auto& [id,endpoint]:next)validateEndpoint(id,endpoint);
      if(!hasProfile(next,selection))selection=fallbackProfile(next);
      bool endpointSelected=false;
      for(const auto& [id,endpoint]:next)
        for(const auto& wire:endpoint.models)
          if(profileId(id,wire)==selection)endpointSelected=true;
      if(!endpointSelected&&!config_->profiles.contains(selection))
        for(const auto& [id,endpoint]:next)
          if(!endpoint.models.empty()){
            selection=profileId(id,endpoint.models.front());
            endpointSelected=true;break;
          }
      if(selection.empty())throw std::invalid_argument("Keep at least one configured model");
    }catch(...){problem(std::current_exception());return;}
    busy_=true;page_=Page::Progress;
    interactive::UiRequest view;
    view.kind=interactive::UiRequest::Kind::Progress;
    view.id="endpoint-saving";view.title="Saving configuration";
    view.body="Writing private endpoint settings…";
    publish(std::move(view),[](const auto&){});
    const auto path=config_->configPath;
    const auto legacy=config_->legacyConfigDirectory.empty()?
      std::filesystem::path{}:config_->legacyConfigDirectory/"native-client.json";
    bool persistSelection=config_->profiles.contains(selection);
    for(const auto& [id,endpoint]:next)
      for(const auto& wire:endpoint.models)
        if(profileId(id,wire)==selection)persistSelection=true;
    auto self=shared_from_this();
    application::runInWorker<event_loop::Unit>(owner_,
      [path,legacy,next,selection,settingEdits,persistSelection]{
        saveEndpoints(path,next,selection,settingEdits,persistSelection,legacy);
        return event_loop::Unit{};
      }).then([self,next=std::move(next),selection=std::move(selection),
          settingEdits=std::move(settingEdits)](const event_loop::Unit*,
          const std::exception_ptr& error)mutable{
        if(!self->active_)return;
        if(error){self->problem(error);return;}
        try{
          std::set<std::string> retained;
          for(const auto& [id,endpoint]:next)
            for(const auto& wire:endpoint.models)
              retained.insert(profileId(id,wire));
          for(const auto& [id,endpoint]:next)
            for(const auto& wire:endpoint.models)
              self->model_->upsertRuntimeProfile(
                runtimeProfile(profileId(id,wire),endpoint,wire),credential(endpoint));
          for(const auto& [id,endpoint]:self->endpoints_)
            for(const auto& wire:endpoint.models)
              if(!retained.contains(profileId(id,wire)))
                self->model_->removeRuntimeProfile(profileId(id,wire));
          self->endpoints_=next;
          self->config_->endpoints=std::move(next);
          self->config_->selectedProfile=selection;
          self->config_->model=selection;
          if(settingEdits.contains("compact_at")){
            self->config_->compactAt=settingEdits.at("compact_at").get<std::size_t>();
            if(self->policy_)self->policy_->applyExternalThreshold(self->config_->compactAt);
          }
          if(settingEdits.contains("thinking"))
            self->config_->thinking=settingEdits.at("thinking").get<bool>();
          if(settingEdits.contains("max_subagents"))
            self->config_->maxSubagents=settingEdits.at("max_subagents").get<std::size_t>();
          if(settingEdits.contains("max_concurrent_subagents"))
            self->config_->maxConcurrentSubagents=
              settingEdits.at("max_concurrent_subagents").get<std::size_t>();
          self->setSelectedProfile(selection);
          self->busy_=false;self->notice_="Saved. Next request uses the selected model.";
          if(self->closeAfterSave_){
            auto message=std::move(self->closeAfterSave_);
            self->close(message);return;
          }
          self->showCurrent();
        }catch(...){self->problem(std::current_exception());}
      });
  }
void Panel::showCurrent(){
    switch(returnPage_){
      case Page::Home:showHome();break;
      case Page::Detail:showDetail();break;
      case Page::Models:showModels();break;
      case Page::Edit:showEdit();break;
      case Page::Settings:showSettings();break;
      default:showHome();break;
    }
  }
user_input::CommandEntry makeConfigCommand(event_loop::Loop& owner,
    std::shared_ptr<user_input::UiCommandRouter> router,
    application::Config config,
    std::shared_ptr<application_model::LocalModelService> model,
    std::shared_ptr<app_state::AppStateStore> store,
    std::function<std::shared_ptr<interactive::conversation_shell::Shell>()> shell,
    std::shared_ptr<application_compaction_settings::Policy> policy){
  auto mutableConfig=std::make_shared<application::Config>(std::move(config));
  Json descriptor={{"type","local-jsx"},{"name","config"},
    {"aliases",Json::array({"settings"})},
    {"description","Manage API endpoints, models and request settings"}};
  return user_input::bindUiCommand(std::move(descriptor),router,
    [&owner,mutableConfig,model=std::move(model),store=std::move(store),
        shell=std::move(shell),policy=std::move(policy)](const std::string&){
      return [&owner,mutableConfig,model,store,shell,policy](auto done,auto ready,auto error){
        auto panel=std::make_shared<Panel>(owner,mutableConfig,model,store,shell,
          std::move(done),std::move(ready),std::move(error),false,std::string{},policy);
        panel->start();
      };
    });
}
user_input::CommandEntry makeEndpointModelCommand(event_loop::Loop& owner,
    std::shared_ptr<user_input::UiCommandRouter> router,
    application::Config config,
    std::shared_ptr<application_model::LocalModelService> model,
    std::shared_ptr<app_state::AppStateStore> store,
    std::function<std::shared_ptr<interactive::conversation_shell::Shell>()> shell){
  auto mutableConfig=std::make_shared<application::Config>(std::move(config));
  Json descriptor={{"type","local-jsx"},{"name","model"},
    {"description","Choose or discover models on an API endpoint"},
    {"argumentHint","[model|refresh]"}};
  return user_input::bindUiCommand(std::move(descriptor),router,
    [&owner,mutableConfig,model=std::move(model),store=std::move(store),
        shell=std::move(shell)](const std::string& args){
      return [&owner,mutableConfig,model,store,shell,args](auto done,auto ready,auto error){
        auto panel=std::make_shared<Panel>(owner,mutableConfig,model,store,shell,
          std::move(done),std::move(ready),std::move(error),true,args);
        panel->start();
      };
    });
}
user_input::CommandEntry makeRoutedModelCommand(user_input::CommandEntry fallback,
    event_loop::Loop& owner,
    std::shared_ptr<user_input::UiCommandRouter> router,
    application::Config config,
    std::shared_ptr<application_model::LocalModelService> model,
    std::shared_ptr<app_state::AppStateStore> store,
    std::function<std::shared_ptr<interactive::conversation_shell::Shell>()> shell){
  const auto path=config.configPath;
  const bool initiallyConfigured=!config.endpoints.empty();
  auto enhanced=makeEndpointModelCommand(owner,std::move(router),std::move(config),
    std::move(model),std::move(store),std::move(shell));
  auto endpointInvoke=enhanced.invokeUi;
  auto fallbackInvoke=std::move(fallback.invokeUi);
  enhanced.invokeUi=[&owner,path,initiallyConfigured,
      endpointInvoke=std::move(endpointInvoke),
      fallbackInvoke=std::move(fallbackInvoke)](const std::string& args){
    event_loop::Deferred<user_input::UiCompletion> output(owner);
    application::runInWorker<std::optional<std::map<std::string,Endpoint>>>(
      owner,[path]{return readEndpoints(path);}).then(
        [output,endpointInvoke,fallbackInvoke,initiallyConfigured,args](
            const std::optional<std::map<std::string,Endpoint>>* catalog,
            const std::exception_ptr& error){
          if(error){output.reject(error);return;}
          const bool hasEndpoints=catalog&&*catalog?
            !(*catalog)->empty():initiallyConfigured;
          try{
            const auto& invoke=hasEndpoints?endpointInvoke:fallbackInvoke;
            invoke(args).then([output](const user_input::UiCompletion* completion,
                const std::exception_ptr& failure){
              if(failure)output.reject(failure);
              else output.resolve(*completion);
            });
          }catch(...){output.reject(std::current_exception());}
        });
    return output;
  };
  return enhanced;
}
}
