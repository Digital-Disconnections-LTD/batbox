#include "commands_indigenous/FlightCommands.hpp"
#include "monster/Sprites.hpp"
#include <stdexcept>

namespace batbox::commands_indigenous {
namespace {
interactive::CommandCompletion result(std::optional<std::string> value,
    interactive::DoneOptions::Display display=
      interactive::DoneOptions::Display::Default){
  interactive::CommandCompletion completed;
  completed.result=std::move(value);completed.options.display=display;
  return completed;
}
void require(bool ready,const char* name){
  if(!ready)throw std::invalid_argument(std::string("Command import required: ")+name);
}
}
user_input::CommandEntry makeCostCommand(std::shared_ptr<CostServices> services){
  if(!services)throw std::invalid_argument("Cost services required");
  user_input::CommandEntry entry;
  entry.descriptor={{"type","local"},{"name","cost"},
    {"description","Show total cost and duration of this Flight"},
    {"supportsNonInteractive",true}};
  entry.loadLocal=[services]{
    event_loop::Deferred<user_input::LocalCall> loaded(services->loop);
    loaded.resolve([services](const std::string&){
      event_loop::Deferred<Json> output(services->loop);
      try{
        require(bool(services->isSubscriber)&&
          bool(services->formatTotalCost),"cost provider");
        std::string text;
        if(services->isSubscriber()){
          require(bool(services->isUsingOverage)&&
            bool(services->showCostForInternalUser),"subscription status");
          text=services->isUsingOverage()?
            "You are currently using your overages to power your Batbox "
            "usage. We will automatically switch you back to your subscription "
            "rate limits when they reset":
            "You are currently using your subscription to power your Batbox usage";
          if(services->showCostForInternalUser())
            text+="\n\n[INTERNAL] Showing cost anyway:\n "+
              services->formatTotalCost();
        }else text=services->formatTotalCost();
        output.resolve(Json{{"type","text"},{"value",text}});
      }catch(...){output.reject(std::current_exception());}
      return output;
    });
    return loaded;
  };
  return entry;
}
user_input::CommandEntry makeRewindCommand(
    std::shared_ptr<RewindServices> services){
  if(!services)throw std::invalid_argument("Rewind services required");
  user_input::CommandEntry entry;
  entry.descriptor={{"type","local"},{"name","rewind"},
    {"aliases",Json::array({"checkpoint"})},
    {"description","Restore code and/or conversation to a previous point"},
    {"argumentHint",""},{"supportsNonInteractive",false}};
  entry.loadLocal=[services]{
    event_loop::Deferred<user_input::LocalCall> loaded(services->loop);
    loaded.resolve([services](const std::string&){
      event_loop::Deferred<Json> output(services->loop);
      try{
        if(services->openMessageSelector)services->openMessageSelector();
        output.resolve(Json{{"type","skip"}});
      }catch(...){output.reject(std::current_exception());}
      return output;
    });return loaded;
  };
  return entry;
}
user_input::CommandEntry makeExitCommand(
    std::shared_ptr<user_input::UiCommandRouter> router,
    std::shared_ptr<ExitServices> services){
  if(!router||!services)
    throw std::invalid_argument("Exit router and services required");
  Json descriptor={{"type","local-jsx"},{"name","exit"},
    {"aliases",Json::array({"quit"})},{"description","Exit Batbox"},
    {"immediate",true}};
  return user_input::bindUiCommand(std::move(descriptor),router,
    [services](const std::string&){
      return [services](auto done,auto ready,auto error){
        try{
          require(bool(services->backgroundFlightsEnabled)&&
            bool(services->isBackgroundFlight)&&
            bool(services->hasCurrentWorktree),"exit state");
          if(services->backgroundFlightsEnabled()&&
             services->isBackgroundFlight()){
            require(bool(services->detachBackgroundFlight),
              "background detach");
            done(result(std::nullopt));
            services->detachBackgroundFlight();return;
          }
          if(services->hasCurrentWorktree()){
            require(bool(services->worktreeExitFlow),"worktree exit flow");
            services->worktreeExitFlow()(std::move(done),
              std::move(ready),std::move(error));return;
          }
          require(bool(services->randomGoodbye)&&
            bool(services->gracefulShutdown),"shutdown");
          done(result(services->randomGoodbye()));
          services->gracefulShutdown(0,"prompt_input_exit").then(
            [error](const event_loop::Unit*,const std::exception_ptr& failure){
              if(failure){
                try{std::rethrow_exception(failure);}
                catch(const std::exception& issue){error(issue.what());}
              }
            });
        }catch(const std::exception& issue){error(issue.what());}
      };
    });
}
user_input::CommandEntry makeMonsterCommand(
    std::shared_ptr<user_input::UiCommandRouter> router,
    std::shared_ptr<MonsterServices> services){
  if(!router||!services||!services->roller)
    throw std::invalid_argument("Monster router and roller required");
  Json descriptor={{"type","local-jsx"},{"name","monster"},
    {"description","View your Batbox monster"}};
  return user_input::bindUiCommand(std::move(descriptor),router,
    [services](const std::string&){
      return [services](auto done,auto ready,auto error){
        try{
          auto configured=services->roller->getMonster();
          Json bones=configured?configured->snapshot():
            services->roller->roll(services->roller->monsterUserId())
              ->bones.snapshot();
          interactive::UiRequest view;
          view.kind=interactive::UiRequest::Kind::Notice;
          view.id="monster";view.title="Monster";
          const auto& name=value::property(bones,"name");
          if(name.is_string())view.body=name.get<std::string>()+'\n';
          for(const auto& line:monster::renderSprite(bones))
            view.body+=line+'\n';
          const auto& rarity=value::property(bones,"rarity");
          if(rarity.is_string())view.body+=rarity.get<std::string>();
          ready(view,[done](const interactive::UiAction& action){
            if(action.kind==interactive::UiAction::Kind::Activate||
               action.kind==interactive::UiAction::Kind::Cancel)
              done(result(std::nullopt,
                interactive::DoneOptions::Display::Skip));
          });
        }catch(const std::exception& issue){error(issue.what());}
      };
    });
}
} // namespace batbox::commands_indigenous
