#include "repository/Detect.hpp"
#include <stdexcept>

namespace batbox::repository {
event_loop::Deferred<std::optional<std::string>> getGithubRepo(event_loop::Loop& loop,
    const ParserImport& importParser,
    const std::function<event_loop::Deferred<std::optional<std::string>>()>& getRemoteUrl,
    const Debug& debug){
  event_loop::Deferred<std::optional<std::string>> result(loop);
  try{
    if(!importParser)throw std::invalid_argument("Repository parser import required");
    importParser().then([result,getRemoteUrl,debug](const ParseGitRemote* parser,const std::exception_ptr& error){
      if(error){result.reject(error);return;}
      try{
        if(!getRemoteUrl)throw std::invalid_argument("Repository getRemoteUrl binding required");
        getRemoteUrl().then([result,parse=*parser,debug](const std::optional<std::string>* remote,const std::exception_ptr& failure){
          if(failure){result.reject(failure);return;}
          try{
            if(!*remote||remote->value().empty()){
              if(debug)debug("Local GitHub repo: unknown");
              result.resolve(std::nullopt);return;
            }
            const auto parsed=parse(**remote);
            if(parsed&&parsed->host=="github.com"){
              const auto name=parsed->owner+"/"+parsed->name;
              if(debug)debug("Local GitHub repo: "+name);
              result.resolve(name);return;
            }
            if(debug)debug("Local GitHub repo: unknown");
            result.resolve(std::nullopt);
          }catch(...){result.reject(std::current_exception());}
        });
      }catch(...){result.reject(std::current_exception());}
    });
  }catch(...){result.reject(std::current_exception());}
  return result;
}
}
