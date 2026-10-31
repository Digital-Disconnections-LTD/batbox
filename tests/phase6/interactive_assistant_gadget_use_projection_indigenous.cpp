#include "interactive/cards_gadget_use/GadgetUseProjection.hpp"
#include "ftxui/dom/node.hpp"
#include "ftxui/screen/screen.hpp"
#include <cassert>
#include <iostream>

using namespace batbox::interactive::cards_gadget_use;
int main(){
  UseProjectionImports imports;
  imports.blackCircle="●";
  imports.subtle=ftxui::Color::GrayDark;
  imports.success=ftxui::Color::GreenLight;
  imports.error=ftxui::Color::Red;
  imports.isBlinking=[](bool){return true;};
  imports.renderGadgetMessage=[](const batbox::Json& value){
    assert(value.is_string());return ftxui::text(value.get<std::string>());};
  GadgetUseCard card;
  card.visible=true;card.name="Bash";card.nameWidth=6;
  card.showDot=true;card.useText="pwd";
  card.progressKind=GadgetUseCard::ProgressKind::WaitingPermission;
  auto element=projectGadgetUse(card,imports);
  assert(element);
  auto screen=ftxui::Screen::Create(ftxui::Dimension::Fixed(40),
                                    ftxui::Dimension::Fixed(3));
  ftxui::Render(screen,*element);
  assert(screen.CellAt(0,0).character=="●");
  assert(screen.CellAt(2,0).character=="B");
  assert(screen.ToString().find("Waiting for permission")!=std::string::npos);
  card.transparent=true;card.name.clear();card.useText=batbox::Json();
  card.progressKind=GadgetUseCard::ProgressKind::GadgetProgress;
  card.progress="PROGRESS";
  element=projectGadgetUse(card,imports);
  assert(element);
  screen=ftxui::Screen::Create(ftxui::Dimension::Fixed(40),
                                ftxui::Dimension::Fixed(2));
  ftxui::Render(screen,*element);
  assert(screen.ToString().find("PROGRESS")!=std::string::npos);
  assert(screen.ToString().find("Bash")==std::string::npos);
  assert(!projectGadgetUse(GadgetUseCard{},imports));
  std::cout<<"3 native assistant tool-use projection cases pass\n";
}
