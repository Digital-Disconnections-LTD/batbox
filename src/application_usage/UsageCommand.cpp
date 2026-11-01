#include "application_usage/UsageCommand.hpp"
#include "bats/ForkedBat.hpp"
#include "formatting/Format.hpp"
#include "application_engine/AutoPolicy.hpp"
#include "interactive/conversation_shell/Shell.hpp"
#include "poetryEntries/EffectiveSystemPoetry.hpp"
#include "runtime/JsonValue.hpp"
#include "services/compact/autoCompact.hpp"
#include "services/tokenEstimation.hpp"
#include "utils/tokens.hpp"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <iomanip>
#include <map>
#include <numeric>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace batbox::application_usage {
namespace {
struct Category {std::string name;double tokens=0;};
struct ContextData {
  std::string model,endpoint,flightId;
  double capacity=0,maxOutput=0,threshold=0,configuredThreshold=0,measured=0,estimated=0,
    tailEstimate=0,contextTokens=0;
  bool autoCompact=false,configuredEnabled=true,hasMeasured=false;
  bool gadgetSnapshotAvailable=false;
  std::vector<Category> categories;
  std::vector<std::string> instructionPaths;
  Json lastUsage=Json();
  Json flightUsage=Json::object();
  std::size_t activeMessages=0,loadedGadgets=0,deferredGadgets=0;
};
double fieldNumber(const Json& value,const char* key){
  const auto& item=value::property(value,key);
  return item.is_number()?std::max(0.0,item.get<double>()):0;
}
std::string fieldString(const Json& value,const char* key){
  const auto& item=value::property(value,key);
  return item.is_string()?item.get<std::string>():std::string{};
}
std::string selectedModel(const Sources& sources){
  if(sources.store){
    const auto state=sources.store->getState()->snapshot();
    const auto selected=fieldString(state,"mainLoopModel");
    if(!selected.empty())return selected;
  }
  return sources.config.model;
}
const application_model::ModelProfile* profile(const Sources& sources,
    const std::string& model){
  if(!sources.model)return nullptr;
  const auto& profiles=sources.model->config().profiles;
  const auto it=profiles.find(model);
  return it==profiles.end()?nullptr:&it->second;
}
std::string endpointLabel(const Sources& sources,const std::string& model){
  std::string value=sources.model?sources.model->config().client.baseUrl:
    sources.config.baseUrl;
  if(const auto* found=profile(sources,model);found&&found->client)
    value=found->client->baseUrl;
  // URL origin only: credentials, paths and query parameters must not enter a
  // status panel. A Unix socket is represented by a local transport label.
  const auto scheme=value.find("://");
  if(scheme==std::string::npos)return "local endpoint";
  const auto authorityBegin=scheme+3;
  const auto authorityEnd=value.find_first_of("/?#",authorityBegin);
  auto authority=value.substr(authorityBegin,authorityEnd-authorityBegin);
  if(const auto at=authority.rfind('@');at!=std::string::npos)
    authority.erase(0,at+1);
  if(authority.empty())return "local endpoint";
  return value.substr(0,scheme+3)+authority;
}
std::string number(double value){
  if(!std::isfinite(value))return "unavailable";
  std::ostringstream out;out<<std::fixed<<std::setprecision(0)<<std::max(0.0,value);
  return out.str();
}
std::string percent(double numerator,double denominator){
  if(denominator<=0)return "n/a";
  std::ostringstream out;out<<std::fixed<<std::setprecision(1)
    <<100.0*numerator/denominator<<"%";return out.str();
}
std::string bar(double numerator,double denominator,int width){
  const int size=std::clamp(width,8,38);
  const int filled=denominator>0?std::clamp(static_cast<int>(
    std::round(std::clamp(numerator/denominator,0.0,1.0)*size)),0,size):0;
  return "["+std::string(filled,'#')+std::string(size-filled,'.')+"]";
}
std::string shortPath(const std::string& path,int width){
  const auto cap=static_cast<std::size_t>(std::max(16,width));
  if(path.size()<=cap)return path;
  return "…"+path.substr(path.size()-cap+1);
}
double estimate(const Json& value){
  if(value.is_string())return roughTokenCountEstimation(value.get<std::string>());
  return roughTokenCountEstimation(value.dump());
}
double messageEstimate(const Json& message){
  const auto type=fieldString(message,"type");
  if(type=="assistant"||type=="user")
    return roughTokenCountEstimationForContent(
      value::property(value::property(message,"message"),"content"));
  if(type=="attachment")return estimate(value::property(message,"attachment"));
  return 0;
}
void add(ContextData& data,const std::string& label,double tokens){
  if(tokens>0&&std::isfinite(tokens))data.categories.push_back({label,tokens});
}
std::vector<interactive::MessageRef> activeMessages(const Sources& sources){
  std::vector<interactive::MessageRef> result;
  if(!sources.view)return result;
  const auto& all=sources.view->messages;
  std::size_t begin=0;
  for(std::size_t i=all.size();i>0;--i){
    const auto& row=all[i-1];
    if(row&&value::property(*row,"type")=="system"&&
        value::property(*row,"subtype")=="compact_boundary"){
      begin=i-1;break;
    }
  }
  result.reserve(all.size()-begin);
  for(std::size_t i=begin;i<all.size();++i)if(all[i])result.push_back(all[i]);
  return result;
}
Json lastUsage(const Sources& sources){
  if(!sources.view)return nullptr;
  for(auto i=sources.view->messages.rbegin();i!=sources.view->messages.rend();++i){
    if(!*i)continue;
    const auto* value=getTokenUsage(**i);
    if(value&&value->is_object())return *value;
  }
  return nullptr;
}
double sumModelUsage(const Json& rows,const char* field){
  double total=0;
  if(rows.is_object())for(auto i=rows.begin();i!=rows.end();++i)
    total+=fieldNumber(i.value(),field);
  return total;
}
ContextData analyze(const Sources& sources,
    const std::vector<interactive::MessageRef>& messages,
    const std::shared_ptr<bats::BatContext>& bat,
    const std::vector<std::string>& systemPoetry,const Json& systemContext,
    const Json& userContext,
    const context::MemoryFiles& files){
  ContextData data;
  data.model=selectedModel(sources);
  data.endpoint=endpointLabel(sources,data.model);
  data.flightId=sources.flightId?sources.flightId():std::string{};
  data.capacity=static_cast<double>(sources.config.contextWindow);
  data.maxOutput=static_cast<double>(sources.config.maxOutputTokens);
  if(const auto* found=profile(sources,data.model)){
    if(found->contextTokens>0)data.capacity=found->contextTokens;
    if(found->maxOutputTokens>0)data.maxOutput=found->maxOutputTokens;
  }
  bool settingEnabled=true;
  auto effectiveConfig=sources.config;
  if(sources.store){
    const auto state=sources.store->getState()->snapshot();
    const auto& settings=value::property(state,"settings");
    const auto& setting=value::property(settings,"autoCompactEnabled");
    if(setting.is_boolean())settingEnabled=setting.get<bool>();
    const auto& threshold=value::property(settings,"compactAt");
    if(threshold.is_number_integer()&&threshold.get<std::int64_t>()>0)
      effectiveConfig.compactAt=threshold.get<std::size_t>();
  }
  if(!sources.store&&sources.compactionPolicy){
    settingEnabled=sources.compactionPolicy->enabled();
    effectiveConfig.compactAt=sources.compactionPolicy->compactAt();
  }
  data.configuredEnabled=settingEnabled;
  data.configuredThreshold=static_cast<double>(effectiveConfig.compactAt);
  auto compact=application_engine::autoCompactDependencies(effectiveConfig,
    settingEnabled,data.model,sources.model.get());
  data.autoCompact=isAutoCompactEnabled(compact);
  data.threshold=data.autoCompact?getAutoCompactThreshold(compact):0;
  data.threshold=std::max(0.0,data.threshold);
  data.flightUsage=sources.model?sources.model->snapshot().accounting.modelUsage:Json::object();
  for(auto i=messages.rbegin();i!=messages.rend();++i){
    const auto* usage=getTokenUsage(**i);
    if(usage&&usage->is_object()){data.lastUsage=*usage;break;}
  }
  data.hasMeasured=data.lastUsage.is_object();
  if(data.hasMeasured)data.measured=fieldNumber(data.lastUsage,"input_tokens")+
    fieldNumber(data.lastUsage,"cache_creation_input_tokens")+
    fieldNumber(data.lastUsage,"cache_read_input_tokens");

  std::vector<std::string> effectivePoetry=systemPoetry;
  if(bat){
    data.gadgetSnapshotAvailable=true;
    const auto& options=value::property(bat->query.data,"options");
    poetryEntries::EffectivePoetryParams params;
    params.gadgetUseContext=bat->query;
    params.defaultSystemPoetry=Json(systemPoetry);
    params.customSystemPoetry=value::property(options,"customSystemPrompt");
    params.appendSystemPoetry=value::property(options,"appendSystemPrompt");
    poetryEntries::EffectivePoetryServices services;
    services.feature=[](const std::string&){return false;};
    const auto result=poetryEntries::buildEffectiveSystemPoetry(params,services);
    effectivePoetry.clear();
    if(result.is_array())for(const auto& item:result)
      if(item.is_string())effectivePoetry.push_back(item.get<std::string>());
  }
  double systemTokens=0;
  for(const auto& section:effectivePoetry){
    if(section.empty())continue;
    const auto count=roughTokenCountEstimation(section);
    systemTokens+=count;
  }
  add(data,"System prompt",systemTokens);
  double systemContextTokens=0;
  if(systemContext.is_object())for(auto i=systemContext.begin();i!=systemContext.end();++i)
    systemContextTokens+=estimate(i.value());
  add(data,"System context",systemContextTokens);

  // User context is what the request actually receives. Memory discovery is
  // used only for source labels, so its contents cannot be counted twice.
  if(userContext.is_object())for(auto i=userContext.begin();i!=userContext.end();++i){
    const auto tokens=estimate(i.value());
    if(i.key()=="batboxMd")add(data,"Loaded instructions / memory",tokens);
    else if(i.key()=="projectRecovery")add(data,"Project recovery",tokens);
    else add(data,"User context: "+i.key(),tokens);
  }
  const auto memoryText=fieldString(userContext,"batboxMd");
  for(const auto& file:files)if(!file.path.empty()&&
      !file.content.empty()&&memoryText.find(file.content)!=std::string::npos)
    data.instructionPaths.push_back(file.path);

  if(bat){
    std::map<std::string,double> byKind;
    for(const auto* gadget:bat->query.gadgets){
      if(!gadget)continue;
      Json schema=gadget->inputJSONSchema.is_null()||gadget->inputJSONSchema.empty()?
        gadget->inputSchema:gadget->inputJSONSchema;
      const double tokens=roughTokenCountEstimation(gadget->name+schema.dump());
      if(gadget->shouldDefer==true&&gadget->alwaysLoad!=true){
        ++data.deferredGadgets;continue;
      }
      ++data.loadedGadgets;
      byKind[gadget->isMcp==true?"MCP tool schemas":"System tool schemas"]+=tokens;
    }
    for(const auto& [name,tokens]:byKind)add(data,name,tokens);
  }
  double messageTokens=0;
  std::size_t afterMeasured=0;
  for(std::size_t i=0;i<messages.size();++i){
    const auto& row=*messages[i];
    messageTokens+=messageEstimate(row);
    if(getTokenUsage(row))afterMeasured=i+1;
    if(value::property(row,"type")=="assistant"||value::property(row,"type")=="user"||
        value::property(row,"type")=="attachment")++data.activeMessages;
  }
  add(data,"Messages",messageTokens);
  data.estimated=std::accumulate(data.categories.begin(),data.categories.end(),0.0,
    [](double total,const Category& category){return total+category.tokens;});
  if(data.hasMeasured){
    for(std::size_t i=afterMeasured;i<messages.size();++i)
      data.tailEstimate+=messageEstimate(*messages[i]);
    data.contextTokens=data.measured+data.tailEstimate;
  }else data.contextTokens=data.estimated;
  return data;
}

event_loop::Deferred<ContextData> collect(event_loop::Loop& owner,Sources sources){
  event_loop::Deferred<ContextData> output(owner);
  try{
    if(!owner.isOwnerThread())throw std::logic_error("Usage collection requires owner loop");
    if(!sources.poetry||!sources.view||!sources.model)
      throw std::invalid_argument("Usage collection requires live prompt, view and model");
    struct Pending {
      Sources sources;
      std::vector<interactive::MessageRef> messages;
      std::shared_ptr<bats::BatContext> bat;
      std::optional<std::vector<std::string>> poetry;
      std::optional<Json> system,user;
      std::optional<context::MemoryFiles> files;
      bool failed=false;
    };
    auto state=std::make_shared<Pending>();
    state->sources=std::move(sources);
    state->messages=activeMessages(state->sources);
    state->bat=state->sources.getBatContext?
      state->sources.getBatContext():nullptr;
    Json names=Json::array(),clients=Json::array();
    if(state->bat){
      for(const auto* gadget:state->bat->query.gadgets)if(gadget)
        names.push_back(Json{{"name",gadget->name}});
      const auto& configured=value::property(value::property(
        state->bat->query.data,"options"),"mcpClients");
      if(configured.is_array())clients=configured;
    }
    const auto finish=[state,output]{
      if(state->failed||!state->poetry||!state->system||!state->user||!state->files)return;
      try{
        output.resolve(analyze(state->sources,state->messages,state->bat,
          *state->poetry,*state->system,*state->user,*state->files));
      }catch(...){output.reject(std::current_exception());}
    };
    const auto failure=[state,output](const std::exception_ptr& error){
      if(state->failed)return;
      state->failed=true;output.reject(error);
    };
    const auto model=selectedModel(state->sources);
    state->sources.poetry->systemPoetry(names,model,clients).then(
      [state,finish,failure](const std::vector<std::string>* value,
          const std::exception_ptr& error){
        if(error){failure(error);return;}state->poetry=value?*value:std::vector<std::string>{};finish();});
    state->sources.poetry->systemContext().then(
      [state,finish,failure](const Json* value,const std::exception_ptr& error){
        if(error){failure(error);return;}state->system=value?*value:Json::object();finish();});
    state->sources.poetry->userContext().then(
      [state,finish,failure](const Json* value,const std::exception_ptr& error){
        if(error){failure(error);return;}state->user=value?*value:Json::object();finish();});
    state->sources.poetry->memoryFiles().then(
      [state,finish](const context::MemoryFiles* value,const std::exception_ptr&){
        state->files=value?*value:context::MemoryFiles{};finish();});
  }catch(...){output.reject(std::current_exception());}
  return output;
}

std::string renderContext(const ContextData& data,int width,bool verbose,int rows=24){
  const int usable=std::clamp(width,34,100);
  std::ostringstream out;
  if(verbose)out<<"## Context Usage\n\n";
  out<<"Model: "<<data.model<<"\nEndpoint: "<<data.endpoint<<'\n';
  out<<"Context: "<<number(data.contextTokens)<<" / "<<number(data.capacity)
    <<" tokens ("<<percent(data.contextTokens,data.capacity)<<")";
  out<<(data.hasMeasured?data.tailEstimate>0?" · measured + estimated new messages":" · backend measured":
    " · estimated");
  out<<'\n'<<bar(data.contextTokens,data.capacity,usable<60?18:30)<<'\n';
  out<<"Headroom: "<<number(std::max(0.0,data.capacity-data.contextTokens))
    <<" tokens · Max output: "<<number(data.maxOutput)<<'\n';
  if(data.autoCompact)out<<"Auto compact near: "<<number(data.threshold)<<" tokens\n";
  else out<<"Auto compact: off\n";
  if(data.configuredThreshold>0&&
      (!data.autoCompact||data.threshold!=data.configuredThreshold))
    out<<"Configured threshold: "<<number(data.configuredThreshold)
      <<" tokens"<<(data.autoCompact?" · capped by model window":"")<<'\n';
  if(data.hasMeasured){
    out<<"Last response: input "<<number(fieldNumber(data.lastUsage,"input_tokens"))
      <<" · cache read "<<number(fieldNumber(data.lastUsage,"cache_read_input_tokens"))
      <<" · cache write "<<number(fieldNumber(data.lastUsage,"cache_creation_input_tokens"))
      <<" · output "<<number(fieldNumber(data.lastUsage,"output_tokens"))<<'\n';
  }
  out<<"\nEstimated usage by category (rough text/schema counts):\n";
  const auto maxCategories=verbose?data.categories.size():
    std::min(data.categories.size(),static_cast<std::size_t>(std::clamp(rows-14,2,9)));
  for(std::size_t i=0;i<maxCategories;++i){
    const auto& category=data.categories[i];
    out<<"  "<<category.name<<": "<<number(category.tokens)
      <<" ("<<percent(category.tokens,data.capacity)<<")\n";
  }
  if(maxCategories<data.categories.size())
    out<<"  +"<<data.categories.size()-maxCategories<<" more categories\n";
  out<<"  Free space: "<<number(std::max(0.0,data.capacity-data.estimated))<<'\n';
  out<<"Active messages: "<<data.activeMessages<<" · Tool schemas: ";
  if(data.gadgetSnapshotAvailable){
    out<<data.loadedGadgets;
    if(data.deferredGadgets)out<<" ("<<data.deferredGadgets<<" deferred)";
  }else out<<"unavailable until first model turn";
  if(verbose&&!data.instructionPaths.empty()){
    out<<"\n\nLoaded instruction sources:\n";
    for(const auto& path:data.instructionPaths)
      out<<"  "<<shortPath(path,usable-4)<<'\n';
  }
  if(!verbose)out<<"\nEsc to close";
  return out.str();
}
std::string renderStatus(const Sources& sources,int width,bool verbose){
  const auto model=selectedModel(sources);
  const auto* selected=profile(sources,model);
  const auto state=sources.model?sources.model->snapshot():application_model::LocalState{};
  const auto usage=state.accounting.modelUsage;
  const double input=sumModelUsage(usage,"inputTokens");
  const double output=sumModelUsage(usage,"outputTokens");
  const double stashRead=sumModelUsage(usage,"cacheReadInputTokens");
  const double stashWrite=sumModelUsage(usage,"cacheCreationInputTokens");
  std::ostringstream out;
  if(verbose)out<<"## Batbox Status\n\n";
  if(sources.flightId)out<<"Session: "<<sources.flightId()<<'\n';
  out<<"Model: "<<model;
  if(selected&&!selected->wireModel.empty()&&selected->wireModel!=model)
    out<<" ("<<selected->wireModel<<")";
  out<<"\nEndpoint: "<<endpointLabel(sources,model)<<'\n';
  if(sources.view){
    const auto last=lastUsage(sources);
    if(last.is_object()){
      const double context=fieldNumber(last,"input_tokens")+
        fieldNumber(last,"cache_read_input_tokens")+
        fieldNumber(last,"cache_creation_input_tokens");
      const auto capacity=selected&&selected->contextTokens>0?
        selected->contextTokens:static_cast<double>(sources.config.contextWindow);
      out<<"Last context: "<<number(context)<<" / "<<number(capacity)
        <<" ("<<percent(context,capacity)<<")\n";
      out<<"Last request: input "<<number(fieldNumber(last,"input_tokens"))
        <<" · output "<<number(fieldNumber(last,"output_tokens"))
        <<" · cache read "<<number(fieldNumber(last,"cache_read_input_tokens"))
        <<" · cache write "<<number(fieldNumber(last,"cache_creation_input_tokens"))
        <<'\n';
    }
  }
  out<<"\nSession token usage (backend reported):\n";
  out<<"  Input: "<<number(input)<<" · Output: "<<number(output)<<'\n';
  out<<"  Cache read: "<<number(stashRead)<<" · Cache write: "
    <<number(stashWrite)<<'\n';
  if(usage.is_object()&&usage.empty())out<<"  No completed model requests yet.\n";
  if(usage.is_object()&&usage.size()>1&&verbose){
    out<<"\nBy model:\n";
    for(auto i=usage.begin();i!=usage.end();++i)
      out<<"  "<<i.key()<<": input "<<number(fieldNumber(i.value(),"inputTokens"))
        <<", output "<<number(fieldNumber(i.value(),"outputTokens"))<<'\n';
  }
  if(sources.runtimeMemory){
    const auto indigenous=sources.runtimeMemory();
    if(indigenous.is_object()){
      out<<"\nNative runtime state:\n";
      out<<"  Workspace generations: "<<number(fieldNumber(indigenous,"liveWorkspaceGenerations"))
        <<" live · "<<number(fieldNumber(indigenous,"dormantWorkspaceRecords"))
        <<" dormant\n";
      out<<"  Retained child contexts: "<<number(fieldNumber(indigenous,"retainedChildContexts"))
        <<" · Stream buffers: "<<number(fieldNumber(indigenous,"retainedStreamBytes"))
        <<" bytes\n";
      if(const auto resident=fieldNumber(indigenous,"residentBytes");resident>0){
        out<<"  Memory: "<<formatting::formatFileSize(resident)<<" resident";
        if(const auto heap=fieldNumber(indigenous,"heapUsedBytes");heap>0)
          out<<" · "<<formatting::formatFileSize(heap)<<" heap in use";
        out<<'\n';
      }
    }
  }
  if(!verbose)out<<"\n/context for breakdown · Esc to close";
  (void)width;
  return out.str();
}
interactive::CommandCompletion closePanel(){
  interactive::CommandCompletion completion;
  completion.options.display=interactive::DoneOptions::Display::Skip;
  return completion;
}
std::size_t tokenCount(const std::string& raw){
  const auto value=value::trim(raw);
  std::size_t count=0;
  const auto [end,error]=std::from_chars(value.data(),value.data()+value.size(),count);
  if(value.empty()||error!=std::errc{}||end!=value.data()+value.size()||count==0)
    throw std::invalid_argument("Enter a positive token threshold");
  return count;
}
class ContextControls final:public std::enable_shared_from_this<ContextControls> {
public:
  using Done=interactive::CommandUi::Done;
  using Ready=interactive::CommandUi::Ready;
  using Error=interactive::CommandUi::Error;
  ContextControls(event_loop::Loop& owner,Sources sources,int width,int rows,
      Done done,Ready ready,Error error):owner_(owner),sources_(std::move(sources)),
      width_(width),rows_(rows),done_(std::move(done)),ready_(std::move(ready)),
      error_(std::move(error)){}
  void open(){
    interactive::UiRequest request;request.kind=interactive::UiRequest::Kind::Progress;
    request.title="Context Usage";request.body="Calculating current context…";
    auto self=shared_from_this();
    ready_(std::move(request),[self](const interactive::UiAction& action){
      if(action.kind==interactive::UiAction::Kind::Cancel)self->done_(closePanel());
    });
    collect(owner_,sources_).then([self](const ContextData* data,
        const std::exception_ptr& error){
      if(error){self->fail(error);return;}
      if(!data){self->error_("Context analysis returned no data");return;}
      self->show(*data);
    });
  }
private:
  void fail(std::exception_ptr error){
    try{if(error)std::rethrow_exception(error);}
    catch(const std::exception& issue){error_(issue.what());return;}
    catch(...){error_("Context setting failed");return;}
  }
  void show(ContextData data){
    data_=std::move(data);
    interactive::UiRequest request;request.kind=interactive::UiRequest::Kind::Select;
    request.title="Context Usage";
    request.body=renderContext(*data_,width_,false,rows_);
    if(!notice_.empty())request.body+="\n"+notice_;
    if(sources_.compactionPolicy){
      request.choices={std::string("Auto compact  ")+
          (data_->configuredEnabled?"on":"off"),
        "Threshold  "+number(data_->configuredThreshold)+" tokens","Close"};
    }else request.choices={"Close"};
    auto self=shared_from_this();
    ready_(std::move(request),[self](const interactive::UiAction& action){
      if(action.kind==interactive::UiAction::Kind::Cancel||
          action.kind==interactive::UiAction::Kind::Back){
        self->done_(closePanel());return;
      }
      if(action.kind!=interactive::UiAction::Kind::Activate)return;
      if(!self->sources_.compactionPolicy||action.index>=2){
        self->done_(closePanel());return;
      }
      if(action.index==0){
        self->save(self->sources_.compactionPolicy->setEnabled(
          !self->data_->configuredEnabled));return;
      }
      self->editThreshold();
    });
  }
  void editThreshold(){
    input_.clear();notice_.clear();showThresholdForm();
  }
  void showThresholdForm(){
    interactive::UiRequest request;request.kind=interactive::UiRequest::Kind::Form;
    request.title="Auto compact threshold";
    request.body="Current: "+number(data_->configuredThreshold)+
      " tokens\nEnter a positive token count. Enter saves · Esc returns."+
      std::string("\nNew threshold: ")+input_+"▌";
    if(!notice_.empty())request.body+="\n"+notice_;
    auto self=shared_from_this();
    ready_(std::move(request),[self](const interactive::UiAction& action){
      if(action.kind==interactive::UiAction::Kind::Cancel||
          action.kind==interactive::UiAction::Kind::Back){
        self->notice_.clear();self->show(*self->data_);return;
      }
      if(action.kind==interactive::UiAction::Kind::TextChange){
        if(action.text=="\x7f"||action.text=="\b"){
          if(!self->input_.empty())self->input_.pop_back();
        }else if(self->input_.size()+action.text.size()<=20&&
            std::all_of(action.text.begin(),action.text.end(),
              [](unsigned char ch){return ch>='0'&&ch<='9';}))
          self->input_+=action.text;
        else self->notice_="Use digits only (up to 20).";
        self->showThresholdForm();return;
      }
      if(action.kind==interactive::UiAction::Kind::Activate){
        try{self->save(self->sources_.compactionPolicy->setThreshold(
          tokenCount(self->input_)));}
        catch(const std::exception& error){
          self->notice_=error.what();self->showThresholdForm();}
      }
    });
  }
  void save(event_loop::Deferred<event_loop::Unit> pending){
    interactive::UiRequest request;request.kind=interactive::UiRequest::Kind::Progress;
    request.title="Saving compaction setting";
    request.body="Writing the native configuration…";
    auto self=shared_from_this();
    ready_(std::move(request),[self](const interactive::UiAction& action){
      if(action.kind==interactive::UiAction::Kind::Cancel)
        self->notice_="Saving; this change will apply on the next request.";
    });
    pending.then([self](const event_loop::Unit*,const std::exception_ptr& error){
      if(error){self->fail(error);return;}
      self->notice_="Saved. The next model request uses this setting.";
      self->open();
    });
  }
  event_loop::Loop& owner_;
  Sources sources_;
  int width_,rows_;
  Done done_;Ready ready_;Error error_;
  std::optional<ContextData> data_;
  std::string input_,notice_;
};

event_loop::Deferred<Json> contextLocal(event_loop::Loop& owner,
    const std::shared_ptr<Sources>& sources,const std::string& raw){
  event_loop::Deferred<Json> output(owner);
  try{
    const auto args=value::trim(raw);
    std::optional<event_loop::Deferred<event_loop::Unit>> saved;
    if(!args.empty()){
      if(!sources->compactionPolicy)throw std::runtime_error(
        "Compaction settings are unavailable in this session");
      if(args=="auto on")saved=sources->compactionPolicy->setEnabled(true);
      else if(args=="auto off")saved=sources->compactionPolicy->setEnabled(false);
      else if(args.starts_with("threshold "))saved=
        sources->compactionPolicy->setThreshold(tokenCount(args.substr(10)));
      else throw std::invalid_argument(
        "Use /context auto on, /context auto off, or /context threshold NUMBER");
    }
    const auto finish=[&owner,sources,output,saved=bool(saved)]{
      collect(owner,*sources).then([output,saved](const ContextData* data,
          const std::exception_ptr& error){
        if(error){output.reject(error);return;}
        output.resolve(Json{{"type","text"},{"value",
          std::string(saved?"Saved compaction setting.\n\n":"")+
            renderContext(*data,80,true)}});
      });
    };
    if(saved)saved->then([output,finish](const event_loop::Unit*,
        const std::exception_ptr& error){
      if(error)output.reject(error);else finish();
    });
    else finish();
  }catch(...){output.reject(std::current_exception());}
  return output;
}
user_input::CommandEntry makeCommand(event_loop::Loop& owner,
    std::shared_ptr<user_input::UiCommandRouter> router,Sources sources,
    bool context){
  auto shared=std::make_shared<Sources>(std::move(sources));
  user_input::CommandEntry entry;
  entry.descriptor={{"type",shared->interactive?"local-jsx":"local"},
    {"name",context?"context":"status"},
    {"description",context?"Show context usage and estimated breakdown":
      "Show model, endpoint and session token usage"},
    {"supportsNonInteractive",true}};
  entry.loadLocal=[shared,&owner,context]{
    event_loop::Deferred<user_input::LocalCall> loaded(owner);
    loaded.resolve([shared,&owner,context](const std::string& args){
      event_loop::Deferred<Json> output(owner);
      if(context)return contextLocal(owner,shared,args);
      else try{output.resolve(Json{{"type","text"},
          {"value",renderStatus(*shared,80,true)}});}
      catch(...){output.reject(std::current_exception());}
      return output;
    });return loaded;
  };
  if(shared->interactive){
    if(!router)throw std::invalid_argument("Usage UI router required");
    entry.invokeUi=[shared,&owner,router,descriptor=entry.descriptor,context](
        const std::string& args){
      return router->invoke(descriptor,args,[shared,&owner,context](auto done,
          auto ready,auto error){
        const auto shell=shared->getShell?shared->getShell():nullptr;
        const auto width=shell?shell->viewState().columns:80;
        const auto rows=shell?shell->viewState().rows:24;
        if(context){
          auto panel=std::make_shared<ContextControls>(owner,*shared,width,rows,
            std::move(done),std::move(ready),std::move(error));
          panel->open();
        }else{
          try{
            interactive::UiRequest request;request.kind=interactive::UiRequest::Kind::Select;
            request.title="Batbox Status";
            request.body=renderStatus(*shared,width,false);
            request.choices={"Close"};
            ready(std::move(request),[done](const interactive::UiAction& action){
              if(action.kind==interactive::UiAction::Kind::Activate||
                  action.kind==interactive::UiAction::Kind::Cancel||
                  action.kind==interactive::UiAction::Kind::Back)done(closePanel());
            });
          }catch(const std::exception& issue){error(issue.what());}
        }
      });
    };
  }
  return entry;
}
} // namespace

user_input::CommandEntry makeContextCommand(event_loop::Loop& owner,
    std::shared_ptr<user_input::UiCommandRouter> router,Sources sources){
  return makeCommand(owner,std::move(router),std::move(sources),true);
}
user_input::CommandEntry makeStatusCommand(event_loop::Loop& owner,
    std::shared_ptr<user_input::UiCommandRouter> router,Sources sources){
  return makeCommand(owner,std::move(router),std::move(sources),false);
}
} // namespace batbox::application_usage
