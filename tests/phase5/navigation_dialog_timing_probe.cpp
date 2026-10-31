#include "interactive/navigation/DialogFocus.hpp"
#include <iostream>

using namespace batbox;
using namespace batbox::interactive;
using namespace batbox::interactive::lifecycle;
using namespace batbox::interactive::navigation;
int main(int argc,char** argv){
  if(argc>1&&std::string(argv[1])!="--probe")return 2;
  event_loop::Loop loop;
  QueryGuard guard;
  PresentationState presentation;
  MessageState messages(std::make_shared<TurnMessages>());
  auto app=std::make_shared<app_state::AppState>();
  app->fields=std::make_shared<value::SharedRecord>();
  LoadingServices services;
  services.store=std::make_shared<app_state::AppStateStore>(app);
  services.nowMs=[]{return 0.0;};
  auto loading=std::make_shared<LoadingState>(loop,guard,presentation,
    messages,std::move(services));
  int repins=0;
  DialogFocus focus(*loading,[&]{++repins;});
  struct Step {Dialog dialog;bool active;double now;};
  const std::vector<Step> steps={
    {{},false,0},{"tool-permission",true,100},
    {"tool-permission",true,110},{"prompt",true,125},
    {"tool-permission",true,200},{{},false,240},
    {{},true,250}
  };
  for(std::size_t i=0;i<steps.size();++i){
    const auto& step=steps[i];
    DialogInputs input;
    input.gadgetPermission=step.dialog=="tool-permission";
    input.poetryRequest=step.dialog=="prompt";
    input.isLoading=step.active;
    const auto selected=focus.observe(input,step.now);
    std::cout<<Json{{"focused",selected?Json(*selected):Json()},
      {"total",loading->totalPausedMs()},
      {"pauseStart",loading->pauseStartTime()?
        Json(*loading->pauseStartTime()):Json()},
      {"repins",repins}}.dump()<<'\n';
  }
}
