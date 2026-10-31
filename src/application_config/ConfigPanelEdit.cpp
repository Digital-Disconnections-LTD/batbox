#include "application_config/ConfigPanelInternal.hpp"
#include "runtime/JsonValue.hpp"
#include <algorithm>
#include <stdexcept>

namespace batbox::application_config {
namespace {constexpr std::size_t kEditChoices=18;}
void Panel::beginNew(){
    new_=true;editing_=Endpoint{};endpointId_.clear();
    input(Field::Label,Page::Home);
  }
void Panel::beginEdit(){new_=false;editing_=endpoints_.at(endpointId_);editIndex_=0;showEdit();}
void Panel::showEdit(){
    page_=returnPage_=Page::Edit;
    interactive::UiRequest view;view.kind=interactive::UiRequest::Kind::Select;
    view.id="endpoint-edit";view.title="◆  EDIT  /  "+editing_.label;
    view.body="Changes take effect after Save. Tokens remain hidden.\n"
      "Context and output limits are operator supplied; discovery gives IDs only.";
    if(!notice_.empty())view.body+="\n"+notice_;
    view.choices={"Label  "+editing_.label,"Base URL  "+editing_.baseUrl,
      "Messages path  "+editing_.messagesPath,
      "Models path  "+(editing_.modelsPath.empty()?"automatic":editing_.modelsPath),
      "Authentication  "+editing_.authHeader,
      std::string("API key  ")+(editing_.authToken.empty()?"unset":"••••"),
      "API key env  "+(editing_.apiKeyEnv.empty()?"unset":editing_.apiKeyEnv),
      "Context tokens  "+std::to_string(editing_.contextWindow),
      "Max output tokens  "+std::to_string(editing_.maxOutputTokens),
      "Thinking support  "+std::string(editing_.thinking?"on":"off"),
      "Prompt caching  "+std::string(editing_.poetryStashing?"on":"off"),
      "Adaptive thinking  "+std::string(editing_.adaptiveThinking?"on":"off"),
      "Structured outputs  "+std::string(editing_.structuredOutputs?"on":"off"),
      "Effort support  "+std::string(editing_.effort?"on":"off"),
      "Send disabled thinking  "+std::string(editing_.sendDisabledThinking?"on":"off"),
      "Custom headers  "+std::to_string(editing_.headers.size())+" configured (values hidden)",
      "Save endpoint","Cancel"};
    const auto capacity=static_cast<std::size_t>(pageSize());
    editIndex_=std::min<std::size_t>(editIndex_,view.choices.size()-1);
    const auto begin=editIndex_>=capacity?editIndex_-capacity+1:0;
    editBegin_=begin;
    const auto last=std::min(view.choices.size(),begin+capacity);
    view.choices=std::vector<std::string>(view.choices.begin()+begin,view.choices.begin()+last);
    view.selectedIndex=editIndex_-begin;
    auto self=shared_from_this();
    publish(std::move(view),[self](const auto& event){
      if(event.kind==interactive::UiAction::Kind::Cancel||
          event.kind==interactive::UiAction::Kind::Back){self->showDetail();return;}
      if(event.kind==interactive::UiAction::Kind::MoveSelection){
        self->editIndex_=movedIndex(self->editIndex_,self->editBegin_,
          std::min<std::size_t>(kEditChoices-self->editBegin_,self->pageSize()),
          kEditChoices,
          event.index);
        self->showEdit();return;
      }
      if(event.kind!=interactive::UiAction::Kind::Activate)return;
      self->editIndex_=std::min<std::size_t>(self->editBegin_+event.index,kEditChoices-1);
      switch(self->editIndex_){
        case 0:self->input(Field::Label,Page::Edit);break;
        case 1:self->input(Field::BaseUrl,Page::Edit);break;
        case 2:self->input(Field::MessagesPath,Page::Edit);break;
        case 3:self->input(Field::ModelsPath,Page::Edit);break;
        case 4:self->editing_.authHeader=self->editing_.authHeader=="authorization"?
          "x-api-key":self->editing_.authHeader=="x-api-key"?"none":"authorization";
          self->showEdit();break;
        case 5:self->input(Field::Token,Page::Edit);break;
        case 6:self->input(Field::KeyEnv,Page::Edit);break;
        case 7:self->input(Field::Context,Page::Edit);break;
        case 8:self->input(Field::Output,Page::Edit);break;
        case 9:self->editing_.thinking=!self->editing_.thinking;self->showEdit();break;
        case 10:self->editing_.poetryStashing=!self->editing_.poetryStashing;self->showEdit();break;
        case 11:self->editing_.adaptiveThinking=!self->editing_.adaptiveThinking;
          self->showEdit();break;
        case 12:self->editing_.structuredOutputs=!self->editing_.structuredOutputs;
          self->showEdit();break;
        case 13:self->editing_.effort=!self->editing_.effort;
          self->showEdit();break;
        case 14:self->editing_.sendDisabledThinking=!self->editing_.sendDisabledThinking;
          self->showEdit();break;
        case 15:self->input(Field::Headers,Page::Edit);break;
        case 16:{auto next=self->endpoints_;next[self->endpointId_]=self->editing_;
          self->returnPage_=Page::Detail;
          self->persist(std::move(next),self->selectedProfile());break;}
        default:self->showDetail();break;
      }
    });
  }
void Panel::input(Field field,Page back){
    field_=field;inputBack_=back;returnPage_=back;input_.clear();showInput();
  }
void Panel::showInput(){
    page_=Page::Input;
    static constexpr const char* labels[]={"Label","Base URL","Messages path",
      "Models path","API key","API key environment variable","Context tokens",
      "Max output tokens","Model ID","Search models","Auto compact tokens",
      "Headers JSON","Max subagents","Concurrent subagents"};
    interactive::UiRequest view;view.kind=interactive::UiRequest::Kind::Form;
    view.id="endpoint-input";view.title=std::string("◆  ")+labels[static_cast<int>(field_)];
    view.sensitiveInput=field_==Field::Token||field_==Field::Headers;
    view.body="Enter to apply  ·  Esc back  ·  Backspace deletes";
    if(view.sensitiveInput)view.body+="\nValue  "+
      std::string(std::min<std::size_t>(input_.size(),32),'*')+
      (input_.size()>32?"…":"")+"▌";
    else view.body+="\nValue  "+input_+"▌";
    if(input_.empty())view.body+="\nBlank keeps the current value. Type <clear> to erase optional fields.";
    if(!notice_.empty())view.body+="\n"+notice_;
    auto self=shared_from_this();
    publish(std::move(view),[self](const auto& event){
      if(event.kind==interactive::UiAction::Kind::Cancel||
          event.kind==interactive::UiAction::Kind::Back){self->backFromInput();return;}
      if(event.kind==interactive::UiAction::Kind::TextChange){
        if(event.text=="\x7f"||event.text=="\b"){
          if(!self->input_.empty()){
            const auto removed=static_cast<unsigned char>(self->input_.back());
            self->input_.pop_back();
            if((removed&0xc0)==0x80){
              while(!self->input_.empty()&&
                  (static_cast<unsigned char>(self->input_.back())&0xc0)==0x80)
                self->input_.pop_back();
              if(!self->input_.empty())self->input_.pop_back();
            }
          }
        }else if(self->input_.size()+event.text.size()<4096)
          self->input_+=event.text;
        self->showInput();return;
      }
      if(event.kind==interactive::UiAction::Kind::Activate)self->submitInput();
    });
  }
void Panel::backFromInput(){
    if(new_){selfNewCancelled();return;}
    switch(inputBack_){
      case Page::Models:showModels();break;
      case Page::Detail:showDetail();break;
      case Page::Edit:showEdit();break;
      case Page::Settings:showSettings();break;
      default:showHome();break;
    }
  }
void Panel::selfNewCancelled(){new_=false;showHome();}
void Panel::submitInput(){
    try{
      const auto value=value::trim(input_);
      if(field_==Field::Search){search_=value;modelIndex_=0;showModels();return;}
      if(field_==Field::Model){
        if(value.empty())throw std::invalid_argument("Enter a model ID");
        auto next=endpoints_;
        auto& models=next.at(endpointId_).models;
        if(std::find(models.begin(),models.end(),value)==models.end())models.push_back(value);
        notice_="Added model ID "+value;
        persist(std::move(next),profileId(endpointId_,value));return;
      }
      if(field_==Field::Compact){
        const auto count=parsePositive(value);
        persist(endpoints_,selectedProfile(),Json{{"compact_at",count}});return;
      }
      if(field_==Field::Subagents||field_==Field::ConcurrentSubagents){
        const auto count=parsePositive(value);
        const auto key=field_==Field::Subagents?"max_subagents":
          "max_concurrent_subagents";
        persist(endpoints_,selectedProfile(),Json{{key,count}});return;
      }
      if(value.empty()&&!new_){backFromInput();return;}
      switch(field_){
        case Field::Label:
          if(value.empty())throw std::invalid_argument("Enter a label");
          editing_.label=value;
          if(new_){endpointId_=slug(value);int suffix=2;
            while(endpoints_.contains(endpointId_))endpointId_=slug(value)+"-"+std::to_string(suffix++);
            input(Field::BaseUrl,Page::Home);return;}break;
        case Field::BaseUrl:
          if(value.empty())throw std::invalid_argument("Enter an HTTP(S) URL");
          editing_.baseUrl=value;
          if(new_){input(Field::Context,Page::Home);return;}break;
        case Field::MessagesPath:editing_.messagesPath=value;break;
        case Field::ModelsPath:editing_.modelsPath=value=="<clear>"?"":value;break;
        case Field::Token:editing_.authToken=value=="<clear>"?"":value;break;
        case Field::KeyEnv:editing_.apiKeyEnv=value=="<clear>"?"":value;break;
        case Field::Headers:{
          if(value=="<clear>"){editing_.headers=Json::object();break;}
          const auto headers=Json::parse(value);
          if(!headers.is_object())throw std::invalid_argument("Headers must be a JSON object");
          for(auto it=headers.begin();it!=headers.end();++it)
            if(!it.value().is_string())
              throw std::invalid_argument("Header values must be strings");
          editing_.headers=headers;break;
        }
        case Field::Context:
          editing_.contextWindow=parsePositive(value);
          if(new_){input(Field::Output,Page::Home);return;}break;
        case Field::Output:
          editing_.maxOutputTokens=parsePositive(value);
          if(new_){
            auto next=endpoints_;next[endpointId_]=editing_;
            returnPage_=Page::Detail;new_=false;
            persist(std::move(next),selectedProfile());return;
          }break;
        default:break;
      }
      notice_.clear();showEdit();
    }catch(...){
      try{throw;}catch(const std::exception& issue){notice_=issue.what();}
      catch(...){notice_="Invalid value";}
      showInput();
    }
  }
}
