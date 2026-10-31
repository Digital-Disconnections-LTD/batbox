#include "terminal_reconciler/HostStatic.hpp"
#include <iostream>

using namespace batbox;
namespace rec=batbox::terminal_reconciler;
int main(){
  Json calls=Json::array();
  terminal_events::Dispatcher dispatcher({1,2,9,0},
    [](std::exception_ptr){});
  rec::HostStaticImports imports;
  imports.dispatcher=&dispatcher;
  imports.scheduleTimeout=[&](std::function<void()>,double delay){
    calls.push_back(Json::array({"schedule",delay}));
    return std::make_shared<int>(1);
  };
  imports.cancelTimeout=[&](const rec::TimeoutRef& timer){
    calls.push_back(Json::array({"cancel",
      *std::static_pointer_cast<int>(timer)}));
  };
  rec::HostStatic host(imports);
  const auto timer=host.scheduleTimeout([](){},10);host.cancelTimeout(timer);
  Json before={{"priority",host.getCurrentUpdatePriority()},
    {"resolved",host.resolveUpdatePriority()},
    {"type",host.resolveEventType()?Json(*host.resolveEventType()):Json(nullptr)},
    {"time",host.resolveEventTimeStamp()}};
  host.setCurrentUpdatePriority(0);
  terminal_events::TerminalEvent event("click",[]{return 42;});
  dispatcher.currentEvent=&event;
  Json after={{"priority",host.getCurrentUpdatePriority()},
    {"resolved",host.resolveUpdatePriority()},
    {"type",host.resolveEventType()?Json(*host.resolveEventType()):Json(nullptr)},
    {"time",host.resolveEventTimeStamp()}};
  auto instance=std::make_shared<terminal_dom::DOMElement>("ink-box");
  Json row={{"kind","static"},{"calls",calls},
    {"before",before},{"after",after},
    {"isPrimaryRenderer",host.isPrimaryRenderer},
    {"supportsMutation",host.supportsMutation},
    {"supportsPersistence",host.supportsPersistence},
    {"supportsHydration",host.supportsHydration},
    {"noTimeout",host.noTimeout},
    {"portalMount",nullptr},{"clear",host.clearContainer()},
    {"shouldText",host.shouldSetTextContent()},
    {"publicSame",host.getPublicInstance(instance)==instance},
    {"fromNode",nullptr},{"fromScope",nullptr},
    {"maySuspend",host.maySuspendCommit()},
    {"preload",host.preloadInstance()},
    {"ready",nullptr},{"pending",nullptr},
    {"eager",host.shouldAttemptEagerTransition()}};
  std::cout<<Json::array({row}).dump()<<'\n';
}
