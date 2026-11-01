#include <ftxui/dom/elements.hpp>
#include <ftxui/dom/selection.hpp>
#include <ftxui/screen/screen.hpp>
#include <iostream>
#include <stdexcept>

int main(){
  auto paragraph=ftxui::paragraph("hello  world again");
  auto screen=ftxui::Screen::Create(ftxui::Dimension::Fixed(30),
    ftxui::Dimension::Fixed(1));
  ftxui::Selection selection(0,0,19,0);
  ftxui::Render(screen,paragraph.get(),selection);
  if(selection.GetParts()!="hello  world again")
    throw std::runtime_error("Paragraph selection lost visible spaces: "+
      selection.GetParts());
  std::cout<<"Paragraph selection keeps visible spaces\n";
}
