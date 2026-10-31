#include "cli/remote/RemoteIO.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>

using namespace batbox;
int main(){
  event_loop::Loop loop;
  std::string line;
  while(std::getline(std::cin,line)){
    const Json test=value::parse(line);
    Json events=Json::array();
    std::optional<std::string> token;
    if(const auto& value=value::property(test,"token");value.is_string())
      token=value.get<std::string>();
    std::shared_ptr<cli::remote::Transport> transport;
    std::function<void(const std::string&)> onData;
    std::function<void()> onClose;
    std::function<Json()> refresh;
    std::function<void(const std::string&,const std::string&)> lifecycle;
    std::function<void(const std::string&,const Json&)> state;
    std::function<void(const Json&)> metadata;
    cli::remote::Services services;
    services.structured.randomUUID=[]{return std::string("request-1");};
    services.structured.logHookError=[](const auto&,const auto&){};
    services.structured.processLine.notifyCommandLifecycle=[](const auto&,
                                                             const auto&){};
    services.getFlightIngressAuthToken=[&]{return token;};
    services.getFlightId=[]{return std::string("session-1");};
    services.environment=[&](const std::string& name)
        ->std::optional<std::string>{
      if(name=="BATBOX_ENVIRONMENT_RUNNER_VERSION"){
        const auto& value=value::property(test,"version");
        if(value.is_string())return value.get<std::string>();
      }
      if(name=="BATBOX_ENVIRONMENT_KIND"&&test.value("bridge",false))
        return "bridge";
      if(name=="BATBOX_USE_CCR_V2"&&test.value("ccr",false))return "1";
      return {};
    };
    services.isDebugMode=[&]{return test.value("debug",false);};
    services.keepAliveIntervalMs=[]{return 0.0;};
    services.logForDebugging=[&](const std::string& message,const Json&){
      events.push_back(Json::array({"debug",message}));
    };
    services.logError=[&](const std::string& message){
      events.push_back(Json::array({"error",message}));
    };
    services.gracefulShutdown=[&](int code,const std::string& reason){
      events.push_back(Json::array({"shutdown",code,reason}));
    };
    services.writeToStdout=[&](const std::string& value){
      events.push_back(Json::array({"stdout",value}));
    };
    services.ndjsonSafeStringify=[](const Json& value){
      return value::stringify(value);
    };
    if(test.contains("initialPrompt")){
      auto at=std::make_shared<std::size_t>(0);
      services.nextInitialPoetryChunk=[&,at]{
        event_loop::Deferred<std::optional<std::string>> done(loop);
        const auto& chunks=test.at("initialPrompt");
        if(*at<chunks.size())done.resolve(
          chunks.at((*at)++).get<std::string>());
        else done.resolve({});
        return done;
      };
    }
    services.registerCleanup=[&](auto){
      events.push_back(Json::array({"registerCleanup"}));
    };
    services.getTransportForUrl=[&](const Json& url,const Json& headers,
        const std::string& id,std::function<Json()> refreshHeaders){
      events.push_back(Json::array({"transportFactory",
        value::property(url,"href"),headers,id}));
      refresh=std::move(refreshHeaders);
      transport=std::make_shared<cli::remote::Transport>();
      transport->isSse=test.value("ccr",false)&&
        !test.value("wrongTransport",false);
      transport->setOnData=[&](auto callback){
        events.push_back(Json::array({"onData"}));onData=std::move(callback);
      };
      transport->setOnClose=[&](auto callback){
        events.push_back(Json::array({"onClose"}));onClose=std::move(callback);
      };
      transport->connect=[&]{
        events.push_back(Json::array({"connect"}));
        event_loop::Deferred<event_loop::Unit> done(loop);done.resolve({});
        return done;
      };
      transport->write=[&](const Json& message){
        events.push_back(Json::array({"transportWrite",message}));
        event_loop::Deferred<event_loop::Unit> done(loop);done.resolve({});
        return done;
      };
      transport->close=[&]{events.push_back(Json::array({"transportClose"}));};
      return transport;
    };
    services.createCcrClient=[&](cli::remote::Transport& selected,
                                  const Json& url){
      events.push_back(Json::array({"ccrConstruct",selected.isSse,
        value::property(url,"href")}));
      auto client=std::make_shared<cli::remote::CcrClient>();
      client->initialize=[&]{
        events.push_back(Json::array({"ccrInitialize"}));
        event_loop::Deferred<Json> done(loop);
        if(test.value("initError",false))done.reject(
          std::make_exception_ptr(std::runtime_error("init failed")));
        else done.resolve(Json{{"worker","restored"}});
        return done;
      };
      client->writeEvent=[&](const Json& message){
        events.push_back(Json::array({"ccrWrite",message}));
        event_loop::Deferred<event_loop::Unit> done(loop);done.resolve({});
        return done;
      };
      client->flushInternalEvents=[&]{
        events.push_back(Json::array({"ccrFlush"}));
        event_loop::Deferred<event_loop::Unit> done(loop);done.resolve({});
        return done;
      };
      client->internalEventsPending=[]{return std::size_t(3);};
      client->close=[&]{
        events.push_back(Json::array({"ccrClose"}));
        event_loop::Deferred<event_loop::Unit> done(loop);done.resolve({});
        return done;
      };
      client->writeInternalEvent=[&](const auto&,const auto&,const auto&){
        event_loop::Deferred<Json> done(loop);done.resolve(Json{{"ok",true}});
        return done;
      };
      client->readInternalEvents=client->readSubagentInternalEvents=[&]{
        event_loop::Deferred<Json> done(loop);done.resolve(Json::array());
        return done;
      };
      client->reportDelivery=[&](const std::string& id,
                                 const std::string& state){
        events.push_back(Json::array({"delivery",id,state}));
      };
      client->reportState=[&](const std::string& value,const Json& details){
        events.push_back(Json::array({"state",value,details}));
      };
      client->reportMetadata=[&](const Json& value){
        events.push_back(Json::array({"metadata",value}));
      };
      return client;
    };
    services.setInternalEventWriter=[&](auto){
      events.push_back(Json::array({"setWriter"}));
    };
    services.setInternalEventReaders=[&](auto,auto){
      events.push_back(Json::array({"setReaders"}));
    };
    services.setCommandLifecycleListener=[&](auto callback){
      events.push_back(Json::array({"setLifecycle"}));
      lifecycle=std::move(callback);
    };
    services.setFlightStateChangedListener=[&](auto callback){
      events.push_back(Json::array({"setState"}));state=std::move(callback);
    };
    services.setFlightMetadataChangedListener=[&](auto callback){
      events.push_back(Json::array({"setMetadata"}));metadata=std::move(callback);
    };
    Json error,refreshed,restored,pending,input=Json::array();
    try{
      cli::remote::RemoteIO remote(loop,test.value("url",
        std::string("ws://localhost/session-1")),services);
      token="refreshed";
      refreshed=refresh();
      onData(test.value("data",std::string("{\"type\":\"keep_alive\"}\n")));
      if(test.contains("initialPrompt")){
        for(std::size_t i=0;i<test.at("initialPrompt").size();++i){
          remote.structured().nextInput().then([&](const auto* item,
                                                    const auto& failure){
            if(failure)std::rethrow_exception(failure);
            if(item&&item->value)input.push_back(*item->value);
          });
          loop.runReady();
        }
      }
      if(test.contains("write")){
        auto written=remote.write(test.at("write"));
        written.then([](const auto*,const auto& failure){
          if(failure)std::rethrow_exception(failure);
        });
      }
      remote.restoredWorkerState().then([&](const Json* value,
                                            const auto& failure){
        if(failure)std::rethrow_exception(failure);
        restored=*value;
      });
      loop.runReady();
      auto flush=remote.flushInternalEvents();
      flush.then([](const auto*,const auto& failure){
        if(failure)std::rethrow_exception(failure);
      });
      if(test.value("ccr",false)){
        lifecycle("id-1","started");state("running",Json{{"reason","test"}});
        metadata(Json{{"name","session"}});
      }
      pending=remote.internalEventsPending();
      remote.close();loop.runReady();
    }catch(const std::exception& failure){error=failure.what();}
    Json result={{"error",error},{"events",events}};
    if(error.is_null()){
      result["refreshed"]=refreshed;
      result["restored"]=restored;
      result["pending"]=pending;
      if(test.contains("initialPrompt"))result["input"]=input;
    }
    std::cout<<value::stringify(result)<<'\n';
  }
}
