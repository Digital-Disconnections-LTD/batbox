#include "termio/parser/AnsiLeaf.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <limits>
#include <string>
using batbox::Json;
namespace ansi=batbox::termio::parser::ansi;
namespace {
Json constants(){
  return Json{{"C0",Json{
    {"NUL",ansi::c0::NUL},{"SOH",ansi::c0::SOH},{"STX",ansi::c0::STX},
    {"ETX",ansi::c0::ETX},{"EOT",ansi::c0::EOT},{"ENQ",ansi::c0::ENQ},
    {"ACK",ansi::c0::ACK},{"BEL",ansi::c0::BEL},{"BS",ansi::c0::BS},
    {"HT",ansi::c0::HT},{"LF",ansi::c0::LF},{"VT",ansi::c0::VT},
    {"FF",ansi::c0::FF},{"CR",ansi::c0::CR},{"SO",ansi::c0::SO},
    {"SI",ansi::c0::SI},{"DLE",ansi::c0::DLE},{"DC1",ansi::c0::DC1},
    {"DC2",ansi::c0::DC2},{"DC3",ansi::c0::DC3},{"DC4",ansi::c0::DC4},
    {"NAK",ansi::c0::NAK},{"SYN",ansi::c0::SYN},{"ETB",ansi::c0::ETB},
    {"CAN",ansi::c0::CAN},{"EM",ansi::c0::EM},{"SUB",ansi::c0::SUB},
    {"ESC",ansi::c0::ESC},{"FS",ansi::c0::FS},{"GS",ansi::c0::GS},
    {"RS",ansi::c0::RS},{"US",ansi::c0::US},{"DEL",ansi::c0::DEL}}},
    {"ESC",std::string(ansi::ESC)},{"BEL",std::string(ansi::BEL)},
    {"SEP",std::string(ansi::SEP)},
    {"ESC_TYPE",Json{{"CSI",ansi::escapeType::CSI},{"OSC",ansi::escapeType::OSC},
      {"DCS",ansi::escapeType::DCS},{"APC",ansi::escapeType::APC},
      {"PM",ansi::escapeType::PM},{"SOS",ansi::escapeType::SOS},
      {"ST",ansi::escapeType::ST}}}};
}
double byteValue(const Json& value){
  if(value.is_number())return value.get<double>();
  if(value.is_null())return 0;
  if(value.is_boolean())return value.get<bool>()?1:0;
  const auto text=value.get<std::string>();
  if(text=="Infinity")return std::numeric_limits<double>::infinity();
  if(text=="-Infinity")return -std::numeric_limits<double>::infinity();
  return std::numeric_limits<double>::quiet_NaN();
}
}
int main(int argc,char** argv){
  if(argc!=2||std::string(argv[1])!="--probe")return 2;
  std::string line;
  while(std::getline(std::cin,line)){
    try{
      const auto value=batbox::value::parse(line);
      const auto result=value.at("kind")=="constants"?Json(constants()):
        Json(ansi::isC0(byteValue(value.at("byte"))));
      std::cout<<batbox::value::stringify(Json{{"result",result}})<<'\n';
    }catch(const std::exception& error){
      std::cout<<batbox::value::stringify(Json{{"error",error.what()}})<<'\n';
    }
  }
}
