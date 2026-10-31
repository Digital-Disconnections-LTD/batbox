#include "interactive/cards_gadget_children/GadgetRejectProjection.hpp"
#include "ftxui/dom/node.hpp"
#include "ftxui/screen/screen.hpp"
#include <cassert>
#include <iostream>

using namespace batbox::interactive::cards_gadget_children;

namespace {
ftxui::Screen draw(const ftxui::Element& element){
  auto screen=ftxui::Screen::Create(ftxui::Dimension::Fixed(40),
                                    ftxui::Dimension::Fixed(1));
  ftxui::Render(screen,element);
  return screen;
}
}

int main(){
  const auto fallback=projectGadgetReject(GadgetRejectCard{},
      ftxui::Color::GrayDark,{});
  assert(draw(fallback).CellAt(5,0).character=="I");
  GadgetRejectCard custom{false,"APPROVED"};
  const auto rendered=projectGadgetReject(custom,ftxui::Color::GrayDark,
      [](const batbox::Json& value){
        assert(value=="APPROVED");
        return ftxui::text("CUSTOM");
      });
  assert(draw(rendered).CellAt(0,0).character=="C");
  std::cout<<"2 native tool-reject projection cases pass\n";
}
