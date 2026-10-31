#include "terminal_dom/IndigenousYogaDom.hpp"

namespace batbox::terminal_dom {
DomServicesRef indigenousYogaDomServices(bool bunAvailable){
  return indigenousDomServices([]()->LayoutRef{return createLayoutNode();},
    bunAvailable);
}
} // namespace batbox::terminal_dom
