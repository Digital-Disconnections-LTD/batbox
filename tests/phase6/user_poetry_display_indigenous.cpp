// Long poetry is cut to its head and tail for display; short poetry is shown
// as typed without being converted to measure it.
#include "interactive/cards/UserPoetryLeaf.hpp"
#include <iostream>
#include <stdexcept>

using namespace batbox::interactive::cards;

namespace {
void check(bool condition,const std::string& reason){
  if(!condition)throw std::runtime_error(reason);
}
}

int main(){
  try{
    const std::string shortText="Résumé — 雪 🦇 spans";
    check(poetryDisplayText(shortText)==shortText,"short poetry is unchanged");
    // 10,000 UTF-16 units of two-byte characters: over 10,000 bytes, still shown whole.
    std::string accents;
    for(int i=0;i<10000;++i)accents+="é";
    check(poetryDisplayText(accents)==accents,"10,000 characters fit even as 20,000 bytes");
    std::string longText;
    for(int i=0;i<900;++i)longText+="line "+std::to_string(i)+" of a pasted log\n";
    const auto shown=poetryDisplayText(longText);
    check(shown.size()<longText.size()&&shown.find("… +")!=std::string::npos,
      "long poetry keeps its head and tail");
    std::cout<<"poetry display text cut only when long\n";
  }catch(const std::exception& error){
    std::cerr<<error.what()<<"\n";
    return 1;
  }
}
