#include "keyboard_input/Keyboard.hpp"
#include <iostream>
using namespace batbox;namespace ki=batbox::keyboard_input;
Json run(const Json& item){const auto method=item.value("method","");const auto text=item.value("text","");
 if(method=="single")return ki::toJson(ki::parseKeypress(text));
 if(method=="response"){const auto v=ki::parseTerminalResponse(text);return v?*v:Json(nullptr);}
 if(method=="mouse"){const auto v=ki::parseMouseEvent(text);return v?ki::toJson(ki::ParsedInput(std::make_shared<ki::ParsedMouse>(*v))):Json(nullptr);}
 if(method=="params")return ki::splitNumericParams(text);
 if(method=="mod"){const auto v=ki::decodeModifier(item.at("value"));return Json{{"shift",v.shift},{"meta",v.meta},{"ctrl",v.ctrl},{"super",v.super}};}
 if(method=="code"){const auto v=ki::keycodeToName(item.at("value"));return v?Json(*v):Json(nullptr);}
 if(method=="names")return ki::nonAlphanumericKeys();
 ki::KeyParseState state;Json results=Json::array();
 for(const auto& op:item.at("steps")){try{
   if(op.value("reset",false))state={};
   const auto old=state;ki::ByteBuffer buffer;ki::Input input=Json("");
   if(op.value("flush",false))input=Json(nullptr);
   else if(op.contains("buffer")){buffer=std::make_shared<std::vector<unsigned char>>(op.at("buffer").get<std::vector<unsigned char>>());input=buffer;}
   else input=op.value("value",Json(""));
   auto parsed=ki::parseMultipleKeypresses(state,input);Json keys=Json::array();for(const auto& key:parsed.keys)keys.push_back(ki::toJson(key));
   const auto same=state.tokenizer==parsed.state.tokenizer;state=std::move(parsed.state);
   results.push_back(Json{{"keys",keys},{"mode",state.mode},{"incomplete",state.incomplete},{"pasteBuffer",state.pasteBuffer},{"sameTokenizer",same},{"oldMode",old.mode},{"buffer",buffer?Json(*buffer):Json(nullptr)}});
 }catch(const std::exception& e){results.push_back(Json{{"error",e.what()}});}}
 return results;
}
int main(){const std::string bytes((std::istreambuf_iterator<char>(std::cin)),{});const auto cases=value::parse(bytes);Json results=Json::array();for(const auto& item:cases)results.push_back(run(item));std::cout<<value::stringify(results)<<'\n';}
