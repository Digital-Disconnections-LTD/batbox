#include "auth/storage/IndigenousPrefetch.hpp"
#include "process/ExecFileNoThrow.hpp"
#include "runtime/JsonValue.hpp"
#include <stdexcept>
#include <utility>

namespace batbox::auth::storage {
PrefetchServices indigenousPrefetchServices(event_loop::Loop& loop,
    std::shared_ptr<KeychainState> state,
    std::function<std::string()> platform,
    std::function<bool()> bareMode,std::string executable){
  if(!state||!platform||!bareMode||executable.empty())
    throw std::invalid_argument("Missing native keychain prefetch binding");
  PrefetchServices services;
  services.platform=std::move(platform);
  services.bareMode=std::move(bareMode);
  services.primeStash=[state](const std::optional<std::string>& output){
    state->prime(output);
  };
  services.spawnSecurity=[&loop,state,executable](
      const std::string& serviceName){
    event_loop::Deferred<PrefetchResult> output(loop);
    process::ExecOptions options;
    options.timeout=10000;
    options.maxBuffer=1000000;
    options.stdin="ignore";
    try{
      process::indigenousExeca(loop)({executable,
        {"find-generic-password","-a",state->username(),"-w","-s",
          serviceName},options}).then([output](
          const process::ExecaResult* result,
          const std::exception_ptr& error){
        if(error||result->failed){
          output.resolve({std::nullopt,!error&&result->shortMessage&&
            *result->shortMessage=="Command timed out"});
          return;
        }
        const auto text=value::trim(result->stdoutText);
        output.resolve({text.empty()?std::nullopt:
          std::optional<std::string>(text),false});
      });
    }catch(...){output.resolve({std::nullopt,false});}
    return output;
  };
  return services;
}
}
