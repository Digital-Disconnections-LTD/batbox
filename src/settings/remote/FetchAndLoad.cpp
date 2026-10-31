#include "settings/remote/FetchAndLoad.hpp"
#include <cerrno>
#include <stdexcept>
#include <system_error>

namespace batbox::settings::remote {
namespace {
using Output=event_loop::Deferred<std::optional<Json>>;
void failOpen(const Output& output,const std::optional<Json>& stashed,
    const FetchAndLoadServices& services){
  try{
    if(stashed){
      services.debug("Remote settings: Using stale cache after error");
      services.stash->setFlightStash(stashed);
      output.resolve(stashed);
    }else output.resolve(std::nullopt);
  }catch(...){output.reject(std::current_exception());}
}
bool isEnoent(const std::exception_ptr& error){
  try{std::rethrow_exception(error);}
  catch(const std::system_error& value){
    return value.code().value()==ENOENT;
  }catch(...){return false;}
}
std::string message(const std::exception_ptr& error){
  try{std::rethrow_exception(error);}
  catch(const std::exception& value){return value.what();}
  catch(...){return "unknown error";}
}
}
event_loop::Deferred<std::optional<Json>> fetchAndLoadRemoteManagedSettings(
    event_loop::Loop& loop,const FetchAndLoadServices& services){
  if(!services.stash||!services.isEligible||!services.fetchWithRetry||
      !services.checkSecurity||!services.handleSecurity||
      !services.saveSettings||!services.unlinkSettings||!services.debug)
    throw std::invalid_argument("Missing remote settings load binding");
  Output output(loop);
  if(!services.isEligible()){output.resolve(std::nullopt);return output;}
  const auto stashed=services.stash->getRemoteManagedSettingsSyncFromStash();
  const std::optional<std::string> checksum=stashed?
    std::optional(computeChecksumFromSettings(*stashed)):std::nullopt;
  try{
    services.fetchWithRetry(checksum).then([output,stashed,services](
        const FetchResult* fetched,const std::exception_ptr& fetchError){
      if(fetchError){failOpen(output,stashed,services);return;}
      try{
        if(!fetched->success){
          if(stashed){
            services.debug("Remote settings: Using stale cache after fetch failure");
            services.stash->setFlightStash(stashed);output.resolve(stashed);
          }else output.resolve(std::nullopt);
          return;
        }
        if(fetched->settings&&fetched->settings->is_null()&&stashed){
          services.debug("Remote settings: Cache still valid (304 Not Modified)");
          services.stash->setFlightStash(stashed);output.resolve(stashed);
          return;
        }
        const Json next=fetched->settings&&value::truthy(*fetched->settings)
          ?*fetched->settings:Json::object();
        if(!value::objectKeys(next).empty()){
          services.checkSecurity(stashed,next).then(
            [output,stashed,next,services](const SecurityDecision* decision,
                const std::exception_ptr& securityError){
              if(securityError){failOpen(output,stashed,services);return;}
              try{
                if(!services.handleSecurity(*decision)){
                  services.debug("Remote settings: User rejected new settings, "
                    "using cached settings");
                  output.resolve(stashed);return;
                }
                services.stash->setFlightStash(next);
                services.saveSettings(next).then([output,next,stashed,services](
                    const event_loop::Unit*,const std::exception_ptr& saveError){
                  if(saveError){failOpen(output,stashed,services);return;}
                  try{
                    services.debug("Remote settings: Applied new settings successfully");
                    output.resolve(next);
                  }catch(...){failOpen(output,stashed,services);}
                });
              }catch(...){failOpen(output,stashed,services);}
            });
          return;
        }
        services.stash->setFlightStash(next);
        try{
          services.unlinkSettings(services.stash->getSettingsPath()).then(
            [output,next,stashed,services](const event_loop::Unit*,
                const std::exception_ptr& unlinkError){
              try{
                if(!unlinkError)
                  services.debug("Remote settings: Deleted cached file (404 response)");
                else if(!isEnoent(unlinkError))
                  services.debug("Remote settings: Failed to delete cached file - "+
                    message(unlinkError));
                output.resolve(next);
              }catch(...){failOpen(output,stashed,services);}
            });
        }catch(...){
          const auto error=std::current_exception();
          try{
            if(!isEnoent(error))
              services.debug("Remote settings: Failed to delete cached file - "+
                message(error));
            output.resolve(next);
          }catch(...){failOpen(output,stashed,services);}
        }
      }catch(...){failOpen(output,stashed,services);}
    });
  }catch(...){failOpen(output,stashed,services);}
  return output;
}
}
