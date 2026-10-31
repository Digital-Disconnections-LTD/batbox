#include "application_runtime/ApplicationRuntime.hpp"
#include <cassert>
#include <iostream>

using namespace batbox;
namespace ar=batbox::application_runtime;
namespace ix=batbox::interactive;

int main(){
  event_loop::Loop loop;
  ix::PresentationState view;
  auto state=std::make_shared<app_state::AppState>();
  state->fields=std::make_shared<value::SharedRecord>();
  auto store=std::make_shared<app_state::AppStateStore>(state);
  auto stash=file_state_stash::createFileStateStashWithSizeLimit(100);
  auto initial=std::make_shared<ix::TurnMessages>();
  ar::Inputs inputs(loop,view);
  inputs.store=store;inputs.initialMessages=initial;inputs.readFileState=stash;
  inputs.engine=std::make_shared<query::AsyncEngineServices>();
  inputs.nowMs=[]{return 100.0;};
  inputs.feature=[](const std::string&){return false;};
  inputs.onDetachedError=[](std::exception_ptr error){
    if(error)std::rethrow_exception(error);
  };
  inputs.context.getMcpGadgets=[](const app_state::AppStateRef&){
    return gadgets::Gadgets{};
  };
  inputs.context.loadedNestedMemoryPaths=
    std::make_shared<runtime::OrderedStringSet>();
  inputs.context.discoveredSkillNames=
    std::make_shared<runtime::OrderedStringSet>();
  inputs.makeQueryParams=[]{return ix::QueryExecutionParams{};};
  inputs.bindSubmitProviders=[](ix::submit::SubmitServices&){};
  inputs.bindRuntimeProviders=[](ix::repl_runtime::RuntimeServices&){};
  inputs.makeSubmitRequest=[](const std::string& text,const ix::InputState&){
    ix::submit::PoetrySubmitRequest request;request.input=text;return request;
  };
  inputs.presentationImports.stream.connectorTextEnabled=[]{return false;};
  inputs.presentationImports.stream.nowMs=[]{return 100.0;};
  inputs.presentationImports.newConversationId=[]{return "next";};
  inputs.presentationImports.removeTranscriptMessage=[](const Json&){};
  int redraws=0;inputs.requestRedraw=[&]{++redraws;};
  auto app=ar::ApplicationRuntime::create(std::move(inputs));
  assert(app->messages().messages()==initial);
  assert(&app->submitServices()->guard==&app->turnState()->guard);
  assert(app->contextFactory());
  auto message=std::make_shared<Json>(Json{{"type","assistant"},{"uuid","a"},
    {"message",{{"content",Json::array({Json{{"type","text"},{"text","hello"}}})}}}});
  query::AsyncEngineStep step;step.event=message;
  app->executionServices()->onQueryEvent(step);
  assert(view.messages.size()==1&&view.messages.front()==message);
  assert(app->messages().messages()->size()==1&&
    app->messages().messages()->front()==message);
  assert(!view.transcriptRows.empty()&&view.transcriptRows.front().text=="hello");
  assert(redraws>0);
  std::cout<<"application runtime stream/state composition passed\n";
}
