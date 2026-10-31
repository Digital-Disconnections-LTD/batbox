#include "application/HostInternal.hpp"
#include "application/Async.hpp"
#include <stdexcept>

namespace batbox::application {
namespace {
std::string errorText(std::exception_ptr error){
  try{if(error)std::rethrow_exception(error);}
  catch(const std::exception& failure){return failure.what();}
  catch(...){return "unknown native application error";}
  return {};
}
}
void Host::Impl::requestExit(){
  if(closing)return;
  closing=true;
  if(fileWatcher)fileWatcher->close();
  if(cli)cli->stop();
  if(permission)permission->cancelAll();
  if(mcpElicitation)mcpElicitation->cancelAll();
  const auto flush=[this]{
    bats::saveStashSafeParams({});
    if(connectors)connectors->shutdown();
    const auto count=std::size_t(bool(flight))+std::size_t(bool(history));
    if(!count){owner.stop();return;}
    auto remaining=std::make_shared<std::size_t>(count);
    const auto settled=[this,remaining](const char* name,
        const std::exception_ptr& error){
      if(error){failed.store(true);log(std::string(name)+" flush failed: "+
        errorText(error));}
      if(--*remaining==0)owner.stop();
    };
    if(flight){
      try{
        flight->flush().then([settled](const event_loop::Unit*,
            const std::exception_ptr& error){settled("Session",error);});
      }catch(...){settled("Session",std::current_exception());}
    }
    if(history){
      try{
        history->flush().then([settled](const event_loop::Unit*,
            const std::exception_ptr& error){settled("History",error);});
      }catch(...){settled("History",std::current_exception());}
    }
  };
  const auto closeMcp=[this,flush]{
    std::vector<std::shared_ptr<mcp::manager::IndigenousRuntime>> managers;
    if(mcpRuntime)managers.push_back(mcpRuntime);
    for(const auto& retired:retiredRoosts)
      if(retired.mcpRuntime)managers.push_back(retired.mcpRuntime);
    if(managers.empty()){flush();return;}
    auto remaining=std::make_shared<std::size_t>(managers.size());
    for(const auto& manager:managers){
      try{
        manager->shutdown().then([this,remaining,flush](
            const event_loop::Unit*,const std::exception_ptr& error){
          if(error){failed.store(true);log("MCP shutdown failed");}
          if(--*remaining==0)flush();
        });
      }catch(...){
        failed.store(true);log("MCP shutdown failed");
        if(--*remaining==0)flush();
      }
    }
  };
  const auto closeLsp=[this,closeMcp]{
    std::vector<std::shared_ptr<application_lsp::Runtime>> generations;
    if(lsp)generations.push_back(lsp);
    for(const auto& retired:retiredRoosts)
      if(retired.lsp)generations.push_back(retired.lsp);
    if(generations.empty()){closeMcp();return;}
    auto futures=std::make_shared<std::vector<std::shared_future<void>>>();
    for(const auto& generation:generations)futures->push_back(generation->shutdownAsync());
    runInWorker<event_loop::Unit>(owner,[generations=std::move(generations),futures]{
      for(auto& future:*futures)future.get();
      return event_loop::Unit{};
    }).then([this,closeMcp](const event_loop::Unit*,const std::exception_ptr& error){
      if(error){failed.store(true);log("LSP shutdown failed: "+errorText(error));}
      closeMcp();
    });
  };
  const auto closeShells=[this,closeLsp]{
    std::vector<application_shell::Host> generations;
    if(managedShell.shell)generations.push_back(managedShell);
    for(const auto& retired:retiredRoosts)
      if(retired.managedShell.shell)generations.push_back(retired.managedShell);
    if(generations.empty()){closeLsp();return;}
    auto remaining=std::make_shared<std::size_t>(generations.size());
    for(const auto& generation:generations){
      try{
        application_shell::stopAndWait(owner,generation).then(
          [this,remaining,closeLsp](const event_loop::Unit*,
              const std::exception_ptr& error){
            if(error){failed.store(true);log("Shell shutdown failed: "+errorText(error));}
            if(--*remaining==0)closeLsp();
          });
      }catch(...){
        failed.store(true);log("Shell shutdown failed: "+
          errorText(std::current_exception()));
        if(--*remaining==0)closeLsp();
      }
    }
  };
  const auto closeHooks=[this,closeShells]{
    std::vector<std::shared_ptr<application_hooks::Provider>> generations;
    if(hooks)generations.push_back(hooks);
    for(const auto& retired:retiredRoosts)
      if(retired.hooks)generations.push_back(retired.hooks);
    if(generations.empty()){closeShells();return;}
    auto remaining=std::make_shared<std::size_t>(generations.size());
    for(const auto& generation:generations){
      try{
        generation->finalizeAsyncHooks().then(
          [this,remaining,closeShells](const event_loop::Unit*,
              const std::exception_ptr& error){
            if(error){failed.store(true);log("Hook shutdown failed: "+
              errorText(error));}
            if(--*remaining==0)closeShells();
          });
      }catch(...){
        failed.store(true);log("Hook shutdown failed: "+
          errorText(std::current_exception()));
        if(--*remaining==0)closeShells();
      }
    }
  };
  const auto closeBats=[this,closeHooks]{
    std::vector<std::shared_ptr<application_bats::IndigenousBatApplication>> generations;
    if(bats)generations.push_back(bats);
    for(const auto& retired:retiredRoosts)
      if(retired.bats)generations.push_back(retired.bats);
    if(generations.empty()){closeHooks();return;}
    auto remaining=std::make_shared<std::size_t>(generations.size());
    for(const auto& generation:generations){
      try{
        generation->shutdown().then([this,remaining,closeHooks](
            const event_loop::Unit*,const std::exception_ptr& error){
          if(error){failed.store(true);log("Agent shutdown failed: "+errorText(error));}
          if(--*remaining==0)closeHooks();
        });
      }catch(...){
        failed.store(true);log("Agent shutdown failed: "+
          errorText(std::current_exception()));
        if(--*remaining==0)closeHooks();
      }
    }
  };
  const auto closeMemory=[this,closeBats]{
    std::vector<std::shared_ptr<application_memory::Provider>> generations;
    if(memory)generations.push_back(memory);
    for(const auto& previous:retiredRoosts)
      if(previous.memory)generations.push_back(previous.memory);
    if(generations.empty()){closeBats();return;}
    auto remaining=std::make_shared<std::size_t>(generations.size());
    const auto settled=[this,remaining,closeBats](std::exception_ptr error){
      if(error){failed.store(true);log("Memory shutdown failed: "+errorText(error));}
      if(--*remaining==0)closeBats();
    };
    for(const auto& generation:generations){
      try{generation->shutdown().then([settled](const event_loop::Unit*,
          std::exception_ptr error){settled(error);});}
      catch(...){settled(std::current_exception());}
    }
  };
  if(!runtime){closeMemory();return;}
  try{
    runtime->stopAndWait().then([this,closeMemory](const event_loop::Unit*,
        const std::exception_ptr& error){
      if(error){failed.store(true);log("Turn shutdown failed: "+errorText(error));}
      closeMemory();
    });
  }catch(...){
    failed.store(true);log("Turn shutdown failed: "+
      errorText(std::current_exception()));closeMemory();
  }
}
void Host::Impl::shutdown(){
  closing=true;
  if(fileWatcher)fileWatcher->close();
  if(cli)cli->stop();
  if(permission)permission->cancelAll();
  if(mcpElicitation)mcpElicitation->cancelAll();
  if(runtime)runtime->stop();
  if(shell)shell->stop();
  if(connectors)connectors->shutdown();
}
} // namespace batbox::application
