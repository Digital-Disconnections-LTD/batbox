#include "interactive/cards_gadget_result/HookProgressProjection.hpp"
#include "ftxui/dom/node.hpp"
#include "ftxui/screen/screen.hpp"
#include <cassert>
#include <iostream>

using namespace batbox::interactive;
using namespace batbox::interactive::cards_gadget_result;

int main() {
  MessageDecisionLookups lookups;
  lookups.inProgressHookCounts["tool"]["PreToolUse"]=2;
  const auto transcript=classifyHookProgress("PreToolUse","tool",true,lookups);
  const auto projected=projectHookProgress(transcript,ftxui::Color::GrayDark);
  assert(projected);
  auto screen=ftxui::Screen::Create(ftxui::Dimension::Fixed(40),
                                    ftxui::Dimension::Fixed(1));
  ftxui::Render(screen,*projected);
  assert(screen.CellAt(2,0).character=="⎿");
  assert(screen.CellAt(5,0).character=="2");
  assert(screen.CellAt(7,0).character=="P");
  lookups.inProgressHookCounts["tool"]["SessionStart"]=1;
  const auto running=classifyHookProgress("SessionStart","tool",false,lookups);
  const auto active=projectHookProgress(running,ftxui::Color::GrayDark);
  assert(active);
  screen=ftxui::Screen::Create(ftxui::Dimension::Fixed(40),
                                ftxui::Dimension::Fixed(1));
  ftxui::Render(screen,*active);
  assert(screen.CellAt(5,0).character=="R");
  lookups.resolvedHookCounts["tool"]["SessionStart"]=1;
  assert(!projectHookProgress(classifyHookProgress("SessionStart","tool",
      false,lookups),ftxui::Color::GrayDark));
  std::cout<<"3 native hook-progress projection checks pass\n";
}
