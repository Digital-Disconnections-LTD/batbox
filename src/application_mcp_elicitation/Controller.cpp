#include "application_mcp_elicitation/Controller.hpp"
#include "application_mcp_elicitation/Validation.hpp"
#include <algorithm>
#include <optional>
#include <stdexcept>

namespace batbox::application_mcp_elicitation {
namespace {
std::string text(const Json& value,const char* key){
  const auto& field=value::property(value,key);
  return field.is_string()?field.get<std::string>():std::string{};
}
Json cancel(){return Json{{"action","cancel"}};}
bool responseAction(const Json& value){
  const auto action=text(value,"action");
  return action=="accept"||action=="decline"||action=="cancel";
}
std::string display(std::string value,std::size_t maximum=2048){
  for(auto& ch:value)if(static_cast<unsigned char>(ch)<32&&ch!='\n'&&ch!='\t')
    ch=' ';
  if(value.size()>maximum)value.resize(maximum);
  return value;
}
}

struct Controller::Pending {
  struct Field {std::string name;Json schema;bool required=false;};
  Pending(event_loop::Loop& owner,std::string name,Json request,
      std::stop_token stop,Hooks callbacks):server(std::move(name)),
      rpc(std::move(request)),params(value::property(rpc,"params")),
      output(owner),signal(stop),hooks(std::move(callbacks)){}
  std::string server,mode,url,elicitationId,problem,input;
  Json rpc,params,values=Json::object();
  event_loop::Deferred<Json> output;
  std::stop_token signal;
  Hooks hooks;
  std::unique_ptr<std::stop_callback<std::function<void()>>> abort;
  std::vector<Field> fields;
  std::size_t fieldIndex=0;
  bool queued=false,responding=false,finished=false,waiting=false,
    completed=false,validating=false;
};

std::shared_ptr<Controller> Controller::create(Inputs inputs){
  if(!inputs.owner.isOwnerThread())throw std::logic_error(
    "MCP elicitation controller requires owner loop");
  if(!inputs.diagnostic)inputs.diagnostic=[](const std::string&){};
  return std::shared_ptr<Controller>(new Controller(std::move(inputs)));
}
void Controller::attachUi(std::shared_ptr<interactive::CommandUi> ui){
  if(!inputs_.owner.isOwnerThread())throw std::logic_error(
    "MCP elicitation UI requires owner loop");
  if(ui_&&showing_)ui_->setGadgetOverlay(std::nullopt);
  ui_=std::move(ui);showing_=false;showNext();
}
event_loop::Deferred<Json> Controller::request(const std::string& server,
    const Json& rpcRequest,std::stop_token signal,Hooks hooks){
  if(!inputs_.owner.isOwnerThread())throw std::logic_error(
    "MCP elicitation requires owner loop");
  auto pending=std::make_shared<Pending>(inputs_.owner,server,rpcRequest,
    signal,std::move(hooks));
  const auto result=pending->output;
  if(value::property(rpcRequest,"method")!="elicitation/create"||
      !pending->params.is_object()){
    result.resolve(cancel());return result;
  }
  pending->mode=text(pending->params,"mode")=="url"?"url":"form";
  pending->url=text(pending->params,"url");
  pending->elicitationId=text(pending->params,"elicitationId");
  if(pending->mode=="form"){
    const auto& schema=value::property(pending->params,"requestedSchema");
    const auto& properties=value::property(schema,"properties");
    if(!properties.is_object()||properties.size()>64){
      inputs_.diagnostic("MCP "+server+" sent an unsupported elicitation form");
      result.resolve(cancel());return result;
    }
    const auto& required=value::property(schema,"required");
    for(auto it=properties.begin();it!=properties.end();++it){
      if(!it.value().is_object()){
        inputs_.diagnostic("MCP "+server+" sent an invalid elicitation field");
        result.resolve(cancel());return result;
      }
      const auto type=text(it.value(),"type");
      if(type=="array"&&optionsFor(it.value()).empty()){
        inputs_.diagnostic("MCP "+server+" sent an array field without options");
        result.resolve(cancel());return result;
      }
      bool mandatory=false;
      if(required.is_array())for(const auto& name:required)
        if(name==it.key()){mandatory=true;break;}
      pending->fields.push_back({it.key(),it.value(),mandatory});
      const auto& initial=value::property(it.value(),"default");
      if(!initial.is_null())pending->values[it.key()]=initial;
    }
    if(!pending->fields.empty()){
      const auto& first=pending->fields.front();
      const auto& initial=value::property(pending->values,first.name);
      if(!initial.is_null()&&text(first.schema,"type")!="array"&&
          text(first.schema,"type")!="boolean")
        pending->input=initial.is_string()?initial.get<std::string>():initial.dump();
    }
  }else if(pending->url.empty()){
    inputs_.diagnostic("MCP "+server+" sent a URL elicitation without a URL");
    result.resolve(cancel());return result;
  }
  all_.push_back(pending);
  const auto owner=inputs_.owner.dispatcher();
  pending->abort=std::make_unique<std::stop_callback<std::function<void()>>>(
    signal,[weak=weak_from_this(),owner,held=std::weak_ptr<Pending>(pending)]{
      owner.postMission([weak,held]{if(auto self=weak.lock())
        if(auto value=held.lock())self->cancelOne(value);});
    });
  if(signal.stop_requested()){cancelOne(pending);return result;}
  if(pending->hooks.before){
    try{pending->hooks.before(server,pending->params,signal).then(
      [weak=weak_from_this(),pending](const Json* hook,
          const std::exception_ptr& error){
        auto self=weak.lock();if(!self||pending->responding)return;
        if(error)self->inputs_.diagnostic("MCP elicitation hook failed; showing the request");
        if(!error&&hook&&responseAction(*hook))self->finish(pending,*hook);
        else self->enqueue(pending);
      });}
    catch(...){inputs_.diagnostic("MCP elicitation hook failed; showing the request");
      enqueue(pending);}
  }else enqueue(pending);
  return result;
}
void Controller::enqueue(const std::shared_ptr<Pending>& pending){
  if(pending->responding||pending->queued)return;
  if(!inputs_.interactive){
    inputs_.diagnostic("MCP "+pending->server+
      " requested input in a noninteractive session; canceled");
    resolve(pending,cancel());return;
  }
  pending->queued=true;queue_.push_back(pending);showNext();
}
void Controller::showNext(){
  if(showing_||queue_.empty()||!ui_)return;
  if(ui_->hasActionableUi()){
    auto weak=weak_from_this();
    retry_=inputs_.owner.setTimeout(50,[weak]{if(auto self=weak.lock())
      self->showNext();},false);
    return;
  }
  showing_=true;show(queue_.front());
}
void Controller::show(const std::shared_ptr<Pending>& pending){
  if(!ui_||queue_.empty()||queue_.front()!=pending)return;
  if(pending->validating){
    interactive::UiRequest request;request.kind=interactive::UiRequest::Kind::Select;
    request.id="mcp-elicitation-validating";
    request.title="Checking date input";
    request.body="Interpreting the date with the configured model…";
    request.choices={"Cancel"};
    auto weak=weak_from_this();
    ui_->setGadgetOverlay(std::move(request),false,[weak,pending](const auto& action){
      if(auto self=weak.lock())self->decide(pending,action);});
    return;
  }
  if(pending->waiting){showWaiting(pending);return;}
  if(pending->mode=="url"){showUrl(pending);return;}
  if(pending->fieldIndex>=pending->fields.size()){showSummary(pending);return;}
  const auto& schema=pending->fields[pending->fieldIndex].schema;
  if(text(schema,"type")=="boolean"||text(schema,"type")=="array"||
      !optionsFor(schema).empty())showOptions(pending);
  else showForm(pending);
}
void Controller::showForm(const std::shared_ptr<Pending>& pending){
  const auto& field=pending->fields[pending->fieldIndex];
  interactive::UiRequest request;request.kind=interactive::UiRequest::Kind::Form;
  request.id="mcp-elicitation-field";request.title="MCP "+pending->server+
    " · "+field.name;
  request.body=display(text(pending->params,"message"))+"\n\n"+
    display(text(field.schema,"description"),500)+"\n"+
    (field.required?"Required":"Optional")+" · "+text(field.schema,"type")+
    "\nValue: "+pending->input+"▌\nEnter next · Esc cancel · Left previous";
  if(!pending->problem.empty())request.body+="\n"+pending->problem;
  auto weak=weak_from_this();
  ui_->setGadgetOverlay(std::move(request),false,[weak,pending](const auto& action){
    if(auto self=weak.lock())self->decide(pending,action);});
}
void Controller::showOptions(const std::shared_ptr<Pending>& pending){
  const auto& field=pending->fields[pending->fieldIndex];
  const auto type=text(field.schema,"type");
  const auto options=optionsFor(field.schema);
  interactive::UiRequest request;request.kind=interactive::UiRequest::Kind::Select;
  request.id="mcp-elicitation-options";request.title="MCP "+pending->server+
    " · "+field.name;
  request.body=display(text(pending->params,"message"))+"\n"+
    display(text(field.schema,"description"),500);
  if(type=="boolean")request.choices={"Yes","No"};
  else for(const auto& option:options){
    auto label=option.label;
    if(type=="array"){
      const auto& selected=value::property(pending->values,field.name);
      const bool checked=selected.is_array()&&
        std::find(selected.begin(),selected.end(),option.value)!=selected.end();
      label=(checked?"[x] ":"[ ] ")+label;
    }
    request.choices.push_back(std::move(label));
  }
  if(type=="array")request.choices.push_back("Continue");
  else if(!field.required)request.choices.push_back("Skip");
  if(!pending->problem.empty())request.body+="\n"+pending->problem;
  auto weak=weak_from_this();
  ui_->setGadgetOverlay(std::move(request),false,[weak,pending](const auto& action){
    if(auto self=weak.lock())self->decide(pending,action);});
}
void Controller::showSummary(const std::shared_ptr<Pending>& pending){
  interactive::UiRequest request;request.kind=interactive::UiRequest::Kind::Select;
  request.id="mcp-elicitation-confirm";
  request.title="MCP "+pending->server+" requests your input";
  request.body=display(text(pending->params,"message"))+"\n\n"+
    std::to_string(pending->values.size())+" field values ready.";
  request.choices={"Accept","Edit fields","Decline","Cancel"};
  if(!pending->problem.empty())request.body+="\n"+pending->problem;
  auto weak=weak_from_this();
  ui_->setGadgetOverlay(std::move(request),false,[weak,pending](const auto& action){
    if(auto self=weak.lock())self->decide(pending,action);});
}
void Controller::showUrl(const std::shared_ptr<Pending>& pending){
  interactive::UiRequest request;request.kind=interactive::UiRequest::Kind::Select;
  request.id="mcp-elicitation-url";
  request.title="MCP "+pending->server+" wants to open a URL";
  request.body=display(text(pending->params,"message"))+"\n\n"+
    display(pending->url,1024);
  request.choices={"Open URL","Decline","Cancel"};
  auto weak=weak_from_this();
  ui_->setGadgetOverlay(std::move(request),false,[weak,pending](const auto& action){
    if(auto self=weak.lock())self->decide(pending,action);});
}
void Controller::showWaiting(const std::shared_ptr<Pending>& pending){
  interactive::UiRequest request;request.kind=interactive::UiRequest::Kind::Select;
  request.id="mcp-elicitation-waiting";
  request.title="MCP "+pending->server+" · waiting for completion";
  request.body=display(pending->url,1024)+"\nThe server will confirm when finished.";
  request.choices={"Open URL again","Continue without waiting","Cancel waiting"};
  auto weak=weak_from_this();
  ui_->setGadgetOverlay(std::move(request),false,[weak,pending](const auto& action){
    if(auto self=weak.lock())self->decide(pending,action);});
}
void Controller::decide(const std::shared_ptr<Pending>& pending,
    const interactive::UiAction& action){
  if(queue_.empty()||queue_.front()!=pending)return;
  if(action.kind==interactive::UiAction::Kind::Cancel){cancelOne(pending);return;}
  if(pending->validating){
    if(action.kind==interactive::UiAction::Kind::Activate)cancelOne(pending);
    return;
  }
  if(action.kind==interactive::UiAction::Kind::Back){
    if(pending->mode=="form"&&pending->fieldIndex>0&&!pending->waiting){
      --pending->fieldIndex;pending->input.clear();pending->problem.clear();
      const auto& previous=pending->fields[pending->fieldIndex];
      const auto& value=value::property(pending->values,previous.name);
      if(!value.is_null()&&text(previous.schema,"type")!="array"&&
          text(previous.schema,"type")!="boolean")
        pending->input=value.is_string()?value.get<std::string>():value.dump();
      show(pending);
    }else cancelOne(pending);
    return;
  }
  if(pending->waiting){
    if(action.kind!=interactive::UiAction::Kind::Activate)return;
    if(action.index==0){if(inputs_.openUrl)inputs_.openUrl(pending->url);return;}
    dismiss(pending);return;
  }
  if(pending->mode=="url"){
    if(action.kind!=interactive::UiAction::Kind::Activate)return;
    if(action.index==0){
      if(inputs_.openUrl)inputs_.openUrl(pending->url);
      resolve(pending,Json{{"action","accept"}});
    }else if(action.index==1)resolve(pending,Json{{"action","decline"}});
    else cancelOne(pending);
    return;
  }
  if(pending->fieldIndex>=pending->fields.size()){
    if(action.kind!=interactive::UiAction::Kind::Activate)return;
    if(action.index==0)resolve(pending,Json{{"action","accept"},
      {"content",pending->values}});
    else if(action.index==1){pending->fieldIndex=0;pending->input.clear();show(pending);}
    else if(action.index==2)resolve(pending,Json{{"action","decline"}});
    else cancelOne(pending);
    return;
  }
  const auto& field=pending->fields[pending->fieldIndex];
  const auto type=text(field.schema,"type");
  const auto options=optionsFor(field.schema);
  if(type=="boolean"||type=="array"||!options.empty()){
    if(action.kind!=interactive::UiAction::Kind::Activate)return;
    if(type=="array"){
      if(action.index<options.size()){
        Json values=value::property(pending->values,field.name);
        if(!values.is_array())values=Json::array();
        const auto found=std::find(values.begin(),values.end(),options[action.index].value);
        if(found==values.end())values.push_back(options[action.index].value);
        else values.erase(found);
        pending->values[field.name]=std::move(values);show(pending);return;
      }
      const auto& selected=value::property(pending->values,field.name);
      if(field.required&&(!selected.is_array()||selected.empty())){
        pending->problem="Select at least one option.";show(pending);return;
      }
    }else if(type=="boolean"&&action.index<2)
      pending->values[field.name]=action.index==0;
    else if(action.index<options.size())
      pending->values[field.name]=options[action.index].value;
    else pending->values.erase(field.name);
  }else{
    if(action.kind==interactive::UiAction::Kind::TextChange){
      if(action.text=="\x7f"||action.text=="\b"){
        if(!pending->input.empty())pending->input.pop_back();
      }else if(pending->input.size()+action.text.size()<=4096)
        pending->input+=action.text;
      pending->problem.clear();show(pending);return;
    }
    if(action.kind!=interactive::UiAction::Kind::Activate)return;
    std::string problem;
    auto value=validateInput(pending->input,field.schema,field.required,problem);
    if(!value){
      const auto format=text(field.schema,"format");
      if(inputs_.parseNaturalDateTime&&text(field.schema,"type")=="string"&&
          (format=="date"||format=="date-time")&&
          !looksLikeISO8601(pending->input)){
        pending->validating=true;show(pending);
        try{inputs_.parseNaturalDateTime(pending->input,format,pending->signal).then(
          [weak=weak_from_this(),pending,problem](
              const DateTimeParseResult* parsed,
              const std::exception_ptr& failure){
            auto self=weak.lock();
            if(!self||pending->finished||!pending->validating)return;
            pending->validating=false;
            std::string validationProblem;
            const auto& current=pending->fields[pending->fieldIndex];
            auto converted=!failure&&parsed&&parsed->success?
              validateInput(parsed->value,current.schema,current.required,
                validationProblem):std::optional<Json>{};
            if(converted){self->advanceField(pending,std::move(*converted));return;}
            pending->problem=problem;
            self->show(pending);
          });}
        catch(...){pending->validating=false;pending->problem=problem;show(pending);}
        return;
      }
      pending->problem=problem;show(pending);return;
    }
    advanceField(pending,std::move(*value));return;
  }
  advanceField(pending,value::property(pending->values,field.name));
}
void Controller::advanceField(const std::shared_ptr<Pending>& pending,Json value){
  const auto& field=pending->fields[pending->fieldIndex];
  if(value.is_null())pending->values.erase(field.name);
  else pending->values[field.name]=std::move(value);
  ++pending->fieldIndex;pending->input.clear();pending->problem.clear();
  if(pending->fieldIndex<pending->fields.size()){
    const auto& next=pending->fields[pending->fieldIndex];
    const auto& value=value::property(pending->values,next.name);
    if(!value.is_null()&&text(next.schema,"type")!="array"&&
        text(next.schema,"type")!="boolean")
      pending->input=value.is_string()?value.get<std::string>():value.dump();
  }
  show(pending);
}
void Controller::resolve(const std::shared_ptr<Pending>& pending,Json raw){
  if(pending->responding)return;
  pending->responding=true;
  if(!responseAction(raw))raw=cancel();
  if(pending->hooks.after&&!pending->signal.stop_requested()){
    try{pending->hooks.after(pending->server,pending->params,raw,
        pending->signal).then([weak=weak_from_this(),pending,raw](
          const Json* value,const std::exception_ptr& error){
        if(auto self=weak.lock()){
          if(error)self->inputs_.diagnostic("MCP elicitation result hook failed");
          self->finish(pending,!error&&value&&responseAction(*value)?*value:raw);
        }
      });return;}
    catch(...){inputs_.diagnostic("MCP elicitation result hook failed");}
  }
  finish(pending,std::move(raw));
}
void Controller::finish(const std::shared_ptr<Pending>& pending,Json result){
  if(pending->finished)return;
  pending->finished=true;
  std::erase_if(all_,[&](const std::weak_ptr<Pending>& item){
    auto value=item.lock();return !value||value==pending;
  });
  pending->output.resolve(result);
  if(pending->mode=="url"&&!pending->elicitationId.empty()&&
      text(result,"action")=="accept"&&
      pending->queued&&!pending->completed&&!pending->signal.stop_requested()){
    pending->waiting=true;showWaiting(pending);return;
  }
  dismiss(pending);
}
void Controller::dismiss(const std::shared_ptr<Pending>& pending){
  const auto found=std::find(queue_.begin(),queue_.end(),pending);
  if(found==queue_.end())return;
  const bool front=found==queue_.begin();
  queue_.erase(found);pending->queued=false;
  if(front){
    showing_=false;
    if(ui_)ui_->setGadgetOverlay(std::nullopt);
    showNext();
  }
}
void Controller::cancelOne(const std::shared_ptr<Pending>& pending){
  if(pending->waiting){dismiss(pending);return;}
  if(!pending->finished){
    if(pending->signal.stop_requested()){
      pending->responding=true;pending->finished=true;
      pending->output.resolve(cancel());dismiss(pending);
    }else resolve(pending,cancel());
  }
}
void Controller::complete(const std::string& server,
    const std::string& elicitationId){
  if(!inputs_.owner.isOwnerThread())throw std::logic_error(
    "MCP elicitation completion requires owner loop");
  for(const auto& pending:queue_)
    if(pending->server==server&&pending->elicitationId==elicitationId&&
        !elicitationId.empty()){
      pending->completed=true;
      if(pending->waiting)dismiss(pending);
      return;
    }
}
void Controller::cancelAll(){
  if(!inputs_.owner.isOwnerThread()){
    auto weak=weak_from_this();
    inputs_.owner.postMission([weak]{if(auto self=weak.lock())self->cancelAll();});
    return;
  }
  retry_.cancel();retry_={};
  for(const auto& weak:all_)if(auto pending=weak.lock()){
    if(!pending->finished){pending->responding=true;pending->finished=true;
      pending->output.resolve(cancel());}
  }
  all_.clear();queue_.clear();showing_=false;
  if(ui_)ui_->setGadgetOverlay(std::nullopt);
}
} // namespace batbox::application_mcp_elicitation
