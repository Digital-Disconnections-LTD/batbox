#include "gadgets/bash/Hints.hpp"
#include "runtime/JsonValue.hpp"
#include <cassert>
#include <clocale>
#include <cmath>
#include <iostream>
#include <string>

namespace {
void checkNumbers() {
  using batbox::value::parseInt10;
  assert(parseInt10(" \t+00123suffix")==123);
  assert(parseInt10("1e3")==1);
  assert(parseInt10("0x10")==0);
  assert(parseInt10("9007199254740993")==9007199254740992.0);
  assert(parseInt10("9007199254740995")==9007199254740996.0);
  const double negativeZero=parseInt10("-000tail");
  assert(negativeZero==0&&std::signbit(negativeZero));
  assert(std::isnan(parseInt10("+oops")));
  assert(std::isinf(parseInt10(std::string(400,'9'))));
  assert(parseInt10("-"+std::string(400,'9'))<0);
  assert(parseInt10(std::string(400,'0')+"1")==1);
  assert(batbox::value::parse("1.25").get<double>()==1.25);

  const batbox::bash_gadget::HintProtocol protocol{"batbox-hint",[](const std::string&){}};
  const auto accepted=[&](const std::string& version) {
    return batbox::bash_gadget::extractBatboxHints(
      "<batbox-hint v="+version+" type=plugin value=example@plugin />","test",protocol)
        .at("hints").size()==1;
  };
  for(const auto* version:{"1","01","+1","1.0","1.",".1e1","1e0","0x1","0b1","0o1"})
    assert(accepted(version));
  for(const auto* version:{"2","1.5","1,5","1e309","1e-9999","1x","NaN","Infinity","0b012"})
    assert(!accepted(version));
}
}

int main() {
  checkNumbers();
  const std::string previous=std::setlocale(LC_NUMERIC,nullptr);
  bool checkedCommaLocale=false;
  for(const auto* name:{"fr_FR.UTF-8","fr_FR","de_DE.UTF-8","de_DE","French_France.1252"}) {
    if(std::setlocale(LC_NUMERIC,name)&&std::string(std::localeconv()->decimal_point)==",") {
      checkNumbers();
      checkedCommaLocale=true;
      break;
    }
  }
  std::setlocale(LC_NUMERIC,previous.c_str());
  std::cout<<"Number parsing passed; comma locale "<<(checkedCommaLocale?"checked":"unavailable")<<'\n';
}
