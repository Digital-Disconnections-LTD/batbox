#include "terminal_reconciler/HostConfig.hpp"
#include "terminal_dom/style/Style.hpp"

namespace batbox::terminal_reconciler {
HostConfigServices indigenousHostConfigServices(
    terminal_dom::DomServicesRef dom,terminal_dom::NodeStash& stash,
    DebugRepaints& debugRepaints,
    terminal_hit_test::PointerHandlerStore* pointerHandlers){
  HostConfigServices services;
  services.dom=std::move(dom);
  services.stash=&stash;
  services.props=indigenousHostPropImports(pointerHandlers);
  services.setTextStyles=[](terminal_dom::DOMElement& node,
      const batbox::value::ValueRef& styles){
    terminal_dom::setTextStyles(node,styles);
  };
  services.debugRepaints=&debugRepaints;
  services.applyStyleDiff=[](terminal_dom::FullLayoutNode& yoga,
      const Json& style,const Json* resolved){
    terminal_dom::style::applyStyles(yoga,style,resolved);
  };
  return services;
}
} // namespace batbox::terminal_reconciler
