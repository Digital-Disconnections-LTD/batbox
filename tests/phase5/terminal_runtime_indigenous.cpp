#include "terminal_runtime/TerminalControls.hpp"
#include "terminal_dom/IndigenousYogaDom.hpp"
#include <cassert>
#include <iostream>
using namespace batbox;
namespace {
struct Output final:terminal_runtime::OutputPort {
  std::string bytes;
  bool isTTY()const override{return false;}
  std::optional<double> columns()const override{return 80;}
  std::optional<double> rows()const override{return 24;}
  void write(const std::string& text)override{bytes+=text;}
};
struct Input final:terminal_runtime::InputPort {
  bool isTTY()const override{return false;}
  bool isRaw()const override{return false;}
  bool hasSetRawMode()const override{return false;}
  void setRawMode(bool)override{assert(false);}
  std::vector<terminal_runtime::ListenerRef> listeners(const std::string&)const override{return {};}
  void addListener(const std::string&,terminal_runtime::ListenerRef)override{}
  void removeListener(const std::string&,terminal_runtime::ListenerRef)override{}
  std::optional<std::string> read()override{return {};}
};
}
int main(){
  event_loop::Loop loop;Output output;Input input;
  auto styles=std::make_shared<terminal_screen::StylePool>();
  auto chars=std::make_shared<terminal_screen::CharPool>();
  auto links=std::make_shared<terminal_screen::HyperlinkPool>();
  terminal_runtime::ControlServices controls;
  controls.ownerLoop=&loop;
  int renders=0,teardowns=0;
  controls.flushSyncFromReconciler=[]{};
  controls.onRender=[&]{++renders;};
  controls.resetLog=[]{};
  controls.renderCurrentNode=[](terminal_runtime::ReactNodeRef){};
  controls.supportsExtendedKeys=[]{return false;};
  controls.logForDebugging=[](const std::string&,const std::string&){};
  controls.screenDebug=[](const std::string&,const std::string&){};
  auto terminal=std::make_shared<terminal_runtime::TerminalControls>(
    output,input,styles,chars,links,std::move(controls));
  assert(terminal->frontFrame().screen->charPool==chars);
  assert(terminal->frontFrame().viewport.width==80);
  terminal->pause();assert(terminal->isPaused());
  terminal->resume();assert(!terminal->isPaused());
  terminal->repaint();
  assert(terminal->frontFrame().screen->hyperlinkPool==links);
  auto root=terminal_dom::createNode("ink-root",terminal_dom::indigenousYogaDomServices());
  assert(root->yogaNode);
  terminal_log_update::LogUpdate log({false,styles},{
    []{return 0.;},[](const std::string&){},
    std::make_shared<termio::OscModule>([]{return std::string();})});
  terminal_runtime::ShutdownServices shutdown;
  shutdown.log=&log;
  shutdown.output=termio::indigenousOutputServices({},[]{return std::string();});
  shutdown.writeSyncStdout=[&](const std::string& text){output.write(text);};
  shutdown.supportsTabStatus=[]{return false;};
  shutdown.wrapForMultiplexer=[](const std::string& text){return text;};
  shutdown.updateContainerNull=[&]{++teardowns;};
  shutdown.flushSyncWork=[]{};
  shutdown.deleteInstance=[]{};
  shutdown.rootNode=root;
  terminal->bindShutdown(std::move(shutdown));
  bool exited=false;
  terminal->waitUntilExit().then([&](const event_loop::Unit*,const std::exception_ptr& error){
    assert(!error);exited=true;
  });
  terminal->unmount();
  loop.run();
  assert(exited&&terminal->isUnmounted()&&!root->yogaNode);
  assert(renders==3&&teardowns==1&&output.bytes.find('\n')!=std::string::npos);
  terminal->unmount();assert(teardowns==1);
  std::cout<<"native runtime pools/layout/diff/shutdown/exit lifecycle passed\n";
}
