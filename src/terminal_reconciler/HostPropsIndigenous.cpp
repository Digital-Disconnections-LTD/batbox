#include "terminal_reconciler/HostProps.hpp"
#include "terminal_dom/style/Style.hpp"
#include <stdexcept>

namespace batbox::terminal_reconciler {
HostPropImports indigenousHostPropImports(
    terminal_hit_test::PointerHandlerStore* pointerHandlers){
  HostPropImports imports;
  imports.pointerHandlers=pointerHandlers;
  imports.setStyle=[](terminal_dom::DOMElement& node,
      const batbox::value::ValueRef& style){
    terminal_dom::setStyle(node,style);
  };
  imports.applyStyles=[](terminal_dom::FullLayoutNode& yoga,
      const batbox::value::ValueRef& style){
    const auto undefined=Json(Json::value_t::discarded);
    terminal_dom::style::applyStyles(yoga,style?*style:undefined);
  };
  imports.setAttribute=[](terminal_dom::DOMElement& node,
      const std::string& key,const Json& value){
    terminal_dom::setAttribute(node,key,value);
  };
  return imports;
}
} // namespace batbox::terminal_reconciler
