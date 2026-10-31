#include "terminal_events/Event.hpp"
#include "terminal_events/KeyboardEvent.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>

namespace batbox::terminal_events {struct EventTarget {int id;};}
using namespace batbox;
namespace te=batbox::terminal_events;
int main(){
  Json rows=Json::array();
  double clock=0;
  const te::MonotonicNow now=[&]{return ++clock;};
  te::Event base;
  rows.push_back(Json{{"kind","baseBefore"},
    {"stopped",base.didStopImmediatePropagation()}});
  base.stopImmediatePropagation();
  rows.push_back(Json{{"kind","baseAfter"},
    {"stopped",base.didStopImmediatePropagation()}});
  {
    te::TerminalEvent event("custom",now);
    te::EventTarget target{1},current{2};
    event.setTarget(&target);event.setCurrentTarget(&current);
    event.setEventPhase("capturing");event.preventDefault();
    rows.push_back(Json{{"kind","terminal"},{"type",event.type()},
      {"time",event.timeStamp()},{"bubbles",event.bubbles()},
      {"cancelable",event.cancelable()},{"target",event.target()==&target},
      {"current",event.currentTarget()==&current},
      {"phase",event.eventPhase()},{"prevented",event.defaultPrevented()},
      {"stopped",event.isPropagationStopped()},
      {"immediate",event.isImmediatePropagationStopped()}});
    event.stopImmediatePropagation();event.setCurrentTarget(nullptr);
    event.setEventPhase("none");
    rows.push_back(Json{{"kind","terminalStopped"},
      {"phase",event.eventPhase()},{"current",nullptr},
      {"stopped",event.isPropagationStopped()},
      {"immediate",event.isImmediatePropagationStopped()}});
  }
  {
    te::TerminalEvent event("custom",now,te::TerminalEventInit{false,false});
    event.preventDefault();event.stopPropagation();
    rows.push_back(Json{{"kind","notCancelable"},{"time",event.timeStamp()},
      {"bubbles",event.bubbles()},{"cancelable",event.cancelable()},
      {"prevented",event.defaultPrevented()},
      {"stopped",event.isPropagationStopped()},
      {"immediate",event.isImmediatePropagationStopped()}});
  }
  {
    te::EventTarget related{3};te::FocusEvent event("blur",now,&related);
    rows.push_back(Json{{"kind","focus"},{"type",event.type()},
      {"time",event.timeStamp()},{"bubbles",event.bubbles()},
      {"cancelable",event.cancelable()},
      {"related",event.relatedTarget()==&related}});
  }
  const auto keyboard=[&](std::string kind,te::ParsedKey parsed){
    te::KeyboardEvent event(parsed,now);
    rows.push_back(Json{{"kind","keyboard:"+kind},
      {"time",event.timeStamp()},{"key",event.key},
      {"ctrl",event.ctrl},{"shift",event.shift},{"meta",event.meta},
      {"super",event.superKey},{"fn",event.fn}});
  };
  {
    te::ParsedKey value;value.name="c";value.sequence="\x03";value.ctrl=true;
    keyboard("ctrl",value);
  }
  {
    te::ParsedKey value;value.name="digit3";value.sequence="3";
    keyboard("printable",value);
  }
  {
    te::ParsedKey value;value.name="down";value.sequence="\x1b[B";
    keyboard("special",value);
  }
  {
    te::ParsedKey value;value.name="delete";value.sequence="\x7f";
    keyboard("delete",value);
  }
  {
    te::ParsedKey value;value.name="emoji";value.sequence="😀";
    keyboard("astral",value);
  }
  {
    te::ParsedKey value;value.sequence="\x01";value.ctrl=true;
    keyboard("ctrlUndefined",value);
  }
  {
    te::ClickEvent event(4,7,true);event.localCol=2;event.localRow=3;
    event.stopImmediatePropagation();
    rows.push_back(Json{{"kind","click"},{"col",event.col},
      {"row",event.row},{"localCol",event.localCol},
      {"localRow",event.localRow},{"blank",event.cellIsBlank},
      {"stopped",event.didStopImmediatePropagation()}});
  }
  {
    te::TerminalFocusEvent event("terminalblur");
    rows.push_back(Json{{"kind","terminalFocus"},{"type",event.type},
      {"stopped",event.didStopImmediatePropagation()}});
  }
  std::cout<<rows.dump()<<'\n';
}
