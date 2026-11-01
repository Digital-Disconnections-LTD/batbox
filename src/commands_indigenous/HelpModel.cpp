#include "commands_indigenous/UiCommands.hpp"
#include "commands/Roster.hpp"
#include <algorithm>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace batbox::commands_indigenous {
namespace {
void required(bool bound,const char* name){
  if(!bound)throw std::invalid_argument(std::string("Model command import required: ")+name);
}
interactive::CommandCompletion complete(std::optional<std::string> result,
    interactive::DoneOptions::Display display=
      interactive::DoneOptions::Display::Default){
  interactive::CommandCompletion output;
  output.result=std::move(result);output.options.display=display;return output;
}
std::string bold(const ModelServices& services,const std::string& text){
  return services.bold?services.bold(text):text;
}
std::string renderLabel(const ModelServices& services,
    const std::optional<std::string>& model){
  required(bool(services.renderDefaultModelSetting)&&
    bool(services.defaultModelSetting),"model display");
  const auto rendered=services.renderDefaultModelSetting(
    model.value_or(services.defaultModelSetting()));
  return model?rendered:rendered+" (default)";
}
bool in(const std::string& value,
    std::initializer_list<std::string_view> options){
  return std::find(options.begin(),options.end(),value)!=options.end();
}
void setAndFinish(const std::optional<std::string>& model,
    const std::optional<std::string>& effort,bool picker,
    const std::shared_ptr<ModelServices>& services,
    const interactive::CommandUi::Done& done){
  required(bool(services->getState)&&bool(services->setModel),"state/model setter");
  const auto prior=services->getState();
  services->setModel(model);
  std::string message="Set model to "+bold(*services,renderLabel(*services,model));
  if(effort)message+=" with "+bold(*services,*effort)+" effort";
  std::optional<bool> fastToggled;
  required(bool(services->isFastModeEnabled),"fast mode feature");
  if(services->isFastModeEnabled()){
    required(bool(services->clearFastModeCooldown)&&
      bool(services->isFastModeSupportedByModel),"fast mode status");
    services->clearFastModeCooldown();
    if(!services->isFastModeSupportedByModel(model)&&prior.fastMode){
      required(bool(services->setFastMode),"fast mode setter");
      services->setFastMode(false);
      fastToggled=false;
    }else if(services->isFastModeSupportedByModel(model)&&prior.fastMode){
      if(!picker||(services->isFastModeAvailable&&
          services->isFastModeAvailable())){
        message+=" · Fast mode ON";fastToggled=true;
      }
    }
  }
  required(bool(services->isExtendedContextMergeEnabled)&&
    bool(services->billedAsExtraUsage),"billing display");
  if(services->billedAsExtraUsage(model,fastToggled==true,
      services->isExtendedContextMergeEnabled()))
    message+=" · Billed as extra usage";
  if(fastToggled==false)message+=" · Fast mode OFF";
  done(complete(message));
}
void inlineModel(const std::string& arg,
    const std::shared_ptr<ModelServices>& services,
    interactive::CommandUi::Done done,
    interactive::CommandUi::Error error){
  try{
    const std::optional<std::string> model=arg=="default"?
      std::nullopt:std::optional<std::string>(arg);
    if(model){
      required(bool(services->isModelAllowed),"model allowlist");
      if(!services->isModelAllowed(*model)){
        done(complete("Model '"+*model+
          "' is not available. Your organization restricts model selection.",
          interactive::DoneOptions::Display::System));return;
      }
      required(bool(services->premiumExtendedContextUnavailable)&&
        bool(services->standardExtendedContextUnavailable),
        "extended context access");
      if(services->premiumExtendedContextUnavailable(*model)){
        done(complete("Premium model with 1M context is not available for your account.",
          interactive::DoneOptions::Display::System));return;
      }
      if(services->standardExtendedContextUnavailable(*model)){
        done(complete("Standard model with 1M context is not available for your account.",
          interactive::DoneOptions::Display::System));return;
      }
    }
    if(!model){setAndFinish(model,std::nullopt,false,services,done);return;}
    required(bool(services->isKnownAlias),"model aliases");
    if(services->isKnownAlias(*model)){
      setAndFinish(model,std::nullopt,false,services,done);return;
    }
    required(bool(services->validateModel),"model validation");
    services->validateModel(*model).then(
      [services,model,done](const Json* validation,
          const std::exception_ptr& failure){
        if(failure){
          try{std::rethrow_exception(failure);}
          catch(const std::exception& e){
            done(complete(std::string("Failed to validate model: ")+e.what(),
              interactive::DoneOptions::Display::System));
          }
          return;
        }
        if(validation&&value::property(*validation,"valid")==true){
          try{setAndFinish(model,std::nullopt,false,services,done);}
          catch(const std::exception& e){
            done(complete(std::string("Failed to validate model: ")+e.what(),
              interactive::DoneOptions::Display::System));
          }
          return;
        }
        const auto& message=validation?value::property(*validation,"error"):Json();
        done(complete(message.is_string()?
          message.get<std::string>():"Model '"+*model+"' not found",
          interactive::DoneOptions::Display::System));
      });
  }catch(const std::exception& failure){error(failure.what());}
}
}
std::string helpSection(const std::string& name){
  static const std::unordered_map<std::string,std::string> sections={
    {"clear","Flight"},{"compact","Flight"},{"resume","Flight"},{"rename","Flight"},
    {"rewind","Flight"},{"export","Flight"},{"copy","Flight"},{"exit","Flight"},
    {"model","Model and settings"},{"effort","Model and settings"},
    {"config","Model and settings"},{"theme","Model and settings"},
    {"permissions","Model and settings"},{"fast","Model and settings"},
    {"bats","Bats and Missions"},{"agents","Bats and Missions"},
    {"leader","Bats and Missions"},{"missions","Bats and Missions"},
    {"tasks","Bats and Missions"},{"bashes","Bats and Missions"},
    {"memory","Roost and context"},{"project","Roost and context"},
    {"context","Roost and context"},{"status","Roost and context"},
    {"cost","Roost and context"},{"mcp","Roost and context"},
    {"skills","Roost and context"},{"init","Roost and context"},
    {"hooks","Roost and context"},{"doctor","Roost and context"}};
  const auto found=sections.find(name);
  return found==sections.end()?"More":found->second;
}
int sectionOrder(const std::string& section){
  static const std::vector<std::string> order={"Flight","Model and settings",
    "Bats and Missions","Roost and context","More"};
  const auto found=std::find(order.begin(),order.end(),section);
  return static_cast<int>(found-order.begin());
}
user_input::CommandEntry makeHelpCommand(
    std::shared_ptr<user_input::UiCommandRouter> router,
    std::function<std::vector<Json>()> commands){
  if(!router||!commands)
    throw std::invalid_argument("Help command router and catalog required");
  Json descriptor={{"type","local-jsx"},{"name","help"},
    {"description","Show help and available commands"}};
  return user_input::bindUiCommand(std::move(descriptor),router,
    [commands](const std::string&){
      return [commands](auto done,auto ready,auto error){
        try{
          struct Entry {std::string name,description,section;};
          std::vector<Entry> entries;
          std::unordered_set<std::string> seen;
          for(const auto& command:commands()){
            const auto& name=value::property(command,"name");
            if(!name.is_string())continue;
            if(value::truthy(value::property(command,"isHidden")))continue;
            if(!seen.insert(name.get<std::string>()).second)continue;
            const auto& description=value::property(command,"description");
            entries.push_back({name.get<std::string>(),
              description.is_string()?description.get<std::string>():std::string{},
              helpSection(name.get<std::string>())});
          }
          std::stable_sort(entries.begin(),entries.end(),[](const Entry& left,
              const Entry& right){
            const auto a=sectionOrder(left.section),b=sectionOrder(right.section);
            return a!=b?a<b:left.name<right.name;
          });
          // Keys sit after the commands; choosing one simply closes help.
          const std::vector<std::pair<std::string,std::string>> keys={
            {"esc","Interrupt the model, or return from a Bat"},
            {"ctrl+o","Transcript with full Gadget output"},
            {"ctrl+r","Search poetry history"},
            {"ctrl+b","Send running work to the background"},
            {"ctrl+e","Show earlier messages in the transcript"},
            {"@ path","Mention a file in your poetry"},
            {"! command","Run a shell command in the Roost"}};
          auto request=std::make_shared<interactive::UiRequest>();
          request->kind=interactive::UiRequest::Kind::Select;
          request->id="help";request->title="Help";
          request->body="Batbox commands and keys. Enter puts a command in your poetry.";
          request->hint="↑↓ browse · enter use · esc close";
          for(const auto& entry:entries){
            request->choices.push_back("/"+entry.name);
            request->choiceInfo.push_back({entry.description,{},entry.section});
          }
          const auto commandCount=request->choices.size();
          for(const auto& [key,label]:keys){
            request->choices.push_back(key);
            request->choiceInfo.push_back({label,{},"Keys"});
          }
          auto respond=std::make_shared<std::function<void(const interactive::UiAction&)>>();
          std::weak_ptr<std::function<void(const interactive::UiAction&)>> weak=respond;
          *respond=[request,commandCount,done,ready,weak](const interactive::UiAction& action){
            using Kind=interactive::UiAction::Kind;
            if(action.kind==Kind::MoveSelection){
              request->selectedIndex=std::min(action.index,request->choices.size()-1);
              if(const auto strong=weak.lock())
                ready(*request,[strong](const interactive::UiAction& next){(*strong)(next);});
              return;
            }
            if(action.kind==Kind::Activate&&action.index<commandCount){
              auto completion=complete(std::nullopt,interactive::DoneOptions::Display::Skip);
              completion.options.nextInput=request->choices[action.index]+" ";
              completion.options.submitNextInput=false;
              done(std::move(completion));
              return;
            }
            if(action.kind==Kind::Activate||action.kind==Kind::Cancel||
               action.kind==Kind::Back)
              done(complete("Help dialog dismissed",
                interactive::DoneOptions::Display::System));
          };
          ready(*request,[respond](const interactive::UiAction& action){(*respond)(action);});
        }catch(const std::exception& failure){error(failure.what());}
      };
    });
}
user_input::CommandEntry makeModelCommand(
    std::shared_ptr<user_input::UiCommandRouter> router,
    std::shared_ptr<ModelServices> services){
  if(!router||!services)
    throw std::invalid_argument("Model command router and services required");
  required(bool(services->renderModelName)&&bool(services->mainLoopModel),
    "dynamic model description");
  Json descriptor={{"type","local-jsx"},{"name","model"},
    {"description","Set the Batbox model (currently "+
      services->renderModelName(services->mainLoopModel())+")"},
    {"argumentHint","[model]"},
    {"immediate",services->immediate?services->immediate():false}};
  return user_input::bindUiCommand(std::move(descriptor),router,
    [services](const std::string& args){
      return [services,arg=value::trim(args)](auto done,auto ready,auto error){
        try{
          if(in(arg,{"list","show","display","current","view","get",
               "check","describe","print","version","about","status","?"})){
            required(bool(services->getState),"state");
            const auto state=services->getState();
            const auto label=renderLabel(*services,state.baseModel);
            const auto effort=state.effort?" (effort: "+*state.effort+")":"";
            if(state.flightModel)
              done(complete("Current model: "+bold(*services,
                renderLabel(*services,state.flightModel))+
                " (Flight override from plan mode)\nBase model: "+
                label+effort));
            else done(complete("Current model: "+label+effort));
            return;
          }
          if(in(arg,{"help","-h","--help"})){
            done(complete("Run /model to open the model selection menu, or "
              "/model [modelName] to set the model.",
              interactive::DoneOptions::Display::System));return;
          }
          if(!arg.empty()){
            services->loop.queueMicrotask([services,arg,done,error]{
              inlineModel(arg,services,done,error);
            });
            return;
          }
          required(bool(services->choices)&&bool(services->getState),
            "model picker");
          auto options=services->choices();
          interactive::UiRequest request;
          request.kind=interactive::UiRequest::Kind::Select;
          request.id="model";request.title="Select model";
          for(const auto& choice:options)
            request.choices.push_back(choice.label);
          const auto current=services->getState();
          request.body="Current: "+renderLabel(*services,current.baseModel);
          ready(request,[services,done,error,options=std::move(options)](
              const interactive::UiAction& action){
            try{
              if(action.kind==interactive::UiAction::Kind::Cancel){
                const auto state=services->getState();
                done(complete("Kept model as "+bold(*services,
                  renderLabel(*services,state.baseModel)),
                  interactive::DoneOptions::Display::System));return;
              }
              if(action.kind!=interactive::UiAction::Kind::Activate||
                 action.index>=options.size())return;
              const auto& choice=options[action.index];
              setAndFinish(choice.model,choice.effort,true,services,done);
            }catch(const std::exception& failure){error(failure.what());}
          });
        }catch(const std::exception& failure){error(failure.what());}
      };
    });
}
} // namespace batbox::commands_indigenous
