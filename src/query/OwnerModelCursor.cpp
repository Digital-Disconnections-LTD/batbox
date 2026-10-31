#include "query/OwnerModelCursor.hpp"
#include "query/OwnerSegmentInvoker.hpp"
#include "query/OwnerStreamConsume.hpp"
#include "query/OwnerWatchdogTimer.hpp"
#include "requests/RequestBuilder.hpp"

namespace batbox::query {
AsyncModelCursor makeOwnerSegmentQueryModelCursor(event_loop::Loop& loop,
    model::PreflightInputs input,model::QueryServices services,
    OwnerPreflightAwaits awaitedImports,
    const std::function<void()>& onStreamingFallback,
    OwnerRequestAwaits requestAwaits){
  if(!loop.isOwnerThread())throw std::logic_error(
    "Owner-bound model cursor requires loop owner");
  const auto dispatcher=loop.dispatcher();
  auto* owner=&loop;
  const auto external=services.signal;
  if(services.preparePreflightSegment)
    throw std::invalid_argument("Owner preflight is already overridden");
  if(services.consumeStreamSegment)
    throw std::invalid_argument("Owner stream consumption is already overridden");
  if(!services.createClient&&!requestAwaits.createClient)
    throw std::invalid_argument("Client creation import is required");
  const auto priorFallback=services.onStreamingFallback;
  const auto priorClient=services.createClient;
  return makeProducerModelCursor(loop,
    [dispatcher,owner,external,input=std::move(input),services=std::move(services),
      awaitedImports=std::move(awaitedImports),priorFallback,
      priorClient,requestAwaits=std::move(requestAwaits),onStreamingFallback](
      const std::function<void(const model::MessageRef&)>& yield,
      std::stop_token stop) mutable {
      std::stop_source networkStop;
      auto cancelNetwork=[source=networkStop]() mutable {source.request_stop();};
      std::stop_callback onExternalStop(external,cancelNetwork);
      std::stop_callback onCursorClose(stop,cancelNetwork);
      services.signal=networkStop.get_token();
      const auto debugLog=services.logForDebugging;
      services.logForDebugging=[dispatcher,debugLog](const std::string& message,
          const std::string& level){
        if(!debugLog)return;
        (void)invokeOwnerSegment(dispatcher,{},[&]{
          debugLog(message,level);return event_loop::Unit{};
        });
      };
      const auto diagnosticLog=services.logForDiagnosticsNoPII;
      services.logForDiagnosticsNoPII=[dispatcher,diagnosticLog](
          const std::string& level,const std::string& event){
        if(!diagnosticLog)return;
        (void)invokeOwnerSegment(dispatcher,{},[&]{
          diagnosticLog(level,event);return event_loop::Unit{};
        });
      };
      services.scheduleWatchdogTimer=makeOwnerWatchdogTimer(*owner);
      const auto checkpoint=services.queryCheckpoint;
      services.queryCheckpoint=[dispatcher,stop,checkpoint](const std::string& name){
        if(!checkpoint)return;
        (void)invokeOwnerSegment(dispatcher,stop,[&]{
          checkpoint(name);return event_loop::Unit{};
        });
      };
      const auto headlessCheckpoint=services.headlessProfilerCheckpoint;
      services.headlessProfilerCheckpoint=[dispatcher,stop,headlessCheckpoint](
          const std::string& name){
        if(!headlessCheckpoint)return;
        (void)invokeOwnerSegment(dispatcher,stop,[&]{
          headlessCheckpoint(name);return event_loop::Unit{};
        });
      };
      const auto endProfile=services.endQueryProfile;
      services.endQueryProfile=[dispatcher,stop,endProfile]{
        if(!endProfile)return;
        (void)invokeOwnerSegment(dispatcher,stop,[&]{
          endProfile();return event_loop::Unit{};
        });
      };
      const auto startActivity=services.startFlightActivity;
      services.startFlightActivity=[dispatcher,startActivity](const std::string& reason){
        if(!startActivity)throw std::invalid_argument(
          "Start session activity import is required");
        (void)invokeOwnerSegment(dispatcher,{},[&]{
          startActivity(reason);return event_loop::Unit{};
        });
      };
      const auto stopActivity=services.stopFlightActivity;
      services.stopFlightActivity=[dispatcher,stopActivity](const std::string& reason){
        if(!stopActivity)throw std::invalid_argument(
          "Stop session activity import is required");
        (void)invokeOwnerSegment(dispatcher,{},[&]{
          stopActivity(reason);return event_loop::Unit{};
        });
      };
      services.createClient=[dispatcher,owner,stop,priorClient,requestAwaits]{
        auto pending=invokeOwnerSegment(dispatcher,stop,[&]{
          if(requestAwaits.createClient)return requestAwaits.createClient();
          event_loop::Deferred<std::shared_ptr<transport::Client>> ready(*owner);
          ready.resolve(priorClient());
          return ready;
        });
        return waitOwnerDeferred(dispatcher,stop,pending);
      };
      services.withRetrySegment=[owner,stop,requestAwaits](
          const retry::RetryOptions& options,retry::Dependencies& imports){
        return withRetryOwnerSegments(*owner,options,imports,
          requestAwaits.retry,stop);
      };
      services.prepareNonStreamingAttemptSegment=[dispatcher,stop](
          double attempt,const retry::RetryContext& context,double timeoutMs,
          const model::NonStreamingDependencies& imports){
        return invokeOwnerSegment(dispatcher,stop,[&]{
          return model::prepareNonStreamingAttempt(attempt,context,timeoutMs,imports);
        });
      };
      services.makeFallbackMessageSegment=[dispatcher,stop](
          const std::function<Json()>& construct){
        return invokeOwnerSegment(dispatcher,stop,[&]{return construct();});
      };
      services.accountFallbackSegment=[dispatcher](
          const std::function<model::FallbackAccounting()>& account){
        return invokeOwnerSegment(dispatcher,{},[&]{return account();});
      };
      services.processErrorSegment=[dispatcher,stop](
          const std::function<std::optional<Json>()>& process){
        return invokeOwnerSegment(dispatcher,stop,[&]{return process();});
      };
      services.processPostStreamSegment=[dispatcher,stop](
          const model::PostStreamInputs& input,
          const model::PostStreamServices& imports){
        (void)invokeOwnerSegment(dispatcher,stop,[&]{
          model::processCompletedStream(input,imports);
          return event_loop::Unit{};
        });
      };
      const auto priorBuild=services.buildParamsSegment;
      services.preparePreflightSegment=[dispatcher,owner,stop,awaitedImports](
          const model::PreflightInputs& request,
          const model::PreflightServices& imports){
        auto pending=invokeOwnerSegment(dispatcher,stop,[&]{
          return startOwnerPreflight(*owner,request,imports,awaitedImports,stop);
        });
        return waitOwnerDeferred(dispatcher,stop,pending);
      };
      services.buildParamsSegment=[dispatcher,stop,priorBuild](
          const Json& retryContext,requests::RequestCapture& capture,
          const requests::BuilderDependencies& imports,
          const requests::WireProtocol& wire,
          const requests::BuilderHeaders& headers){
        return invokeOwnerSegment(dispatcher,stop,[&]{
          return priorBuild?priorBuild(retryContext,capture,imports,wire,headers):
            requests::buildParamsFromContext(retryContext,capture,imports,wire,headers);
        });
      };
      services.prepareStreamingAttemptSegment=[dispatcher,stop](
          double attempt,const retry::RetryContext& context,
          model::StreamingAttemptState& state,
          const model::StreamingAttemptDependencies& imports){
        return invokeOwnerSegment(dispatcher,stop,[&]{
          return model::prepareStreamingAttempt(attempt,context,state,imports);
        });
      };
      services.finishStreamingAttemptSegment=[dispatcher,stop](
          model::StreamingAttemptState& state,model::OpenedStream opened,
          const model::StreamingAttemptDependencies& imports){
        return invokeOwnerSegment(dispatcher,stop,[&]{
          return model::finishStreamingAttempt(state,std::move(opened),imports);
        });
      };
      services.onStreamingFallback=[dispatcher,stop,priorFallback,
          onStreamingFallback]{
        (void)invokeOwnerSegment(dispatcher,stop,[&]{
          if(priorFallback)priorFallback();
          if(onStreamingFallback)onStreamingFallback();
          return event_loop::Unit{};
        });
      };
      services.consumeStreamSegment=[dispatcher,stop](
          model::OpenedStream& opened,streaming::Reducer& reducer,
          const std::function<void(const streaming::MessageRef&)>& yield,
          model::StreamWatchdog* watchdog,
          const std::function<void()>& onFirstChunk,
          model::StreamObservation* observation){
        consumeOwnerStream(dispatcher,stop,opened,reducer,yield,watchdog,
          onFirstChunk,observation);
      };
      services.completeRequestSegment=[dispatcher,stop](
          const model::CompletionState& state,
          const model::CompletionServices& imports){
        (void)invokeOwnerSegment(dispatcher,stop,[&]{
          model::completeQueryModel(state,imports);
          return event_loop::Unit{};
        });
      };
      (void)model::queryModel(input,std::move(services),yield);
    },{},[](std::exception_ptr error){
      try{if(error)std::rethrow_exception(error);}
      catch(const model::GeneratorClosed&){return true;}
      catch(const transport::APIUserAbortError&){return true;}
      catch(...){return false;}
      return false;
    });
}
} // namespace batbox::query
