#include "interactive/cards_gadget_result/GadgetErrorProjection.hpp"
#include "ftxui/dom/node.hpp"
#include "ftxui/screen/screen.hpp"
#include <cassert>
#include <iostream>

using namespace batbox::interactive::cards_gadget_result;

namespace {
ftxui::Screen draw(const ftxui::Element& element) {
  auto screen=ftxui::Screen::Create(ftxui::Dimension::Fixed(50),
                                    ftxui::Dimension::Fixed(3));
  ftxui::Render(screen,element);
  return screen;
}
}

int main() {
  const auto subtle=ftxui::Color::GrayDark;
  auto screen=draw(projectCanceledGadgetResult(subtle));
  assert(screen.CellAt(2,0).character=="⎿");
  assert(screen.CellAt(5,0).character=="I");
  const auto rejection=projectSimpleGadgetError(
      {GadgetErrorKind::RejectedWithReason,{}},subtle);
  assert(rejection);
  screen=draw(*rejection);
  assert(screen.CellAt(5,0).character=="G");
  assert(!projectSimpleGadgetError(
      {GadgetErrorKind::GadgetRenderer,{}},subtle));
  const GadgetErrorPalette palette{{subtle,ftxui::Color::White,
      ftxui::Color::RedLight,ftxui::Color::YellowLight},subtle};
  screen=draw(projectFallbackError({"oops",1,"ctrl+o",true},50,palette));
  assert(screen.CellAt(2,0).character=="⎿");
  assert(screen.CellAt(5,0).character=="o");
  assert(screen.CellAt(5,1).character=="…");
  assert(screen.CellAt(16,1).character=="c");
  std::cout<<"4 native tool-error projection checks pass\n";
}
