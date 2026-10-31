#include "interactive/cards_gadget_children/GadgetSuccessProjection.hpp"
#include "ftxui/dom/node.hpp"
#include "ftxui/screen/screen.hpp"
#include <cassert>
#include <iostream>

using namespace batbox::interactive::cards_gadget_children;
using batbox::interactive::cards_gadget_result::HookProgressKind;

int main(){
  int renderCalls=0;
  SuccessProjectionImports imports;
  imports.columns=40;
  imports.subtle=ftxui::Color::GrayDark;
  imports.success=ftxui::Color::GreenLight;
  imports.tickGlyph="✓";
  imports.renderGadgetMessage=[&](const batbox::Json& value){
    ++renderCalls;
    assert(value=="RESULT");
    return ftxui::text("RESULT");
  };
  GadgetSuccessCard card;
  card.visible=true;
  card.renderedMessage="RESULT";
  card.classifierRule="allow pwd";
  card.yoloReason="known";
  card.hookProgress={HookProgressKind::TranscriptSummary,1,"PostToolUse",
                     "1 PostToolUse hook ran"};
  auto element=projectGadgetSuccess(card,imports);
  assert(element&&renderCalls==1);
  auto screen=ftxui::Screen::Create(ftxui::Dimension::Fixed(40),
                                    ftxui::Dimension::Fixed(4));
  ftxui::Render(screen,*element);
  assert(screen.CellAt(0,0).character=="R");
  assert(screen.CellAt(2,1).character=="⎿");
  assert(screen.CellAt(5,1).character=="✓");
  assert(screen.CellAt(5,2).character=="A");
  assert(screen.CellAt(5,3).character=="1");
  assert(!projectGadgetSuccess(GadgetSuccessCard{},imports));
  card.rendersAsAssistantText=true;
  const auto plain=projectGadgetSuccess(card,imports);
  assert(plain&&renderCalls==2);
  std::cout<<"3 native tool-success projection cases pass\n";
}
