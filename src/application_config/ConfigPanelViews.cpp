#include "application_config/ConfigPanelInternal.hpp"
#include "application_config/Discovery.hpp"
#include "application/Async.hpp"
#include <algorithm>
#include <set>

namespace batbox::application_config {
void Panel::showHome(){
    page_=returnPage_=Page::Home;
    std::vector<std::string> ids;
    for(const auto& [id,endpoint]:endpoints_)ids.push_back(id);
    const auto total=ids.size()+3;
    homeIndex_=std::min(homeIndex_,total-1);
    const auto capacity=static_cast<std::size_t>(pageSize());
    const auto begin=homeIndex_>=capacity?homeIndex_-capacity+1:0;
    homeBegin_=begin;
    interactive::UiRequest view;
    view.kind=interactive::UiRequest::Kind::Select;
    view.id="endpoint-home";view.title="◆  BATBOX  /  CONFIG";
    view.body="ENDPOINTS  "+std::to_string(ids.size())+
      " configured  ·  active "+selectedProfile()+
      "\n↑↓ navigate  ·  Enter open  ·  Esc close";
    if(!notice_.empty())view.body+="\n"+notice_;
    for(auto i=begin;i<std::min(total,begin+capacity);++i){
      if(i<ids.size()){
        const auto& endpoint=endpoints_.at(ids[i]);
        view.choices.push_back((selectedProfile().starts_with(ids[i]+"::")?"● ":"○ ")+
          endpoint.label+"  ·  "+std::to_string(endpoint.models.size())+" models");
      }else if(i==ids.size())view.choices.push_back("＋ Add endpoint");
      else if(i==ids.size()+1)view.choices.push_back("⚙ Request settings");
      else view.choices.push_back("Done");
    }
    view.selectedIndex=homeIndex_-begin;
    auto self=shared_from_this();
    publish(std::move(view),[self,ids=std::move(ids)](const auto& event){
      if(event.kind==interactive::UiAction::Kind::Cancel||
          event.kind==interactive::UiAction::Kind::Back){self->close();return;}
      if(event.kind==interactive::UiAction::Kind::MoveSelection){
        self->homeIndex_=movedIndex(self->homeIndex_,self->homeBegin_,
          std::min<std::size_t>(ids.size()+3-self->homeBegin_,self->pageSize()),
          ids.size()+3,event.index);
        self->showHome();return;
      }
      if(event.kind!=interactive::UiAction::Kind::Activate)return;
      const auto index=self->homeBegin_+event.index;
      if(index<ids.size()){
        self->endpointId_=ids[index];self->detailIndex_=0;self->showDetail();
      }else if(index==ids.size())self->beginNew();
      else if(index==ids.size()+1)self->showSettings();
      else self->close();
    });
  }
void Panel::showDetail(){
    page_=returnPage_=Page::Detail;
    if(!endpoints_.contains(endpointId_)){showHome();return;}
    const auto& endpoint=endpoints_.at(endpointId_);
    interactive::UiRequest view;view.kind=interactive::UiRequest::Kind::Select;
    view.id="endpoint-detail";view.title="◆  "+endpoint.label;
    view.body=endpoint.baseUrl+"\nMessages  "+endpoint.messagesPath+
      "  ·  auth "+endpoint.authHeader+
      "  ·  key "+(endpoint.authToken.empty()&&endpoint.apiKeyEnv.empty()?"unset":"••••")+
      "\nLimits  "+std::to_string(endpoint.contextWindow)+" context / "+
      std::to_string(endpoint.maxOutputTokens)+" output tokens"+
      "\nCapabilities  thinking "+(endpoint.thinking?"on":"off")+
      " · cache "+(endpoint.poetryStashing?"on":"off")+
      " · adaptive "+(endpoint.adaptiveThinking?"on":"off")+
      "\nDiscovery  "+modelsUrl(endpoint)+
      "\nEsc back  ·  keys remain hidden";
    if(!notice_.empty())view.body+="\n"+notice_;
    view.choices={"Browse models ("+std::to_string(endpoint.models.size())+")",
      "↻ Discover / refresh models","＋ Add model ID","Edit endpoint",
      "Delete endpoint","Back"};
    view.selectedIndex=std::min<std::size_t>(detailIndex_,view.choices.size()-1);
    auto self=shared_from_this();
    publish(std::move(view),[self](const auto& event){
      if(event.kind==interactive::UiAction::Kind::Cancel||
          event.kind==interactive::UiAction::Kind::Back){self->showHome();return;}
      if(event.kind==interactive::UiAction::Kind::MoveSelection){
        self->detailIndex_=std::min<std::size_t>(event.index,5);self->showDetail();return;
      }
      if(event.kind==interactive::UiAction::Kind::TextChange){
        if(event.text=="r")self->refresh();
        else if(event.text=="m")self->input(Field::Model,Page::Detail);
        return;
      }
      if(event.kind!=interactive::UiAction::Kind::Activate)return;
      self->detailIndex_=std::min<std::size_t>(event.index,5);
      switch(self->detailIndex_){
        case 0:self->modelIndex_=0;self->showModels();break;
        case 1:self->refresh();break;
        case 2:self->input(Field::Model,Page::Detail);break;
        case 3:self->beginEdit();break;
        case 4:self->confirmDelete();break;
        default:self->showHome();break;
      }
    });
  }
std::vector<std::string> Panel::filteredModels()const{
    std::vector<std::string> result;
    const auto filter=lower(search_);
    for(const auto& model:endpoints_.at(endpointId_).models)
      if(filter.empty()||lower(model).find(filter)!=std::string::npos)
        result.push_back(model);
    return result;
  }
void Panel::showModels(){
    page_=returnPage_=Page::Models;
    if(!endpoints_.contains(endpointId_)){showHome();return;}
    const auto models=filteredModels();
    const auto total=models.size()+3;
    modelIndex_=std::min(modelIndex_,total-1);
    const auto capacity=static_cast<std::size_t>(pageSize());
    const auto begin=modelIndex_>=capacity?modelIndex_-capacity+1:0;
    modelBegin_=begin;
    interactive::UiRequest view;view.kind=interactive::UiRequest::Kind::Select;
    view.id="endpoint-models";view.title="◆  "+endpoints_.at(endpointId_).label+" / MODELS";
    view.body=std::to_string(models.size())+" shown of "+
      std::to_string(endpoints_.at(endpointId_).models.size())+
      "  ·  / search  ·  r refresh  ·  m add  ·  Esc back";
    if(!search_.empty())view.body+="\nFilter  "+search_;
    if(!notice_.empty())view.body+="\n"+notice_;
    if(models.empty())view.body+="\nNo models shown. Add an ID manually or refresh discovery.";
    for(auto i=begin;i<std::min(total,begin+capacity);++i){
      if(i<models.size())view.choices.push_back(
        (selectedProfile()==profileId(endpointId_,models[i])?"● ":"○ ")+models[i]);
      else if(i==models.size())view.choices.push_back("＋ Add model ID");
      else if(i==models.size()+1)view.choices.push_back("↻ Refresh discovery");
      else view.choices.push_back("Back");
    }
    view.selectedIndex=modelIndex_-begin;
    auto self=shared_from_this();
    publish(std::move(view),[self,models=std::move(models)](const auto& event){
      if(event.kind==interactive::UiAction::Kind::Cancel||
          event.kind==interactive::UiAction::Kind::Back){self->showDetail();return;}
      if(event.kind==interactive::UiAction::Kind::MoveSelection){
        self->modelIndex_=movedIndex(self->modelIndex_,self->modelBegin_,
          std::min<std::size_t>(models.size()+3-self->modelBegin_,self->pageSize()),
          models.size()+3,event.index);
        self->showModels();return;
      }
      if(event.kind==interactive::UiAction::Kind::TextChange){
        if(event.text=="/")self->input(Field::Search,Page::Models);
        else if(event.text=="r")self->refresh();
        else if(event.text=="m")self->input(Field::Model,Page::Models);
        else if(event.text=="c"){self->search_.clear();self->modelIndex_=0;self->showModels();}
        return;
      }
      if(event.kind!=interactive::UiAction::Kind::Activate)return;
      const auto index=self->modelBegin_+event.index;
      if(index<models.size()){
        self->notice_="Selecting "+models[index];
        if(self->modelMode_)self->closeAfterSave_="Set model to "+models[index];
        self->persist(self->endpoints_,profileId(self->endpointId_,models[index]));
      }else if(index==models.size())self->input(Field::Model,Page::Models);
      else if(index==models.size()+1)self->refresh();
      else self->showDetail();
    });
  }
void Panel::refresh(){
    if(busy_||!endpoints_.contains(endpointId_))return;
    const auto endpoint=endpoints_.at(endpointId_);
    busy_=true;page_=Page::Progress;
    interactive::UiRequest view;view.kind=interactive::UiRequest::Kind::Progress;
    view.id="endpoint-discovery";view.title="Discovering models";
    view.body="GET "+modelsUrl(endpoint)+"\nWaiting for a model list…";
    publish(std::move(view),[](const auto&){});
    auto self=shared_from_this();
    application::runInWorker<DiscoveryResult>(owner_,[endpoint]{return discoverModels(endpoint);})
      .then([self](const DiscoveryResult* result,const std::exception_ptr& error){
        if(!self->active_)return;
        self->busy_=false;
        if(error){self->problem(error);return;}
        if(!result||!result->success){
          self->notice_=result?result->error:"Model discovery failed";
          self->showCurrent();return;
        }
        if(result->models.empty()){
          self->notice_="The endpoint returned no models. Enter a model ID manually.";
          self->showCurrent();return;
        }
        auto next=self->endpoints_;
        auto& list=next.at(self->endpointId_).models;
        std::set<std::string> known(list.begin(),list.end());
        std::size_t added=0;
        for(const auto& id:result->models)
          if(known.insert(id).second){list.push_back(id);++added;}
        self->notice_="Discovered "+std::to_string(result->models.size())+
          " models ("+std::to_string(added)+" new).";
        self->persist(std::move(next),self->selectedProfile());
      });
  }
void Panel::showSettings(){
    page_=returnPage_=Page::Settings;
    interactive::UiRequest view;view.kind=interactive::UiRequest::Kind::Select;
    view.id="endpoint-settings";view.title="⚙  REQUEST SETTINGS";
    view.body="Endpoint token limits are edited on each endpoint.\n"
      "Compaction changes apply to the next request. Other defaults apply on next launch.";
    if(!notice_.empty())view.body+="\n"+notice_;
    view.choices={"Auto compact at  "+std::to_string(config_->compactAt)+" tokens",
      "Thinking default  "+std::string(config_->thinking?"on":"off"),
      "Max subagents  "+std::to_string(config_->maxSubagents),
      "Concurrent subagents  "+std::to_string(config_->maxConcurrentSubagents),
      "Back"};
    view.selectedIndex=std::min<std::size_t>(settingsIndex_,4);
    auto self=shared_from_this();
    publish(std::move(view),[self](const auto& event){
      if(event.kind==interactive::UiAction::Kind::Cancel||
          event.kind==interactive::UiAction::Kind::Back){self->showHome();return;}
      if(event.kind==interactive::UiAction::Kind::MoveSelection){
        self->settingsIndex_=std::min<std::size_t>(event.index,4);self->showSettings();return;}
      if(event.kind!=interactive::UiAction::Kind::Activate)return;
      self->settingsIndex_=std::min<std::size_t>(event.index,4);
      if(self->settingsIndex_==0)self->input(Field::Compact,Page::Settings);
      else if(self->settingsIndex_==1)
        self->persist(self->endpoints_,self->selectedProfile(),
          Json{{"thinking",!self->config_->thinking}});
      else if(self->settingsIndex_==2)
        self->input(Field::Subagents,Page::Settings);
      else if(self->settingsIndex_==3)
        self->input(Field::ConcurrentSubagents,Page::Settings);
      else self->showHome();
    });
  }
void Panel::confirmDelete(){
    page_=Page::ConfirmDelete;returnPage_=Page::Detail;
    interactive::UiRequest view;view.kind=interactive::UiRequest::Kind::Confirm;
    view.id="endpoint-delete";view.title="Delete "+endpoints_.at(endpointId_).label+"?";
    view.body="This removes its configured model routes and saved API key.";
    view.choices={"Cancel","Delete endpoint"};view.selectedIndex=0;
    auto self=shared_from_this();
    publish(std::move(view),[self](const auto& event){
      if(event.kind==interactive::UiAction::Kind::Cancel||
          event.kind==interactive::UiAction::Kind::Back){self->showDetail();return;}
      if(event.kind==interactive::UiAction::Kind::Activate&&event.index==1){
        auto next=self->endpoints_;next.erase(self->endpointId_);
        self->returnPage_=Page::Home;
        self->persist(std::move(next),self->selectedProfile());
      }else if(event.kind==interactive::UiAction::Kind::Activate)self->showDetail();
    });
  }
}
