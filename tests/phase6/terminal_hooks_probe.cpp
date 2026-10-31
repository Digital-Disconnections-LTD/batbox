#include "terminal_hooks/Notification.hpp"
#include "terminal_hooks/Title.hpp"
#include "terminal_hooks/TabStatus.hpp"
#include "termio/OscLink.hpp"
#include <iostream>
#include <regex>

using namespace batbox;
int main(){
  std::vector<std::string> trace;
  const auto makeWrite=[&](std::string label){
    return std::make_shared<terminal_hooks::WriteRaw>(
      [&,label](const std::string& text){trace.push_back(label+text);});};
  const auto write1=makeWrite("write1:"),write2=makeWrite("write2:");
  bool progressAvailable=true;
  terminal_hooks::NotificationServices notificationServices;
  notificationServices.osc=[](const Json& parts){
    return termio::osc(parts,"other");};
  notificationServices.wrapForMultiplexer=[](const std::string& text){
    return text;};
  notificationServices.isProgressReportingAvailable=[&](){
    return progressAvailable;};
  terminal_hooks::NotificationHook notification;
  const auto first=notification.render(write1,notificationServices);
  const auto same=notification.render(write1,notificationServices);
  trace.push_back(std::string("memoSame:")+(first==same?"true":"false"));
  first->notifyITerm2("Hello","Notice");
  first->notifyITerm2("Plain");
  first->notifyKitty("Body","Header",7);
  first->notifyGhostty("Body","Header");
  first->notifyBell();
  first->progress(terminal_hooks::ProgressState::Running,33.6);
  first->progress(terminal_hooks::ProgressState::Error,123.5);
  first->progress(terminal_hooks::ProgressState::Indeterminate);
  first->progress(terminal_hooks::ProgressState::Completed);
  first->progress(std::nullopt);
  progressAvailable=false;
  first->progress(terminal_hooks::ProgressState::Error,90);
  try{notification.render(nullptr,notificationServices);}
  catch(const std::exception& error){
    trace.push_back("providerError:"+std::string(error.what()));}
  const auto changed=notification.render(write2,notificationServices);
  trace.push_back(std::string("memoNew:")+(changed!=first?"true":"false"));
  changed->notifyBell();
  terminal_hooks::TitleServices titleServices;
  titleServices.osc=notificationServices.osc;
  titleServices.stripAnsi=[](const std::string& text){
    return std::regex_replace(text,std::regex("\\x1b\\[[0-9;]*m"),"");};
  bool windows=false;
  titleServices.isWindows=[&](){return windows;};
  titleServices.setProcessTitle=[&](const std::string& value){
    trace.push_back("processTitle:"+value);};
  terminal_hooks::TerminalTitleEffect title;
  title.commit("Hi\x1b[31m red\x1b[0m",write1,titleServices);
  title.commit("Hi\x1b[31m red\x1b[0m",write1,titleServices);
  title.commit(std::nullopt,write1,titleServices);
  title.commit("New",write2,titleServices);
  windows=true;
  title.commit("Win\x1b[31m",write2,titleServices);
  terminal_hooks::TabStatusServices tabServices;
  tabServices.osc=notificationServices.osc;
  tabServices.wrapForMultiplexer=notificationServices.wrapForMultiplexer;
  bool supports=true;
  tabServices.supportsTabStatus=[&](){return supports;};
  terminal_hooks::TabStatusEffect tab;
  tab.commit(terminal_hooks::TabStatusKind::Busy,write1,tabServices);
  tab.commit(terminal_hooks::TabStatusKind::Busy,write1,tabServices);
  tab.commit(std::nullopt,write1,tabServices);
  tab.commit(std::nullopt,write1,tabServices);
  supports=false;
  tab.commit(terminal_hooks::TabStatusKind::Waiting,write1,tabServices);
  supports=true;
  tab.commit(terminal_hooks::TabStatusKind::Waiting,write2,tabServices);
  tab.commit(std::nullopt,write2,tabServices);
  tab.commit(terminal_hooks::TabStatusKind::Idle,write2,tabServices);
  Json out=trace;
  std::cout<<out.dump()<<'\n';
}
