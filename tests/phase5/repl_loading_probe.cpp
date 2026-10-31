#include "interactive/lifecycle/LoadingState.hpp"
#include <iostream>
#include <stdexcept>

using namespace batbox;
using namespace batbox::interactive;
using namespace batbox::interactive::lifecycle;
Json run(bool hasTip){
  event_loop::Loop loop;
  QueryGuard guard;
  PresentationState presentation;
  presentation.apiMetrics.push_back({7,1,1,0,0});
  presentation.streamingGadgetUses.push_back({1,Json::object(),""});
  std::vector<std::string> trace;
  auto message=std::make_shared<Json>(Json{{"type","assistant"},
    {"message",{{"content",Json::array({Json{{"type","tool_use"},
      {"name","Bash"},{"input",{{"command","sudo git status"}}}}})}}}});
  MessageState messages(std::make_shared<TurnMessages>(TurnMessages{message}));
  messages.setUserInputOnProcessing("prompt");
  auto app=std::make_shared<app_state::AppState>();
  app->fields=std::make_shared<value::SharedRecord>();
  if(!hasTip)app->fields->set("spinnerTip","old");
  auto store=std::make_shared<app_state::AppStateStore>(app,
    [&](const app_state::AppStateStore::Change& change){
      const auto current=change.newState->fieldRef("spinnerTip");
      trace.push_back(std::string("spinner:")+
        (current&&current->is_string()?current->get<std::string>():"undefined"));
    });
  LoadingServices services;
  services.store=store;services.nowMs=[]{return 100.0;};
  services.theme={{"name","dark"}};
  services.getTipToShowOnSpinner=[&](const TipContext&){
    trace.push_back("pick");
    event_loop::Deferred<TipRef> value(loop);
    if(!hasTip){value.resolve({});return value;}
    auto tip=std::make_shared<Tip>();
    tip->content=[&](const Json& options){
      trace.push_back("content:"+options.at("theme").at("name").get<std::string>());
      event_loop::Deferred<std::string> content(loop);
      content.resolve("tip text");return content;
    };
    value.resolve(tip);return value;
  };
  services.recordShownTip=[&](const TipRef&){trace.push_back("record");};
  services.endInteractionSpan=[&]{trace.push_back("end");};
  services.clearSpeculativeChecks=[&]{trace.push_back("clear");};
  services.onDetachedError=[](std::exception_ptr error){std::rethrow_exception(error);};
  auto loading=std::make_shared<LoadingState>(loop,guard,presentation,
    messages,std::move(services));
  loading->setResponseLength([](double old){return old+4;});
  const auto metric=presentation.apiMetrics.back();
  loading->onStreamingText([](const auto& old){return std::optional<std::string>(
    old.value_or("")+"abc\nx");});
  const auto streamed=presentation.streamingText;
  loading->setShowStreamingText(false);
  loading->onStreamingText([](const auto&)->std::optional<std::string>{
    throw std::runtime_error("hidden updater called");
  });
  loading->setExternalLoading(true);
  const auto timing=Json{{"start",loading->loadingStartTime()},
    {"paused",loading->totalPausedMs()},
    {"pauseStart",loading->pauseStartTime()?Json(*loading->pauseStartTime()):Json()}};
  loading->resetLoadingState();loading->resetLoadingState();
  for(int i=0;i<8;++i)loop.runReady();
  Json gadgets=Json::array();
  for(const auto& item:loading->bashGadgets())gadgets.push_back(item);
  const auto tip=store->getState()->fieldRef("spinnerTip");
  return Json{{"metric",{{"ttftMs",metric.ttftMs},
    {"firstTokenTime",metric.firstTokenTime},{"lastTokenTime",metric.lastTokenTime},
    {"responseLengthBaseline",metric.responseLengthBaseline},
    {"endResponseLength",metric.endResponseLength}}},
    {"streamed",streamed?Json(*streamed):Json()},
    {"timing",timing},{"responseLength",loading->responseLength()},
    {"metricsLength",presentation.apiMetrics.size()},
    {"streaming",presentation.streamingText?Json(*presentation.streamingText):Json()},
    {"toolsLength",presentation.streamingGadgetUses.size()},
    {"processing",messages.processingInput()?Json(*messages.processingInput()):Json()},
    {"external",loading->isLoading()},
    {"spinnerTip",tip?*tip:Json()},
    {"tipPicked",loading->tipPickedThisTurn()},
    {"bashTools",gadgets},{"bashToolsProcessedIndex",loading->bashGadgetsProcessedIndex()},
    {"trace",trace}};
}
int main(int argc,char** argv){
  if(argc>1&&std::string(argv[1])!="--probe")return 2;
  std::cout<<run(true).dump()<<'\n'<<run(false).dump()<<'\n';
}
