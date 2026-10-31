#include "terminal_dom/IndigenousYogaDom.hpp"
#include <cassert>
#include <iostream>
#include <stdexcept>

using namespace batbox;
namespace td=batbox::terminal_dom;
int main(){
  bool missingFactory=false;
  try{td::indigenousDomServices({});}
  catch(const std::invalid_argument&){missingFactory=true;}
  assert(missingFactory);
  const auto services=td::indigenousYogaDomServices();
  assert(services->expandTabs("a\tb")=="a       b");
  const auto measured=services->measureText("漢字\nabc",3);
  assert(measured.width==4&&measured.height==3);
  assert(services->wrapText("abcdef",3,"wrap")=="abc\ndef");
  td::NodeStash stash;
  auto root=td::createNode("ink-root",services);
  auto text=td::createNode("ink-text",services);
  auto leaf=td::createTextNode("abcdef");
  td::appendChildNode(*text,leaf,stash);
  td::appendChildNode(*root,text,stash);
  const auto direct=td::measureTextNode(*text,3,
    td::LayoutMeasureMode::Exactly,*services);
  assert(direct.width&&*direct.width==3);
  assert(direct.height&&*direct.height==2);
  auto layout=std::dynamic_pointer_cast<td::FullLayoutNode>(root->yogaNode);
  assert(layout);
  layout->setWidth(3);layout->setHeight(3);
  layout->calculateLayout(3,3);
  auto textLayout=std::dynamic_pointer_cast<td::FullLayoutNode>(text->yogaNode);
  assert(textLayout);
  assert(textLayout->getComputedWidth()==3);
  assert(textLayout->getComputedHeight()==2);
  std::cout<<"Native DOM Yoga and Bun text services compose\n";
}
