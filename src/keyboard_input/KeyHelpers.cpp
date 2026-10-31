#include "keyboard_input/Keyboard.hpp"
#include "runtime/ArrayJoin.hpp"
#include <cmath>
#include <stdexcept>
namespace batbox::keyboard_input {
std::uint32_t toUint32(double value){
  if(!std::isfinite(value)||value==0)return 0;
  auto mod=std::fmod(std::trunc(value),4294967296.0);
  if(mod<0)mod+=4294967296.0;
  return static_cast<std::uint32_t>(mod);
}
Modifiers decodeModifier(double modifier){
  const auto m=toUint32(modifier-1);
  return {bool(m&1),bool(m&2),bool(m&4),bool(m&8)};
}
std::optional<std::string> keycodeToName(double keycode){
  if(keycode==9)return "tab";
  if(keycode==13||keycode==57414)return "return";
  if(keycode==27)return "escape";
  if(keycode==32)return "space";
  if(keycode==127)return "backspace";
  if(keycode>=57399&&keycode<=57408&&std::floor(keycode)==keycode)
    return std::to_string(static_cast<int>(keycode)-57399);
  if(keycode==57409)return ".";
  if(keycode==57410)return "/";
  if(keycode==57411)return "*";
  if(keycode==57412)return "-";
  if(keycode==57413)return "+";
  if(keycode==57415)return "=";
  if(keycode>=32&&keycode<=126){
    auto c=static_cast<unsigned>(std::trunc(keycode));
    if(c>=65&&c<=90)c+=32;
    return std::string(1,static_cast<char>(c));
  }
  return {};
}
std::string inputToString(const Input& input){
  if(const auto buffer=std::get_if<ByteBuffer>(&input)){
    if(!*buffer)throw std::invalid_argument("Input buffer required");
    auto& bytes=**buffer;
    if(bytes.size()==1&&bytes[0]>127){bytes[0]-=128;return "\x1b"+value::decodeUtf8(std::string(bytes.begin(),bytes.end()));}
    return value::decodeUtf8(std::string(bytes.begin(),bytes.end()));
  }
  const auto& value=std::get<Json>(input);
  if(value.is_string())return value.get<std::string>();
  if(value.is_discarded())return "";
  if(value.is_null())return "null";
  if(value.is_boolean())return value.get<bool>()?"true":"false";
  if(value.is_number())return value::numberToString(value.get<double>());
  if(value.is_array())return value::arrayJoin(value);
  if(value.contains("toString"))throw std::invalid_argument("Cannot convert object to primitive value");
  return "[object Object]";
}
ParsedKey createPasteKey(const std::string& content){
  ParsedKey key;key.name="";key.sequence=content;key.raw=content;key.isPasted=true;return key;
}
ParsedKey createNavKey(const std::string& sequence,const std::string& name,bool ctrl){
  ParsedKey key;key.name=name;key.ctrl=ctrl;key.sequence=sequence;key.raw=sequence;return key;
}
Json toJson(const ParsedKey& key){
  Json out={{"kind","key"},{"fn",key.fn},{"ctrl",key.ctrl},{"meta",key.meta},{"shift",key.shift},
    {"option",key.option},{"super",key.super},{"isPasted",key.isPasted}};
  if(key.name)out["name"]=*key.name;
  if(key.sequence)out["sequence"]=*key.sequence;
  if(key.raw)out["raw"]=*key.raw;
  if(key.code)out["code"]=*key.code;
  return out;
}
Json toJson(const ParsedInput& input){
  return std::visit([](const auto& value)->Json{
    using T=std::decay_t<decltype(value)>;
    if constexpr(std::is_same_v<T,KeyRef>)return toJson(*value);
    else if constexpr(std::is_same_v<T,ParsedResponse>)return Json{{"kind","response"},{"sequence",value.sequence},{"response",*value.response}};
    else return Json{{"kind","mouse"},{"button",value->button},{"action",value->action},{"col",value->col},{"row",value->row},{"sequence",value->sequence}};
  },input);
}
}
