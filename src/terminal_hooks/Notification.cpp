#include "terminal_hooks/Notification.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace batbox::terminal_hooks {
TerminalNotification::TerminalNotification(WriteRawRef writeRaw,
    NotificationServices services):writeRaw_(std::move(writeRaw)),
    services_(std::move(services)){}

void TerminalNotification::writeWrapped(const Json& parts)const{
  if(!writeRaw_||!services_.osc||!services_.wrapForMultiplexer)
    throw std::logic_error("Terminal notification imports required");
  (*writeRaw_)(services_.wrapForMultiplexer(services_.osc(parts)));
}
void TerminalNotification::notifyITerm2(const std::string& message,
    const std::optional<std::string>& title)const{
  const auto displayString=title&&!title->empty()?
    *title+":\n"+message:message;
  writeWrapped(Json::array({9,"\n\n"+displayString}));
}
void TerminalNotification::notifyKitty(const std::string& message,
    const std::string& title,double id)const{
  const auto idString=value::numberToString(id);
  writeWrapped(Json::array({99,"i="+idString+":d=0:p=title",title}));
  writeWrapped(Json::array({99,"i="+idString+":p=body",message}));
  writeWrapped(Json::array({99,"i="+idString+":d=1:a=focus",""}));
}
void TerminalNotification::notifyGhostty(const std::string& message,
    const std::string& title)const{
  writeWrapped(Json::array({777,"notify",title,message}));
}
void TerminalNotification::notifyBell()const{
  if(!writeRaw_)throw std::logic_error("Terminal write import required");
  (*writeRaw_)("\a");
}
void TerminalNotification::progress(std::optional<ProgressState> state,
    std::optional<double> percentage)const{
  if(!services_.isProgressReportingAvailable)
    throw std::logic_error("Progress capability import required");
  if(!services_.isProgressReportingAvailable())return;
  if(!state){writeWrapped(Json::array({9,4,0,""}));return;}
  const double rounded=value::round(percentage.value_or(0));
  const double pct=std::isnan(rounded)?rounded:
    std::max(0.0,std::min(100.0,rounded));
  switch(*state){
    case ProgressState::Completed:
      writeWrapped(Json::array({9,4,0,""}));break;
    case ProgressState::Error:
      writeWrapped(Json::array({9,4,2,value::numberToString(pct)}));break;
    case ProgressState::Indeterminate:
      writeWrapped(Json::array({9,4,3,""}));break;
    case ProgressState::Running:
      writeWrapped(Json::array({9,4,1,value::numberToString(pct)}));break;
  }
}
std::shared_ptr<TerminalNotification> NotificationHook::render(
    WriteRawRef writeRaw,const NotificationServices& services){
  if(!writeRaw)throw std::runtime_error(
    "useTerminalNotification must be used within TerminalWriteProvider");
  if(memo_&&previousWrite_==writeRaw)return memo_;
  previousWrite_=writeRaw;
  memo_=std::make_shared<TerminalNotification>(
    std::move(writeRaw),services);
  return memo_;
}
}
