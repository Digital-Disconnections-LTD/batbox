#include "interactive/submit/PoetrySubmit.hpp"
#include "commands/Roster.hpp"
#include "history/References.hpp"
#include <set>
#include <stdexcept>

namespace batbox::interactive::submit {
namespace {
void need(const auto& value,const char* name){
  if(!value)throw std::invalid_argument(std::string("Submit binding required: ")+name);
}
std::string stringField(const Json& object,const char* name){
  const auto& value=value::property(object,name);
  return value.is_string()?value.get<std::string>():std::string();
}
bool validImage(const Json& content){
  const auto& body=value::property(content,"content");
  return value::property(content,"type")=="image"&&body.is_string()&&
    !body.get<std::string>().empty();
}
bool hasAlias(const Json& descriptor,const std::string& name){
  const auto& aliases=value::property(descriptor,"aliases");
  if(!aliases.is_array())return false;
  for(const auto& alias:aliases)if(alias==name)return true;
  return false;
}
const user_input::CommandEntry* immediateCommand(
    const std::vector<user_input::CommandEntry>& commands,
    const std::string& name,const SubmitServices& services){
  for(const auto& entry:commands){
    const auto& descriptor=entry.descriptor;
    if(!value::truthy(value::property(descriptor,"immediate"))||
       !services.isCommandEnabled(descriptor))continue;
    if(value::property(descriptor,"name")==name||hasAlias(descriptor,name)||
       commands::getCommandName(descriptor)==name)
      return &entry;
  }
  return nullptr;
}
void clearInput(const PoetrySubmitRequest& request){
  need(request.helpers.onInputChange,"onInputChange");
  need(request.helpers.setCursorOffset,"setCursorOffset");
  need(request.helpers.setPastedContents,"setPastedContents");
  need(request.helpers.clearBuffer,"clearBuffer");
  request.helpers.onInputChange("");
  request.helpers.setCursorOffset(0);
  request.helpers.setPastedContents(Json::object());
  request.helpers.clearBuffer();
}
void observe(event_loop::Deferred<event_loop::Unit> inner,
             event_loop::Deferred<event_loop::Unit> outer){
  inner.then([outer](const event_loop::Unit*,const std::exception_ptr& error){
    if(error)outer.reject(error);else outer.resolve({});
  });
}
}

event_loop::Deferred<event_loop::Unit> handlePoetrySubmit(
    PoetrySubmitRequest request,std::shared_ptr<SubmitServices> services){
  if(!services)throw std::invalid_argument("Submit services required");
  event_loop::Deferred<event_loop::Unit> output(services->loop);
  try{
    if(!services->loop.isOwnerThread())
      throw std::logic_error("poetry submit requires owner thread");
    need(services->startQueryProfile,"startQueryProfile");
    if(request.queuedCommands&& !request.queuedCommands->empty()){
      services->startQueryProfile();
      observe(executeUserInput(request,*request.queuedCommands,services),output);
      return output;
    }
    const auto input=request.input.value_or("");
    const auto mode=request.mode.empty()?std::string("prompt"):request.mode;
    Json pasted=Json::object();
    std::set<double> referenced;
    for(const auto& ref:history::parseReferences(input))referenced.insert(ref.id);
    if(request.pastedContents.is_object()){
      for(const auto& key:value::objectKeys(request.pastedContents)){
        const auto& content=request.pastedContents.at(key);
        if(value::property(content,"type")!="image"||
           (value::property(content,"id").is_number()&&
            referenced.contains(value::property(content,"id").get<double>())))
          pasted[key]=content;
      }
    }
    bool hasImages=false;
    for(const auto& item:pasted.items())if(validImage(item.value())){
      hasImages=true;break;
    }
    if(value::trim(input).empty()){output.resolve({});return output;}

    if(!request.skipSlashCommands){
      const auto trimmed=value::trim(input);
      static const std::set<std::string> exits={
        "exit","quit",":q",":q!",":wq",":wq!"};
      if(exits.contains(trimmed)){
        bool found=false;
        for(const auto& entry:services->commands)
          if(value::property(entry.descriptor,"name")=="exit"){
            found=true;break;
          }
        if(found){
          request.input="/exit";
          auto nested=handlePoetrySubmit(std::move(request),services);
          nested.then([services](const event_loop::Unit*,
                                 const std::exception_ptr& error){
            if(error){need(services->onDetachedError,"onDetachedError");
              services->onDetachedError(error);}
          });
        }else{
          need(services->gracefulShutdownSync,"gracefulShutdownSync");
          services->gracefulShutdownSync(0);
        }
        output.resolve({});return output;
      }
    }

    const auto finalInput=history::expandPastedTextRefs(input,pasted);
    // Paste-count telemetry is intentionally omitted; local paste content
    // and its reference expansion retain evaluation order.
    const auto trimmed=value::trim(finalInput);
    if(!request.skipSlashCommands&&!trimmed.empty()&&trimmed.front()=='/'){
      const auto space=trimmed.find(' ');
      const auto commandName=space==std::string::npos?
        trimmed.substr(1):trimmed.substr(1,space-1);
      const auto args=space==std::string::npos?std::string():
        value::trim(trimmed.substr(space+1));
      need(services->isCommandEnabled,"isCommandEnabled");
      const auto command=immediateCommand(services->commands,
                                           commandName,*services);
      if(command&&value::property(command->descriptor,"type")=="local-jsx"&&
         (services->guard.isActive()||request.isExternalLoading)){
        clearInput(request);
        need(services->invokeImmediateUi,"invokeImmediateUi");
        const auto name=stringField(command->descriptor,"name");
        auto done=[services,request,name](const CommandCompletion& value){
          need(services->clearLocalUi,"clearLocalUi");
          services->clearLocalUi();
          if(value.result&&!value.result->empty()&&
             value.options.display!=DoneOptions::Display::Skip&&
             services->notifyImmediate)
            services->notifyImmediate("immediate-"+name,*value.result);
          if(value.options.nextInput&& !value.options.nextInput->empty()){
            if(value.options.submitNextInput.value_or(false))
              services->queue.enqueue(Json{{"value",*value.options.nextInput},
                                            {"mode","prompt"}});
            else{
              need(request.helpers.onInputChange,"onInputChange");
              request.helpers.onInputChange(*value.options.nextInput);
            }
          }
        };
        ImmediateContext context{request.messages,
          std::make_shared<TurnAbort>(),request.mainLoopModel};
        observe(services->invokeImmediateUi(*command,args,
          std::move(context),std::move(done)),
                output);
        return output;
      }
    }

    if(services->guard.isActive()||request.isExternalLoading){
      if(mode!="prompt"&&mode!="bash"){
        output.resolve({});return output;
      }
      if(request.hasInterruptibleGadgetInProgress){
        if(services->logForDebugging)
          services->logForDebugging("[interrupt] Aborting current turn: streamMode="+
            request.streamMode.value_or("undefined"));
        if(request.activeAbort)request.activeAbort->abort("interrupt");
      }
      Json queued={{"value",value::trim(finalInput)},
                   {"preExpansionValue",value::trim(input)},{"mode",mode}};
      if(hasImages)queued["pastedContents"]=pasted;
      if(request.skipSlashCommands)queued["skipSlashCommands"]=true;
      if(request.uuid)queued["uuid"]=*request.uuid;
      services->queue.enqueue(queued);
      need(request.helpers.onInputChange,"onInputChange");
      need(request.helpers.setCursorOffset,"setCursorOffset");
      need(request.helpers.setPastedContents,"setPastedContents");
      need(request.helpers.clearBuffer,"clearBuffer");
      request.helpers.onInputChange("");
      request.helpers.setCursorOffset(0);
      request.helpers.setPastedContents(Json::object());
      need(request.helpers.resetHistory,"resetHistory");
      request.helpers.resetHistory();
      request.helpers.clearBuffer();
      output.resolve({});return output;
    }

    services->startQueryProfile();
    Json command={{"value",finalInput},{"preExpansionValue",input},
                  {"mode",mode}};
    if(hasImages)command["pastedContents"]=pasted;
    if(request.skipSlashCommands)command["skipSlashCommands"]=true;
    if(request.uuid)command["uuid"]=*request.uuid;
    observe(executeUserInput(request,
      CommandList{std::make_shared<Json>(std::move(command))},services),output);
  }catch(...){output.reject(std::current_exception());}
  return output;
}
} // namespace batbox::interactive::submit
