#include "cleanup/Terminal.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <vector>

using namespace batbox;
namespace {
std::string hex(const std::string& text){
  static const char digits[]="0123456789abcdef";std::string out;
  for(unsigned char byte:text){out+=digits[byte>>4];out+=digits[byte&15];}
  return out;
}
}
int main(){
  Json out=Json::array();
  for(const auto& [name,kitty]:std::vector<std::pair<std::string,bool>>{{"default",false},{"kitty",true}}){
    const auto seq=cleanup::indigenousTerminalSequences(kitty);
    Json fields=Json::array();
    for(const auto& text:{seq.disableMouseTracking,seq.exitAltScreen,
      seq.disableModifyOtherKeys,seq.disableKittyKeyboard,seq.disableFocusEvents,
      seq.disableBracketedPaste,seq.showCursor,seq.clearProgress,seq.clearTabStatus,
      seq.clearTitle})fields.push_back(hex(text));
    Json wraps=Json::array();
    for(const auto& pair:std::vector<std::pair<bool,bool>>{{false,false},{true,true},{false,true}})
      wraps.push_back(hex(cleanup::indigenousWrapForMultiplexer(seq.clearTabStatus,pair.first,pair.second)));
    out.push_back({{"terminal",name},{"fields",fields},{"wrap",wraps}});
  }
  std::cout<<value::stringify(out)<<'\n';
}
