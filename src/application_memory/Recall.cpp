#include "application_memory/Provider.hpp"
#include "application/Async.hpp"
#include "memory_prefetch/Bindings.hpp"
#include "runtime/IndigenousPath.hpp"
#include "runtime/Platform.hpp"
#include "transport/Errors.hpp"
#include "transport/RetryAdapter.hpp"
#include <algorithm>

namespace batbox::application_memory {
std::shared_ptr<memory_prefetch::Services> Provider::makePrefetchServices(){
  requireOwner();
  if(prefetch_)return prefetch_;
  auto service=std::make_shared<memory_prefetch::Services>(
    memory_prefetch::Services{inputs_.owner,{},nullptr,{},{},{},{},{},{},{},{},{},{}});
  service->paths=paths();service->pathStash=&pathStash_;
  service->cwd=runtime::pathToUtf8(inputs_.config.cwd);
  const auto weak=weak_from_this();
  service->searchFeatureEnabled=[weak]{
    const auto self=weak.lock();if(!self||self->closing_)return false;
    if(self->prefetch_)self->prefetch_->paths=self->paths();
    return self->recallEnabled();
  };
  service->defaultStandardModel=[model=inputs_.model]{return model->config().mainModel;};
  service->nowMillis=[]{return static_cast<double>(platform::nowMillis());};
  service->isAbortError=[](const std::exception& error){
    return dynamic_cast<const transport::APIUserAbortError*>(&error)||
      dynamic_cast<const model::UserAbortError*>(&error);
  };
  service->logWarning=inputs_.log;
  service->logError=[log=inputs_.log](std::exception_ptr error){
    try{if(error)std::rethrow_exception(error);}
    catch(const std::exception& failure){log(std::string("Memory recall: ")+failure.what());}
  };
  service->prefetchStarted=[weak](std::stop_source stop){
    if(auto self=weak.lock()){
      ++self->recallJobs_;self->recallStops_.push_back(std::move(stop));
    }
  };
  service->prefetchFinished=[weak](std::stop_source stop){
    if(auto self=weak.lock()){
      if(self->recallJobs_>0)--self->recallJobs_;
      std::erase(self->recallStops_,stop);
      if(self->recallJobs_==0){
        auto waiters=std::move(self->idleWaiters_);self->idleWaiters_.clear();
        for(const auto& waiter:waiters)waiter.resolve({});
      }
    }
  };
  // Reuse the sideQuery request builder and the configured indigenous
  // endpoint/auth/TLS owner. Memory never discovers or imports credentials.
  side_query::Services side;
  side.getClient=[weak](side_query::ClientOptions options){
    auto self=weak.lock();if(!self)throw std::runtime_error("Memory owner closed");
    event_loop::Deferred<std::shared_ptr<side_query::Client>> ready(self->inputs_.owner);
    auto model=self->inputs_.model;
    auto modelServices=model->makeQueryServices({},Json{{"model",options.model}});
    auto client=modelServices.createClient();
    auto result=std::make_shared<side_query::Client>();
    result->create=[loop=&self->inputs_.owner,client,model,
        retryImports=modelServices.retry,options](Json body,std::stop_token stop){
      return application::runInWorker<side_query::ResponseRef>(*loop,
          [client,model,body=std::move(body),stop,retryImports,options]()mutable{
        (void)model; // Keep callback imports and credentials owner alive.
        retry::RetryOptions policy;policy.model=options.model;
        policy.maxRetries=options.maxRetries;policy.signal=stop;
        policy.querySource="memdir_relevance";
        retryImports.getClient=[]{return Json();};
        retryImports.yield=[](const Json&){};
        retryImports.operation=[client,body,stop](const Json&,double,
            const retry::RetryContext&){
          try{return client->request(body,false,{},stop).body;}
          catch(...){throw retry::RetryFailure(
            transport::errorRecordFromException(std::current_exception()));}
        };
        return std::make_shared<Json>(retry::withRetry(policy,retryImports));
      });
    };
    ready.resolve(std::move(result));return ready;
  };
  side.getModelBetas=[](const std::string&){return std::vector<std::string>{};};
  side.supportsStructuredOutputs=[model=inputs_.model](const std::string& id){
    const auto profile=model->findProfile(id);
    return profile&&profile->structuredOutputs;
  };
  side.systemPoetryPrefix=[](const Json&){return std::string{};};
  side.normalizeModel=[model=inputs_.model](const std::string& id){
    const auto profile=model->findProfile(id);
    return !profile||profile->wireModel.empty()?id:profile->wireModel;
  };
  side.nowMillis=service->nowMillis;
  side.setLastApiCompletionTimestamp=[](double){};
  memory_prefetch::bindSideQuery(*service,std::move(side));
  prefetch_=std::move(service);return prefetch_;
}
} // namespace batbox::application_memory
