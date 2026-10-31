#include "terminal_runtime_roster/MeasureElement.hpp"
#include "terminal_dom/FullLayoutPort.hpp"
#include <stdexcept>

namespace batbox::terminal_runtime_roster {
MeasuredElement measureElement(const terminal_dom::DOMElement& node){
  if(!node.yogaNode)return {0,0};
  const auto yoga=std::dynamic_pointer_cast<terminal_dom::FullLayoutNode>(
    node.yogaNode);
  if(!yoga)throw std::logic_error("Full layout node required");
  return {yoga->getComputedWidth(),yoga->getComputedHeight()};
}
}
