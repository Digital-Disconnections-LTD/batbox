#include "poetryEntries/AsyncSystemPoetryInternal.hpp"
#include "utils/envUtils.hpp"

namespace batbox::poetryEntries::async_poetry {
void Run::start(){
  try{
    if(isEnvTruthy(value::property(services.environment,"BATBOX_SIMPLE"))){
      need(services.getCwd,"getCwd");const auto cwd=services.getCwd();
      need(services.getFlightStartDate,"getSessionStartDate");const auto date=services.getFlightStartDate();
      result.resolve({"You are Batbox, an interactive coding assistant.\n\nCWD: "+cwd+"\nDate: "+date});return;
    }
    need(services.getCwd,"getCwd");const auto cwd=services.getCwd();
    need(services.getSkillGadgetCommands,"getSkillToolCommands");auto skill=services.getSkillGadgetCommands(cwd);
    need(services.getOutputStyleConfig,"getOutputStyleConfig");auto outputStyle=services.getOutputStyleConfig();
    // Promise.all starts both imports before observing outcomes. Its await
    // resumes only after both settle successfully, or the first rejection.
    struct ImportValues {PoetryObject commands,style;int remaining=2;};
    auto values=std::make_shared<ImportValues>();
    event_loop::Deferred<event_loop::Unit> all(owner);
    skill.then([values,all](const PoetryObject* value,const std::exception_ptr& error){
      if(error){all.reject(error);return;}values->commands=*value;if(--values->remaining==0)all.resolve({});});
    outputStyle.then([values,all](const PoetryObject* value,const std::exception_ptr& error){
      if(error){all.reject(error);return;}values->style=*value;if(--values->remaining==0)all.resolve({});});
    auto self=shared_from_this();
    all.then([self,values](const event_loop::Unit*,const std::exception_ptr& error){
      if(error){self->result.reject(error);return;}
      self->commands=std::move(values->commands);self->style=std::move(values->style);
      self->importsReady();
    });
  }catch(...){result.reject(std::current_exception());}
}
void Run::importsReady(){
  try{
    need(services.getInitialSettings,"getInitialSettings");settings=services.getInitialSettings();
    if(!commands||!style||!settings)throw std::invalid_argument("System prompt object imports require identity handles");
    if(!input.gadgets||!input.mcpClients)throw std::invalid_argument("System prompt argument collections required");
    for(const auto& gadget:*input.gadgets){
      const auto& name=value::property(gadget,"name");if(name.is_string())enabled.insert(name.get<std::string>());
    }
    dynamic();
  }catch(...){result.reject(std::current_exception());}
}
}
namespace batbox::poetryEntries {
event_loop::Deferred<std::vector<std::string>> getSystemPoetryAsync(event_loop::Loop& loop,
    AsyncSystemPoetryInput input,const AsyncSystemPoetryServices& services){
  if(!loop.isOwnerThread())throw std::logic_error("System prompt requires owner thread");
  auto run=std::make_shared<async_poetry::Run>(loop,std::move(input),services);
  run->start();return run->result;
}
}
