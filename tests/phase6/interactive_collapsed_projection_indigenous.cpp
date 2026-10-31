#include "interactive/cards_collapsed/CollapsedProjection.hpp"
#include "interactive/cards_collapsed/CollapsedBinding.hpp"
#include "interactive/cards_collapsed/VerboseGadgetUseProjection.hpp"
#include "ftxui/dom/node.hpp"
#include "ftxui/screen/screen.hpp"
#include <cassert>
#include <iostream>

using namespace batbox::interactive::cards_collapsed;
int main(){
  CollapsedProjectionImports imports;
  imports.blackCircle="●";
  imports.subtle=ftxui::Color::GrayDark;
  imports.isBlinking=[](bool){return true;};
  imports.renderVerboseGadgetUse=[](const batbox::Json&){return ftxui::text("VERBOSE");};
  imports.formatSecondsShort=[](double){return std::string("1.2s");};
  imports.basename=[](const std::string&){return std::string("MEMORY.md");};
  imports.renderAnsi=[](const std::string&){return ftxui::text("MEMORY");};
  CollapsedCard card;
  card.visible=true;card.active=true;card.summary={{"Reading ",false,{}},
    {"2",true,{}},{" files",false,{}}};
  card.hintLines={"src/main.cpp"};card.hookSummary="Ran 1 PreToolUse hook (1.2s)";
  auto element=projectCollapsedReadSearch(card,imports);
  assert(element);
  auto screen=ftxui::Screen::Create(ftxui::Dimension::Fixed(60),
                                    ftxui::Dimension::Fixed(5));
  ftxui::Render(screen,*element);
  const auto text=screen.ToString();
  assert(text.find("Reading ")!=std::string::npos);
  assert(text.find(" files")!=std::string::npos);
  assert(text.find("src/main.cpp")!=std::string::npos);
  assert(text.find("Ran 1 PreToolUse hook")!=std::string::npos);
  card.verbose=true;card.verboseGadgetUses={batbox::Json{{"type","assistant"}}};
  element=projectCollapsedReadSearch(card,imports);
  assert(element);
  screen=ftxui::Screen::Create(ftxui::Dimension::Fixed(40),
                                ftxui::Dimension::Fixed(2));
  ftxui::Render(screen,*element);
  assert(screen.ToString().find("VERBOSE")!=std::string::npos);
  VerboseProjectionImports verbose;
  verbose.blackCircle="●";
  verbose.isBlinking=[](bool){return true;};
  verbose.renderComponent=[](const batbox::Json& value){
    return ftxui::text(value.get<std::string>());};
  VerboseGadgetUseCard child;
  child.visible=true;child.name="Bash";child.useMessage="pwd";
  child.hasResultMessage=true;child.resultMessage="done";
  const auto projected=projectVerboseGadgetUse(child,verbose);
  assert(projected);
  screen=ftxui::Screen::Create(ftxui::Dimension::Fixed(40),
                                ftxui::Dimension::Fixed(4));
  ftxui::Render(screen,*projected);
  assert(screen.ToString().find("Bash")!=std::string::npos);
  assert(screen.ToString().find("pwd")!=std::string::npos);
  assert(screen.ToString().find("done")!=std::string::npos);
  batbox::gadgets::Gadget gadget;gadget.name="Bash";
  gadget.renderGadgetUseMessage=[](const batbox::Json&,const batbox::Json&){
    return batbox::Json();};
  batbox::gadgets::Gadgets gadgets{&gadget};
  batbox::interactive::MessageLookups lookups;
  batbox::interactive::GadgetIDs active;
  VerboseGadgetUseServices services;
  services.readSelectedBackground=[](){return batbox::Json();};
  services.safeParseInput=[](const batbox::gadgets::Gadget&,
    const batbox::Json& raw)->std::optional<batbox::Json>{return raw;};
  services.userFacingName=[](const batbox::gadgets::Gadget&,
    const std::optional<batbox::Json>&){return std::string("Bash");};
  services.renderUse=[](const batbox::gadgets::Gadget&,const batbox::Json&,
    const batbox::Json&){return batbox::Json("pwd");};
  card.verboseGadgetUses={batbox::Json{{"type","assistant"},
    {"message",{{"content",batbox::Json::array({batbox::Json{
      {"type","tool_use"},{"id","id"},{"name","Bash"},
      {"input",{{"command","pwd"}}}}})}}}}};
  const auto bound=projectCollapsedWithIndigenousGadgets(card,gadgets,lookups,
    active,services,verbose,imports);
  assert(bound);
  screen=ftxui::Screen::Create(ftxui::Dimension::Fixed(40),
                                ftxui::Dimension::Fixed(3));
  ftxui::Render(screen,*bound);
  assert(screen.ToString().find("Bash")!=std::string::npos);
  assert(screen.ToString().find("pwd")!=std::string::npos);
  assert(!projectCollapsedReadSearch(CollapsedCard{},imports));
  std::cout<<"5 native collapsed-card projection cases pass\n";
}
