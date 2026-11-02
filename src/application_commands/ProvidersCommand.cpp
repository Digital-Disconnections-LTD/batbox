#include "application_commands/ProvidersCommand.hpp"
#include "application_config/ConfigPanelInternal.hpp"
#include "application_config/EndpointStore.hpp"
#include "commands_indigenous/UiCommands.hpp"
#include "runtime/JsonValue.hpp"
#include <algorithm>
#include <cctype>
#include <set>
#include <stdexcept>

namespace batbox::application_commands {
namespace {
using application::EndpointRecord;
using Kind=interactive::UiAction::Kind;
using providers::CatalogProvider;

interactive::CommandCompletion completion(std::optional<std::string> message){
  interactive::CommandCompletion result;
  result.result=std::move(message);
  result.options.display=result.result?interactive::DoneOptions::Display::System:
    interactive::DoneOptions::Display::Skip;
  return result;
}
std::string lowered(std::string value){
  std::transform(value.begin(),value.end(),value.begin(),[](unsigned char c){
    return static_cast<char>(std::tolower(c));});
  return value;
}
bool matches(const std::string& haystack,const std::string& filter){
  const auto text=lowered(haystack),needle=lowered(filter);
  std::size_t start=0;
  while(start<needle.size()){
    const auto end=needle.find(' ',start);
    const auto term=needle.substr(start,end==std::string::npos?std::string::npos:end-start);
    if(!term.empty()&&text.find(term)==std::string::npos)return false;
    if(end==std::string::npos)break;
    start=end+1;
  }
  return true;
}
std::string host(const std::string& url){
  const auto scheme=url.find("://");
  if(scheme==std::string::npos)return url;
  const auto end=url.find('/',scheme+3);
  return url.substr(scheme+3,end==std::string::npos?std::string::npos:end-scheme-3);
}
std::string modelsSummary(const std::vector<std::string>& models,std::size_t limit=3){
  std::string text;
  for(std::size_t i=0;i<models.size()&&i<limit;++i)text+=(i?", ":"")+models[i];
  if(models.size()>limit)text+=", …";
  return text;
}
std::string countText(std::size_t count,const char* one,const char* many){
  return std::to_string(count)+" "+(count==1?one:many);
}
std::string badges(const providers::CatalogModel& model){
  std::string text;
  if(model.toolCall)text="tools";
  if(model.reasoning)text+=std::string(text.empty()?"":" · ")+"reasoning";
  return text;
}
std::string limits(const providers::CatalogModel& model){
  std::string text;
  if(model.context)text=providers::compactTokens(model.context)+" context";
  if(model.output)text+=std::string(text.empty()?"":" · ")+
    providers::compactTokens(model.output)+" output";
  return text;
}
// An endpoint configured by hand still opens in the walk-through.
CatalogProvider providerFromEndpoint(const EndpointRecord& endpoint){
  CatalogProvider provider;
  provider.id=endpoint.preset.empty()?"custom":endpoint.preset;
  provider.name=endpoint.label;provider.kind=endpoint.provider.empty()?"anthropic":
    endpoint.provider;
  provider.baseUrl=endpoint.baseUrl;provider.messagesPath=endpoint.messagesPath;
  provider.modelsPath=endpoint.modelsPath;provider.authHeader=endpoint.authHeader;
  provider.auth=endpoint.authHeader=="none"?"none":"key";
  provider.headers=endpoint.headers;provider.quirks=endpoint.quirks;
  return provider;
}

class ProvidersDialog final:public std::enable_shared_from_this<ProvidersDialog> {
public:
  ProvidersDialog(event_loop::Loop& owner,std::shared_ptr<ProvidersInputs> inputs,
      interactive::CommandUi::Done done,interactive::CommandUi::Ready ready,std::string notice):
      owner_(owner),inputs_(std::move(inputs)),catalog_(*inputs_->catalog),
      done_(std::move(done)),ready_(std::move(ready)),notice_(std::move(notice)){}
  void start(){
    auto self=shared_from_this();
    showList();
    owner_.queueWork([self]{
        auto endpoints=self->inputs_->readEndpoints();
        auto servers=self->inputs_->discover();
        self->loadedEndpoints_=std::move(endpoints);
        self->loadedServers_=std::move(servers);
      },[self](std::exception_ptr failure){
        if(!self->active_)return;
        self->discovering_=false;
        if(failure)self->notice_=errorText(failure);
        self->endpoints_=std::move(self->loadedEndpoints_);
        self->servers_=std::move(self->loadedServers_);
        if(self->page_==Page::List)self->showList();
      });
  }
private:
  enum class Page{List,Info,Key,KeyInput,BaseUrl,Testing,TestFailed,Models,ModelInput,Saving};
  enum class RowKind{Server,Configured,Catalog,Inert};
  struct Row{RowKind kind;std::size_t index;};
  enum class KeyChoice{UseEnv,Paste,OtherEnv,Keep};

  static std::string errorText(std::exception_ptr failure){
    try{std::rethrow_exception(failure);}
    catch(const std::exception& error){return error.what();}
    catch(...){return "Something went wrong";}
  }
  void close(std::optional<std::string> message){
    if(!active_)return;
    active_=false;testing_=false;
    done_(completion(std::move(message)));
  }
  void publish(interactive::UiRequest request){
    if(!active_)return;
    auto self=shared_from_this();
    ready_(std::move(request),[self](const interactive::UiAction& action){
      if(self->active_)self->act(action);
    });
  }
  std::string title(const std::string& step)const{
    return provider_.name+"  ·  "+step;
  }

  // ---- list ---------------------------------------------------------------
  void rebuildRows(){
    rows_.clear();
    if(discovering_){if(filter_.empty())rows_.push_back({RowKind::Inert,0});}
    else for(std::size_t i=0;i<servers_.size();++i){
      const auto& server=servers_[i];
      if(matches(server.name+" "+server.preset+" "+server.baseUrl+" "+
          modelsSummary(server.models,50),filter_))rows_.push_back({RowKind::Server,i});
    }
    configuredIds_.clear();
    for(const auto& [id,endpoint]:endpoints_){
      configuredIds_.push_back(id);
      if(matches(id+" "+endpoint.label+" "+endpoint.preset+" "+endpoint.baseUrl,filter_))
        rows_.push_back({RowKind::Configured,configuredIds_.size()-1});
    }
    for(const std::string section:{"Cloud","Local servers","Unsupported yet"})
      for(std::size_t i=0;i<catalog_.providers.size();++i){
        const auto& provider=catalog_.providers[i];
        if(providers::catalogSection(provider)!=section)continue;
        std::string haystack=provider.id+" "+provider.name;
        for(const auto& alias:provider.aliases)haystack+=" "+alias;
        if(matches(haystack,filter_))rows_.push_back({RowKind::Catalog,i});
      }
  }
  bool selectable(const Row& row)const{return row.kind!=RowKind::Inert;}
  void showList(){
    page_=Page::List;
    rebuildRows();
    if(rows_.empty())listIndex_=0;
    else listIndex_=std::min(listIndex_,rows_.size()-1);
    interactive::UiRequest view;
    view.kind=interactive::UiRequest::Kind::Select;
    view.id="providers";view.title="Providers";
    const auto supported=std::count_if(catalog_.providers.begin(),catalog_.providers.end(),
      [](const auto& provider){return provider.supported();});
    view.body=filter_.empty()?
      "Connect a model server. "+std::to_string(supported)+" providers in the catalog; "
      "type to search.":
      "Search: "+filter_+"▏ · "+countText(rows_.size(),"match","matches");
    if(!notice_.empty())view.body+="\n"+notice_;
    view.hint="type to search · ↑↓ choose · enter connect · esc close";
    view.bodyPreviewLines=2;
    const auto active=inputs_->activeProfile?inputs_->activeProfile():std::string{};
    for(const auto& row:rows_){
      interactive::UiChoice info;
      switch(row.kind){
        case RowKind::Inert:
          view.choices.push_back("Looking for local servers…");
          info.section="Found on this machine";
          info.detail="Ollama, LM Studio, llama-server, vLLM";
          break;
        case RowKind::Server:{
          const auto& server=servers_[row.index];
          view.choices.push_back(server.confirmed?server.name:server.name+" (unidentified)");
          info.section="Found on this machine";
          info.badge=countText(server.models.size(),"model","models");
          for(const auto& [id,endpoint]:endpoints_)
            if(endpoint.baseUrl==server.baseUrl){info.badge="configured";break;}
          info.detail=host(server.baseUrl)+(server.models.empty()?"":
            " · "+modelsSummary(server.models));
          break;
        }
        case RowKind::Configured:{
          const auto& id=configuredIds_[row.index];
          const auto& endpoint=endpoints_.at(id);
          view.choices.push_back(endpoint.label);
          info.section="Configured";
          info.badge=active.starts_with(id+"::")?"active":
            countText(endpoint.models.size(),"model","models");
          info.detail=id+" · "+host(endpoint.baseUrl);
          break;
        }
        case RowKind::Catalog:{
          const auto& provider=catalog_.providers[row.index];
          view.choices.push_back(provider.name);
          info.section=providers::catalogSection(provider);
          if(!provider.supported()){info.detail=provider.reason;break;}
          if(provider.comingSoon())info.badge="coming soon";
          else if(!providers::presentKeyEnv(provider,inputs_->environment).empty())
            info.badge="key found";
          info.detail=providers::shortKindLabel(provider.kind)+" · "+host(provider.baseUrl);
          break;
        }
      }
      view.choiceInfo.push_back(std::move(info));
    }
    if(rows_.empty()){
      view.choices.push_back("No provider matches \""+filter_+"\"");
      view.choiceInfo.push_back({"Backspace edits the search; ctrl+u clears it","",""});
    }
    view.selectedIndex=listIndex_;
    publish(std::move(view));
  }
  void listAction(const interactive::UiAction& action){
    if(action.kind==Kind::Cancel||action.kind==Kind::Back){close(std::nullopt);return;}
    if(action.kind==Kind::MoveSelection){
      if(!rows_.empty())listIndex_=std::min(action.index,rows_.size()-1);
      showList();return;
    }
    if(action.kind==Kind::TextChange){
      auto next=commands_indigenous::applyFilterKeys(filter_,action.text);
      if(next!=filter_){filter_=std::move(next);listIndex_=0;notice_.clear();showList();}
      return;
    }
    if(action.kind!=Kind::Activate||rows_.empty())return;
    listIndex_=std::min(action.index,rows_.size()-1);
    const auto row=rows_[listIndex_];
    if(!selectable(row))return;
    notice_.clear();
    setup_={};editingId_.clear();keepKey_=false;fromServer_=false;
    liveResult_.reset();modelFilter_.clear();modelIndex_=0;
    if(row.kind==RowKind::Server){
      const auto& server=servers_[row.index];
      const auto* known=catalog_.find(server.preset);
      if(!known){notice_="No catalog entry for "+server.name;showList();return;}
      provider_=*known;
      provider_.name=server.name;
      setup_.baseUrl=server.baseUrl;setup_.liveModels=server.models;
      setup_.liveContexts=server.contexts;
      fromServer_=true;
      providers::ConnectionResult found;
      found.reached=found.ok=true;found.models=server.models;
      found.message="Found "+providers::describeLocalServer(server);
      liveResult_=found;
      showModels();return;
    }
    if(row.kind==RowKind::Configured){
      editingId_=configuredIds_[row.index];
      const auto& endpoint=endpoints_.at(editingId_);
      const auto* known=catalog_.find(endpoint.preset);
      provider_=known&&known->kind==endpoint.provider?*known:providerFromEndpoint(endpoint);
      provider_.name=endpoint.label;
      setup_.baseUrl=endpoint.baseUrl;
      keepKey_=!endpoint.authToken.empty()||!endpoint.apiKeyEnv.empty();
      savedKey_=endpoint.authToken;savedKeyEnv_=endpoint.apiKeyEnv;
      if(keepKey_||provider_.auth=="key")showKey();
      else beginBaseUrl();
      return;
    }
    const auto& provider=catalog_.providers[row.index];
    provider_=provider;
    if(!provider.supported()){
      showInfo(provider.name,provider.name+" is not supported yet. "+provider.reason+
        (provider.doc.empty()?"":"\nDocs: "+provider.doc));
      return;
    }
    if(provider.comingSoon()){
      showInfo(provider.name,provider.name+" signs in with GitHub's device flow, which is "
        "coming soon. "+provider.note);
      return;
    }
    setup_.baseUrl=providers::resolvedBaseUrl(provider,inputs_->environment);
    if(providers::takesKey(provider))showKey();
    else beginBaseUrl();
  }

  // ---- info ---------------------------------------------------------------
  void showInfo(const std::string& heading,const std::string& body){
    page_=Page::Info;
    interactive::UiRequest view;
    view.kind=interactive::UiRequest::Kind::Select;
    view.id="providers-info";view.title=heading;view.body=body;
    view.choices={"Back to providers"};
    view.hint="enter back · esc back";
    publish(std::move(view));
  }

  // ---- key ----------------------------------------------------------------
  void showKey(){
    keyChoices_.clear();
    const auto present=providers::presentKeyEnv(provider_,inputs_->environment);
    if(!keepKey_&&present.empty()){showKeyInput(false);return;}
    page_=Page::Key;
    interactive::UiRequest view;
    view.kind=interactive::UiRequest::Kind::Select;
    view.id="providers-key";view.title=title("API key");
    view.body="Batbox saves the variable name when you pick one, never its value.";
    if(!notice_.empty())view.body+="\n"+notice_;
    if(keepKey_){
      keyChoices_.push_back({KeyChoice::Keep,{}});
      view.choices.push_back("Keep the saved key");
      view.choiceInfo.push_back({savedKeyEnv_.empty()?"stored in native-client.json":
        "read from $"+savedKeyEnv_,"",""});
    }
    for(const auto& name:present){
      keyChoices_.push_back({KeyChoice::UseEnv,name});
      view.choices.push_back("Use $"+name);
      view.choiceInfo.push_back({"found in your environment","",""});
    }
    keyChoices_.push_back({KeyChoice::Paste,{}});
    view.choices.push_back("Paste a key");
    view.choiceInfo.push_back({"saved privately (mode 0600)","",""});
    keyChoices_.push_back({KeyChoice::OtherEnv,{}});
    view.choices.push_back("Use another variable");
    view.choiceInfo.push_back({"type its name","",""});
    view.hint="↑↓ choose · enter continue · esc back";
    view.selectedIndex=std::min(keyIndex_,view.choices.size()-1);
    publish(std::move(view));
  }
  void keyAction(const interactive::UiAction& action){
    if(action.kind==Kind::Cancel||action.kind==Kind::Back){showList();return;}
    if(action.kind==Kind::MoveSelection){keyIndex_=action.index;showKey();return;}
    if(action.kind!=Kind::Activate||keyChoices_.empty())return;
    keyIndex_=std::min(action.index,keyChoices_.size()-1);
    const auto& [choice,name]=keyChoices_[keyIndex_];
    notice_.clear();
    switch(choice){
      case KeyChoice::Keep:setup_.key=savedKey_;setup_.keyEnv=savedKeyEnv_;beginBaseUrl();break;
      case KeyChoice::UseEnv:setup_.key.clear();setup_.keyEnv=name;beginBaseUrl();break;
      case KeyChoice::Paste:showKeyInput(false);break;
      case KeyChoice::OtherEnv:showKeyInput(true);break;
    }
  }
  void showKeyInput(bool variable){
    page_=Page::KeyInput;keyVariable_=variable;
    if(input_.empty()&&!variable)input_.clear();
    interactive::UiRequest view;
    view.kind=interactive::UiRequest::Kind::Form;
    view.id="providers-key-input";
    view.title=title(variable?"Key variable":"API key");
    view.sensitiveInput=!variable;
    if(variable){
      view.body="Variable  $"+input_+"▌\nBatbox reads it when it sends a request.";
    }else{
      const auto shown=std::min<std::size_t>(input_.size(),32);
      view.body="Key  "+std::string(shown,'*')+(input_.size()>32?"…":"")+"▌\n"
        "Saved to native-client.json, readable only by you.";
      if(!provider_.keyEnv.empty())view.body+=" Set $"+provider_.keyEnv.front()+
        " instead and Batbox will offer it.";
      if(!provider_.doc.empty())view.body+="\nGet a key: "+provider_.doc;
    }
    if(!notice_.empty())view.body+="\n"+notice_;
    view.hint=variable?"type the name · enter continue · esc back":
      "paste or type · enter continue · esc back";
    publish(std::move(view));
  }
  void keyInputAction(const interactive::UiAction& action){
    if(action.kind==Kind::Cancel||action.kind==Kind::Back){
      input_.clear();notice_.clear();
      if(keepKey_||!providers::presentKeyEnv(provider_,inputs_->environment).empty())showKey();
      else showList();
      return;
    }
    if(action.kind==Kind::TextChange){
      auto next=commands_indigenous::applyFilterKeys(input_,action.text);
      if(next.size()<4096)input_=std::move(next);
      showKeyInput(keyVariable_);return;
    }
    if(action.kind!=Kind::Activate)return;
    auto value=value::trim(input_);
    if(value.empty()){notice_=keyVariable_?"Type a variable name":"Paste a key first";
      showKeyInput(keyVariable_);return;}
    if(!keyVariable_&&value.front()=='$'){keyVariable_=true;value.erase(0,1);}
    if(keyVariable_){
      if(value.front()=='$')value.erase(0,1);
      if(value.find_first_of(" \t=")!=std::string::npos){
        notice_="A variable name has no spaces or =";showKeyInput(true);return;}
      setup_.keyEnv=value;setup_.key.clear();
    }else{setup_.key=value;setup_.keyEnv.clear();}
    input_.clear();notice_.clear();
    beginBaseUrl();
  }

  // ---- base URL -----------------------------------------------------------
  void beginBaseUrl(){
    input_=setup_.baseUrl.empty()?provider_.baseUrl:setup_.baseUrl;
    showBaseUrl();
  }
  void showBaseUrl(){
    page_=Page::BaseUrl;
    interactive::UiRequest view;
    view.kind=interactive::UiRequest::Kind::Form;
    view.id="providers-base-url";view.title=title("Base URL");
    view.body="URL  "+input_+"▌\nRequests go to "+(input_.empty()?"…":input_)+
      provider_.messagesPath+". Edit it for a self-hosted or regional server.";
    if(!provider_.note.empty())view.body+="\n"+provider_.note;
    if(!notice_.empty())view.body+="\n"+notice_;
    view.hint=provider_.kind=="bedrock"||provider_.kind=="vertex"?
      "type to edit · enter continue · ctrl+u clear · esc back":
      "type to edit · enter test connection · ctrl+u clear · esc back";
    publish(std::move(view));
  }
  void baseUrlAction(const interactive::UiAction& action){
    if(action.kind==Kind::Cancel||action.kind==Kind::Back){
      notice_.clear();
      if(providers::takesKey(provider_)||keepKey_)showKey();else showList();
      return;
    }
    if(action.kind==Kind::TextChange){
      auto next=commands_indigenous::applyFilterKeys(input_,action.text);
      next.erase(std::remove(next.begin(),next.end(),' '),next.end());
      if(next.size()<2048)input_=std::move(next);
      showBaseUrl();return;
    }
    if(action.kind!=Kind::Activate)return;
    auto value=value::trim(input_);
    while(value.ends_with('/'))value.pop_back();
    if(!value.starts_with("http://")&&!value.starts_with("https://")){
      notice_="Enter an http:// or https:// URL";showBaseUrl();return;}
    if(providers::needsBaseUrlEdit(value)){
      notice_="Replace the {placeholder} part of the URL first";showBaseUrl();return;}
    setup_.baseUrl=value;notice_.clear();
    if(provider_.kind=="bedrock"||provider_.kind=="vertex"){
      providers::ConnectionResult skipped;
      skipped.message=provider_.kind=="bedrock"?
        "Bedrock requests are signed with your AWS credentials when they are sent.":
        "Vertex requests use your Google credentials when they are sent.";
      liveResult_=skipped;setup_.liveModels.clear();
      showModels();return;
    }
    beginTest();
  }

  // ---- connection test -----------------------------------------------------
  EndpointRecord draft()const{
    auto probe=setup_;
    if(probe.model.empty())probe.model=provider_.models.empty()?"probe":provider_.models.front().id;
    return providers::buildEndpoint(provider_,probe,catalog_.defaultContext,catalog_.defaultOutput);
  }
  void beginTest(){
    EndpointRecord endpoint;
    try{endpoint=draft();}
    catch(const std::exception& error){notice_=error.what();showBaseUrl();return;}
    testUrl_.clear();
    try{testUrl_=application_config::modelsUrl(endpoint);}catch(...){}
    testing_=true;spinner_=0;++testGeneration_;
    showTesting();
    auto self=shared_from_this();
    auto result=std::make_shared<providers::ConnectionResult>();
    const auto generation=testGeneration_;
    owner_.queueWork([self,endpoint,result]{*result=self->inputs_->test(endpoint);},
      [self,result,generation](std::exception_ptr failure){
        if(!self->active_||generation!=self->testGeneration_||!self->testing_)return;
        self->testing_=false;
        if(failure){result->message=errorText(failure);result->ok=false;}
        self->liveResult_=*result;
        if(result->ok){
          self->setup_.liveModels=result->models;
          self->showModels();
        }else self->showTestFailed();
      });
    if(inputs_->animate)scheduleSpin(generation);
  }
  void scheduleSpin(std::uint64_t generation){
    auto weak=weak_from_this();
    owner_.setTimeout(100,[weak,generation]{
      auto self=weak.lock();
      if(!self||!self->active_||!self->testing_||generation!=self->testGeneration_)return;
      ++self->spinner_;self->showTesting();self->scheduleSpin(generation);
    },false);
  }
  void showTesting(){
    page_=Page::Testing;
    static const char* frames[]={"⠋","⠙","⠹","⠸","⠼","⠴","⠦","⠧","⠇","⠏"};
    interactive::UiRequest view;
    view.kind=interactive::UiRequest::Kind::Progress;
    view.id="providers-testing";view.title=title("Testing connection");
    view.body=std::string(frames[spinner_%10])+" GET "+testUrl_+
      "\nListing models only; no chat request is sent.";
    view.hint="esc back";
    publish(std::move(view));
  }
  void testingAction(const interactive::UiAction& action){
    if(action.kind==Kind::Cancel||action.kind==Kind::Back){
      testing_=false;++testGeneration_;showBaseUrl();
    }
  }
  void showTestFailed(){
    page_=Page::TestFailed;
    interactive::UiRequest view;
    view.kind=interactive::UiRequest::Kind::Select;
    view.id="providers-test-failed";view.title=title("Connection failed");
    view.body=liveResult_?liveResult_->message:"The connection test failed.";
    view.choices={"Try again","Edit the base URL","Change the key",
      "Pick a model from the catalog"};
    view.choiceInfo={{"",""," "},{setup_.baseUrl,"",""},
      {providers::takesKey(provider_)?"":"this server takes no key","",""},
      {"save without a live check","",""}};
    view.hint="↑↓ choose · enter select · esc back";
    view.selectedIndex=std::min<std::size_t>(failedIndex_,3);
    publish(std::move(view));
  }
  void testFailedAction(const interactive::UiAction& action){
    if(action.kind==Kind::Cancel||action.kind==Kind::Back){input_=setup_.baseUrl;showBaseUrl();return;}
    if(action.kind==Kind::MoveSelection){failedIndex_=std::min<std::size_t>(action.index,3);
      showTestFailed();return;}
    if(action.kind!=Kind::Activate)return;
    failedIndex_=std::min<std::size_t>(action.index,3);
    switch(failedIndex_){
      case 0:beginTest();break;
      case 1:input_=setup_.baseUrl;showBaseUrl();break;
      case 2:showKey();break;
      default:setup_.liveModels.clear();showModels();break;
    }
  }

  // ---- models -------------------------------------------------------------
  struct ModelRow{std::string id,detail,badge;bool manual=false;};
  std::vector<ModelRow> modelRows()const{
    std::vector<ModelRow> rows;
    std::set<std::string> seen;
    const auto add=[&](const std::string& id){
      if(id.empty()||!seen.insert(id).second)return;
      if(!matches(id,modelFilter_))return;
      ModelRow row{id,{},{}};
      if(const auto* known=provider_.model(id)){
        row.detail=limits(*known);row.badge=badges(*known);
        if(known->name!=known->id)row.detail=known->name+(row.detail.empty()?"":" · "+row.detail);
      }else if(const auto context=setup_.liveContexts.find(id);context!=setup_.liveContexts.end())
        row.detail=providers::compactTokens(context->second)+" context";
      rows.push_back(std::move(row));
    };
    if(!setup_.liveModels.empty()){
      std::set<std::string> live(setup_.liveModels.begin(),setup_.liveModels.end());
      for(const auto& model:provider_.models)if(live.contains(model.id))add(model.id);
      for(const auto& id:setup_.liveModels)add(id);
    }else for(const auto& model:provider_.models)add(model.id);
    rows.push_back({"Enter a model ID…","type any model the server accepts","",true});
    return rows;
  }
  void showModels(){
    page_=Page::Models;
    const auto rows=modelRows();
    modelIndex_=std::min(modelIndex_,rows.size()-1);
    interactive::UiRequest view;
    view.kind=interactive::UiRequest::Kind::Select;
    view.id="providers-models";view.title=title("Model");
    std::string status;
    if(liveResult_&&liveResult_->ok)status=liveResult_->message;
    else if(liveResult_)status=liveResult_->message+" Showing the catalog.";
    else status="From the catalog.";
    if(!status.empty()&&!status.ends_with('.'))status+='.';
    view.body=modelFilter_.empty()?status+" Type to search.":
      "Search: "+modelFilter_+"▏ · "+countText(rows.size()-1,"match","matches");
    if(!notice_.empty())view.body+="\n"+notice_;
    view.bodyPreviewLines=2;
    view.hint="type to search · ↑↓ choose · enter save · esc back";
    for(const auto& row:rows){
      view.choices.push_back(row.id);
      view.choiceInfo.push_back({row.detail,row.badge,row.manual?"Other":""});
    }
    view.selectedIndex=modelIndex_;
    publish(std::move(view));
  }
  void modelsAction(const interactive::UiAction& action){
    if(action.kind==Kind::Cancel||action.kind==Kind::Back){
      modelFilter_.clear();notice_.clear();
      if(fromServer_)showList();
      else{input_=setup_.baseUrl;showBaseUrl();}
      return;
    }
    if(action.kind==Kind::MoveSelection){modelIndex_=action.index;showModels();return;}
    if(action.kind==Kind::TextChange){
      auto next=commands_indigenous::applyFilterKeys(modelFilter_,action.text);
      if(next!=modelFilter_){modelFilter_=std::move(next);modelIndex_=0;showModels();}
      return;
    }
    if(action.kind!=Kind::Activate)return;
    const auto rows=modelRows();
    modelIndex_=std::min(action.index,rows.size()-1);
    const auto& row=rows[modelIndex_];
    if(row.manual){input_=modelFilter_;showModelInput();return;}
    save(row.id);
  }
  void showModelInput(){
    page_=Page::ModelInput;
    interactive::UiRequest view;
    view.kind=interactive::UiRequest::Kind::Form;
    view.id="providers-model-input";view.title=title("Model ID");
    view.body="Model  "+input_+"▌\nThe exact id the server expects, such as "+
      (provider_.models.empty()?std::string("qwen3-coder:30b"):provider_.models.front().id)+".";
    if(!notice_.empty())view.body+="\n"+notice_;
    view.hint="type the id · enter save · esc back";
    publish(std::move(view));
  }
  void modelInputAction(const interactive::UiAction& action){
    if(action.kind==Kind::Cancel||action.kind==Kind::Back){notice_.clear();showModels();return;}
    if(action.kind==Kind::TextChange){
      auto next=commands_indigenous::applyFilterKeys(input_,action.text);
      if(next.size()<1024)input_=std::move(next);
      showModelInput();return;
    }
    if(action.kind!=Kind::Activate)return;
    const auto value=value::trim(input_);
    if(value.empty()){notice_="Type a model id";showModelInput();return;}
    save(value);
  }

  // ---- save ---------------------------------------------------------------
  void save(const std::string& model){
    setup_.model=model;
    EndpointRecord endpoint;
    std::string id;
    try{
      endpoint=providers::buildEndpoint(provider_,setup_,catalog_.defaultContext,
        catalog_.defaultOutput);
      if(!editingId_.empty()){
        id=editingId_;
        const auto& previous=endpoints_.at(editingId_);
        endpoint.label=previous.label;
        if(endpoint.preset=="custom")endpoint.preset=previous.preset;
      }else id=providers::uniqueEndpointId(endpoints_,provider_.id);
      application_config::validateEndpoint(id,endpoint);
    }catch(const std::exception& error){
      notice_=error.what();
      if(page_==Page::ModelInput)showModelInput();else showModels();
      return;
    }
    page_=Page::Saving;
    interactive::UiRequest view;
    view.kind=interactive::UiRequest::Kind::Progress;
    view.id="providers-saving";view.title=title("Saving");
    view.body="Writing the endpoint to native-client.json (private)…";
    publish(std::move(view));
    auto self=shared_from_this();
    auto result=std::make_shared<providers::SaveResult>();
    auto endpoints=endpoints_;
    owner_.queueWork([self,result,endpoints,id,endpoint,model]{
        *result=self->inputs_->save(endpoints,id,endpoint,model);
      },[self,result,model](std::exception_ptr failure){
        if(!self->active_)return;
        if(failure){self->notice_=errorText(failure);self->showModels();return;}
        self->applySaved(*result);
        self->close(providers::addedNotice(result->endpointId,model));
      });
  }
  void applySaved(const providers::SaveResult& saved){
    if(inputs_->model){
      std::set<std::string> retained;
      for(const auto& wire:saved.endpoint.models){
        const auto profile=application_config::profileId(saved.endpointId,wire);
        retained.insert(profile);
        inputs_->model->upsertRuntimeProfile(
          application_config::runtimeProfile(profile,saved.endpoint,wire),
          application_config::credential(saved.endpoint));
      }
      if(const auto previous=endpoints_.find(saved.endpointId);previous!=endpoints_.end())
        for(const auto& wire:previous->second.models){
          const auto profile=application_config::profileId(saved.endpointId,wire);
          if(!retained.contains(profile))inputs_->model->removeRuntimeProfile(profile);
        }
    }
    if(inputs_->store)inputs_->store->setState([profile=saved.profile](
        const app_state::AppStateRef& previous){
      auto next=std::make_shared<app_state::AppState>(previous->shallowCopy());
      next->fields->set("mainLoopModel",profile);
      next->fields->set("mainLoopModelForSession",nullptr);
      return next;
    });
    endpoints_=saved.endpoints;
    if(inputs_->onSaved)inputs_->onSaved(saved);
  }

  void act(const interactive::UiAction& action){
    switch(page_){
      case Page::List:listAction(action);break;
      case Page::Info:
        if(action.kind==Kind::Activate||action.kind==Kind::Cancel||action.kind==Kind::Back)
          showList();
        break;
      case Page::Key:keyAction(action);break;
      case Page::KeyInput:keyInputAction(action);break;
      case Page::BaseUrl:baseUrlAction(action);break;
      case Page::Testing:testingAction(action);break;
      case Page::TestFailed:testFailedAction(action);break;
      case Page::Models:modelsAction(action);break;
      case Page::ModelInput:modelInputAction(action);break;
      case Page::Saving:break;
    }
  }

  event_loop::Loop& owner_;
  std::shared_ptr<ProvidersInputs> inputs_;
  const providers::Catalog& catalog_;
  interactive::CommandUi::Done done_;
  interactive::CommandUi::Ready ready_;
  std::string notice_;
  Page page_=Page::List;
  bool active_=true,discovering_=true,testing_=false,keepKey_=false,keyVariable_=false;
  bool fromServer_=false;
  std::vector<providers::LocalServer> servers_,loadedServers_;
  std::map<std::string,EndpointRecord> endpoints_,loadedEndpoints_;
  std::vector<std::string> configuredIds_;
  std::vector<Row> rows_;
  std::vector<std::pair<KeyChoice,std::string>> keyChoices_;
  std::string filter_,modelFilter_,input_,editingId_,savedKey_,savedKeyEnv_,testUrl_;
  std::size_t listIndex_=0,keyIndex_=0,modelIndex_=0,failedIndex_=0,spinner_=0;
  std::uint64_t testGeneration_=0;
  CatalogProvider provider_;
  providers::Setup setup_;
  std::optional<providers::ConnectionResult> liveResult_;
};
} // namespace

std::shared_ptr<ProvidersInputs> completeProvidersInputs(ProvidersInputs inputs){
  auto shared=std::make_shared<ProvidersInputs>(std::move(inputs));
  if(!shared->catalog)shared->catalog=&providers::bundledCatalog();
  if(!shared->environment)shared->environment=providers::processEnvironment();
  if(!shared->discover)shared->discover=[]{return providers::discoverLocalServers();};
  if(!shared->test)shared->test=[](const EndpointRecord& endpoint){
    return providers::testConnection(endpoint);};
  if(!shared->readEndpoints)shared->readEndpoints=[path=shared->configPath,
      legacy=shared->legacyPath]{
    auto source=path;
    if(!std::filesystem::exists(source)&&!legacy.empty()&&std::filesystem::exists(legacy))
      source=legacy;
    auto loaded=std::filesystem::exists(source)?application_config::readEndpoints(source):
      std::nullopt;
    return loaded?*loaded:std::map<std::string,EndpointRecord>{};
  };
  if(!shared->save)shared->save=[path=shared->configPath,legacy=shared->legacyPath](
      std::map<std::string,EndpointRecord> endpoints,const std::string& id,
      EndpointRecord endpoint,const std::string& model){
    return providers::saveEndpoint(path,legacy,std::move(endpoints),id,std::move(endpoint),model);
  };
  return shared;
}

void startProvidersDialog(event_loop::Loop& owner,std::shared_ptr<ProvidersInputs> inputs,
    interactive::CommandUi::Done done,interactive::CommandUi::Ready ready,std::string notice){
  std::make_shared<ProvidersDialog>(owner,std::move(inputs),std::move(done),
    std::move(ready),std::move(notice))->start();
}

std::string providersText(const std::vector<providers::LocalServer>& servers,
    const std::map<std::string,EndpointRecord>& endpoints){
  std::string text="Model servers on this machine\n";
  if(servers.empty())text+="  none found\n";
  for(const auto& server:servers)
    text+="  "+providers::describeLocalServer(server)+" · "+server.baseUrl+"\n";
  text+="Configured endpoints\n";
  if(endpoints.empty())text+="  none yet\n";
  for(const auto& [id,endpoint]:endpoints)
    text+="  "+id+" · "+endpoint.label+" · "+countText(endpoint.models.size(),"model","models")+"\n";
  text+="Run /providers in the interactive terminal, or batbox providers add <id>, to connect "
    "one of "+std::to_string(providers::bundledCatalog().providers.size())+" providers.";
  return text;
}

user_input::CommandEntry makeProvidersCommand(event_loop::Loop& owner,
    std::shared_ptr<user_input::UiCommandRouter> router,ProvidersInputs inputs){
  if(!router)throw std::invalid_argument("Providers command needs a router");
  auto shared=completeProvidersInputs(std::move(inputs));
  Json descriptor={{"type","local-jsx"},{"name","providers"},
    {"aliases",Json::array({"connect"})},
    {"description","Connect a model server or provider: local servers, catalog, keys"},
    {"supportsNonInteractive",true}};
  return user_input::bindUiCommand(std::move(descriptor),std::move(router),
    [shared,&owner](const std::string& args){
      return [shared,&owner,args](auto done,auto ready,auto error){
        if(shared->interactive){
          startProvidersDialog(owner,shared,done,ready,value::trim(args)=="not-connected"?
            "Connect a model before sending a prompt.":"");
          return;
        }
        auto servers=std::make_shared<std::vector<providers::LocalServer>>();
        auto endpoints=std::make_shared<std::map<std::string,EndpointRecord>>();
        owner.queueWork([shared,servers,endpoints]{
            *servers=shared->discover();*endpoints=shared->readEndpoints();},
          [servers,endpoints,done,error](std::exception_ptr failure){
            if(failure){
              try{std::rethrow_exception(failure);}
              catch(const std::exception& issue){error(issue.what());}
              catch(...){error("Provider checks failed");}
              return;
            }
            done(completion(providersText(*servers,*endpoints)));
          });
      };
    });
}
} // namespace batbox::application_commands
