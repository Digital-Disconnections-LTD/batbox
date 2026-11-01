// Short non-ASCII labels keep their measured width, and the kept widths
// always match a fresh measurement, including after the store fills up.
#include "terminal_ftxui/TextFit.hpp"
#include "terminal_ftxui/IndigenousStringMetrics.hpp"
#include "text_width/StringWidth.hpp"
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace batbox;

namespace {
void check(bool condition,const std::string& reason){
  if(!condition)throw std::runtime_error(reason);
}
}

int main(){
  try{
    auto intl=std::make_shared<intl::Stash>(intl::indigenousServices());
    auto width=std::make_shared<text_width::StringWidth>(text_width::indigenousServices(intl));
    terminal_ftxui::installIndigenousStringMetrics(intl,width);
    const std::vector<std::pair<std::string,double>> labels={
      {"◆ plan",6},{"⎇ night-shift",13},{"…/BATBOX/batbox",15},{"✓ Read",6},
      {"界",2},{"👍🏽",2},{"naïve café",10},{"\x1b[31m◆\x1b[0m",1}};
    for(int round=0;round<3;++round)
      for(const auto& [text,expected]:labels){
        check(width->stringWidthJavaScript(Json(text))==expected,
          "Width of '"+text+"' changed on round "+std::to_string(round));
        check(terminal_ftxui::displayWidth(text)==static_cast<int>(expected),
          "Terminal width of '"+text+"' is wrong");
      }
    // Fill past the store's capacity; every answer still matches a fresh measure.
    text_width::StringWidth fresh(text_width::indigenousServices(intl));
    for(int i=0;i<5000;++i){
      const auto text="◆ "+std::to_string(i)+(i%3==0?" 界":"");
      const auto kept=width->stringWidthJavaScript(Json(text));
      check(kept==fresh.stringWidthJavaScript(Json(text)),"Kept width differs for "+text);
      check(width->stringWidthJavaScript(Json(text))==kept,"Repeated width differs for "+text);
    }
    std::string longText;
    for(int i=0;i<40;++i)longText+="界";
    check(width->stringWidthJavaScript(Json(longText))==80,"Long text is measured in full");
    for(const auto& [text,expected]:labels)
      check(width->stringWidthJavaScript(Json(text))==expected,"Width of '"+text+"' after refill");
    std::cout<<"Short labels keep exact widths across frames and refills\n";
    return 0;
  }catch(const std::exception& error){
    std::cerr<<error.what()<<"\n";
    return 1;
  }
}
