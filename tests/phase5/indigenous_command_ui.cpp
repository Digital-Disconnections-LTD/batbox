#include "interactive/CommandUi.hpp"
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace batbox;
using interactive::CommandUi;
using interactive::CommandCompletion;
using interactive::CommandDescriptor;
using interactive::UiAction;
using interactive::UiRequest;

static void expect(bool condition,const char* message){
  if(!condition)throw std::runtime_error(message);
}
int main(int argc,char** argv){
  event_loop::Loop owner;
  std::vector<std::string> trace;
  std::optional<UiRequest> visible;
  CommandUi::Callbacks callbacks;
  callbacks.onUiChanged=[&](std::optional<UiRequest> ui){
    visible=ui;
    trace.push_back(ui?"ui:"+ui->id:"ui:clear");
  };
  callbacks.onSettled=[&](const CommandDescriptor& command,
                          const CommandCompletion& completion,bool failed){
    trace.push_back("settled:"+command.name+":"+
      (failed?"failed":completion.result.value_or("empty")));
  };
  callbacks.onError=[&](const std::string& error){
    trace.push_back("error:"+error);
  };
  callbacks.onQueueBlocked=[&](bool blocked){
    trace.push_back(blocked?"queue:blocked":"queue:ready");
  };
  callbacks.onPoetryFocus=[&](bool focused){
    trace.push_back(focused?"focus:prompt":"focus:modal");
  };
  auto ui=std::make_shared<CommandUi>(owner,callbacks);

  UiRequest modal;
  modal.kind=UiRequest::Kind::Select;
  modal.id="config";modal.title="Configuration";
  modal.choices={"one","two"};
  ui->start(CommandDescriptor{"config","",false,true},[&](
      CommandUi::Done done,CommandUi::Ready ready,CommandUi::Error){
    ready(modal,[done=std::move(done)](const UiAction& action){
      if(action.kind==UiAction::Kind::Activate&&action.index==1){
        CommandCompletion result;
        result.result="Configuration dismissed";
        result.options.display=interactive::DoneOptions::Display::System;
        done(std::move(result));
      }
    });
  });
  expect(ui->busy()&&ui->queueBlocked()&&ui->hasActiveLocalUi()&&
    !ui->poetryFocused()&&visible&&visible->id=="config", "modal start");
  UiRequest gadget;gadget.id="tool";
  ui->setGadgetOverlay(gadget);
  expect(visible&&visible->id=="config","tool must not overwrite command");
  ui->dispatchAction(UiAction{UiAction::Kind::Activate,1,{}});
  expect(ui->busy()&&visible&&visible->id=="config", "completion awaits microtask");
  owner.runReady();
  expect(!ui->busy()&&!ui->queueBlocked()&&!visible&&ui->poetryFocused(),
         "modal completion cleanup");

  // A command that completes while loading may still return a UI request.
  // The request is ignored after onDone, preserving input focus.
  ui->start(CommandDescriptor{"early","",false,false},[&](
      CommandUi::Done done,CommandUi::Ready ready,CommandUi::Error){
    done(CommandCompletion{});
    modal.id="late";
    ready(modal,[](const UiAction&){});
  });
  expect(!visible&&ui->poetryFocused(),"early completion must skip UI");
  owner.runReady();
  expect(!ui->busy()&&!ui->queueBlocked(),"early cleanup");

  // Failed loading explicitly clears UI before the failed result resumes.
  ui->start(CommandDescriptor{"broken","",false,false},[&](
      CommandUi::Done,CommandUi::Ready,CommandUi::Error error){
    error("load failed");
  });
  expect(!visible&&ui->queueBlocked()&&ui->poetryFocused(),
         "error clears UI before settlement");
  owner.runReady();
  expect(!ui->busy()&&!ui->queueBlocked(),"error cleanup");

  const std::vector<std::string> expected={
    "queue:blocked","ui:config","focus:modal",
    "settled:config:Configuration dismissed","ui:clear",
    "queue:ready","focus:prompt",
    "queue:blocked","settled:early:empty","ui:clear","queue:ready",
    "queue:blocked","error:load failed","ui:clear",
    "settled:broken:failed","ui:clear","queue:ready"};
  if(trace!=expected){
    for(const auto& entry:trace)std::cerr<<entry<<'\n';
    return 1;
  }
  if(argc>1&&std::string(argv[1])=="--probe"){
    const char* names[]={"normal","early","error"};
    std::size_t caseIndex=0;
    bool first=true;
    for(const auto& entry:trace){
      if(entry=="queue:blocked"){
        if(!first)std::cout<<"]}\n";
        std::cout<<"{\"kind\":\""<<names[caseIndex++]<<"\",\"events\":[";
        first=true;
        continue;
      }
      std::string event;
      if(entry.starts_with("ui:"))event=entry;
      else if(entry.starts_with("settled:"))event="settled";
      else if(entry.starts_with("error:"))event=entry;
      else continue;
      if(!first)std::cout<<',';
      std::cout<<'"'<<event<<'"';
      first=false;
    }
    std::cout<<"]}\n";
    return 0;
  }
  std::cout<<"Command UI: modal completion, early done, and error ordering pass\n";
}
