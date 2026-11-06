#include "runtime/JsonValue.hpp"
#include "runtime/Decimal.hpp"
#include <cmath>
#include <cstdint>
#include <functional>
#include <iostream>
#include <limits>
#include <random>
#include <string>
#include <vector>

using batbox::Json;
namespace value = batbox::value;

namespace reference {
// The parser as it was before strings were written straight to UTF-8, kept
// verbatim so values and error positions can be compared byte for byte.
struct Parser {
  std::string_view input;
  std::size_t position=0;
  [[noreturn]]void fail()const{throw Json::parse_error::create(101,position+1,"invalid JSON",nullptr);}
  void whitespace(){while(position<input.size()&&(input[position]==' '||input[position]=='\t'||input[position]=='\n'||input[position]=='\r'))++position;}
  bool take(char c){if(position<input.size()&&input[position]==c){++position;return true;}return false;}
  unsigned hex() {
    if(position==input.size())fail();
    const auto c=input[position++];
    if(c>='0'&&c<='9')return c-'0';
    if(c>='a'&&c<='f')return c-'a'+10;
    if(c>='A'&&c<='F')return c-'A'+10;
    fail();
  }
  std::string string() {
    if(!take('"'))fail();
    std::u16string units;
    while(position<input.size()) {
      auto c=static_cast<unsigned char>(input[position++]);
      if(c=='"')return value::fromUtf16(units);
      if(c<0x20)fail();
      if(c=='\\') {
        if(position==input.size())fail();
        const char escape=input[position++];
        switch(escape) {
          case '"':case '\\':case '/':units+=static_cast<char16_t>(escape);break;
          case 'b':units+='\b';break;case 'f':units+='\f';break;case 'n':units+='\n';break;
          case 'r':units+='\r';break;case 't':units+='\t';break;
          case 'u':{unsigned v=0;for(int i=0;i<4;++i)v=(v<<4)|hex();units+=static_cast<char16_t>(v);break;}
          default:fail();
        }
      } else if(c<0x80)units+=static_cast<char16_t>(c);
      else {
        const auto start=position-1;
        const auto width=(c&0xe0)==0xc0?2:(c&0xf0)==0xe0?3:(c&0xf8)==0xf0?4:0;
        if(!width||start+width>input.size())fail();
        try{units+=value::utf16CodeUnits(input.substr(start,width));}catch(const std::invalid_argument&){fail();}
        position=start+width;
      }
    }
    fail();
  }
  Json number() {
    const auto begin=position;
    take('-');
    if(!take('0')) {
      if(position==input.size()||input[position]<'1'||input[position]>'9')fail();
      while(position<input.size()&&input[position]>='0'&&input[position]<='9')++position;
    }
    if(take('.')) {
      const auto digits=position;
      while(position<input.size()&&input[position]>='0'&&input[position]<='9')++position;
      if(position==digits)fail();
    }
    if(take('e')||take('E')) {
      if(!take('+'))take('-');
      const auto digits=position;
      while(position<input.size()&&input[position]>='0'&&input[position]<='9')++position;
      if(position==digits)fail();
    }
    const double v=batbox::value::detail::decimalNumber(input.substr(begin,position-begin));
    if(std::isfinite(v)&&!(v==0&&std::signbit(v))&&std::trunc(v)==v&&v>=-9223372036854775808.0&&v<9223372036854775808.0)
      return static_cast<std::int64_t>(v);
    return v;
  }
  Json value() {
    whitespace();
    if(position==input.size())fail();
    const char c=input[position];
    if(c=='"')return string();
    if(take('{')) {
      Json result=Json::object();whitespace();if(take('}'))return result;
      do{whitespace();auto key=string();whitespace();if(!take(':'))fail();result[key]=value();whitespace();if(take('}'))return result;}while(take(','));
      fail();
    }
    if(take('[')) {
      Json result=Json::array();whitespace();if(take(']'))return result;
      do{result.push_back(value());whitespace();if(take(']'))return result;}while(take(','));
      fail();
    }
    for(const auto& literal:{std::pair{"true",Json(true)},std::pair{"false",Json(false)},std::pair{"null",Json()}}) {
      const std::string_view word=literal.first;
      if(input.substr(position).starts_with(word)){position+=word.size();return literal.second;}
    }
    return number();
  }
};
Json parse(std::string_view text) {
  Parser parser{text};auto result=parser.value();parser.whitespace();if(parser.position!=text.size())parser.fail();return result;
}
} // namespace reference

namespace {
int failures=0;
// Type-tagged and byte-exact, so integer/float and WTF-8 differences show.
void describe(const Json& v,std::string& out) {
  static constexpr char digits[]="0123456789abcdef";
  switch(v.type()) {
    case Json::value_t::object:
      out+='{';for(auto it=v.begin();it!=v.end();++it){describe(Json(it.key()),out);out+=':';describe(it.value(),out);out+=',';}out+='}';break;
    case Json::value_t::array:out+='[';for(const auto& item:v){describe(item,out);out+=',';}out+=']';break;
    case Json::value_t::string:
      out+="s";for(unsigned char c:v.get_ref<const std::string&>()){out+=digits[c>>4];out+=digits[c&15];}break;
    case Json::value_t::number_integer:out+="i"+std::to_string(v.get<std::int64_t>());break;
    case Json::value_t::number_unsigned:out+="u"+std::to_string(v.get<std::uint64_t>());break;
    case Json::value_t::number_float:out+="f"+v.dump();break;
    default:out+=v.dump();break;
  }
}
std::string outcome(const std::function<Json()>& run) {
  Json parsed;
  try{parsed=run();}
  catch(const Json::exception& error){return std::string("error:")+error.what();}
  std::string out="value:";describe(parsed,out);return out;
}
void check(const std::string& text) {
  const auto expected=outcome([&]{return reference::parse(text);});
  const auto actual=outcome([&]{return value::parse(text);});
  if(expected!=actual&&++failures<=20)
    std::cerr<<"FAIL for "<<Json(text).dump(-1,' ',true,Json::error_handler_t::replace)<<"\n  expected "<<expected<<"\n  actual   "<<actual<<'\n';
  const auto quiet=value::parse(text,false);
  if(expected.starts_with("error:")!=quiet.is_discarded()&&++failures<=20)std::cerr<<"FAIL quiet mode\n";
}
std::string wtf8(std::uint32_t cp) {
  std::string out;
  if(cp<0x80)out+=static_cast<char>(cp);
  else if(cp<0x800){out+=static_cast<char>(0xc0|(cp>>6));out+=static_cast<char>(0x80|(cp&63));}
  else if(cp<0x10000){out+=static_cast<char>(0xe0|(cp>>12));out+=static_cast<char>(0x80|((cp>>6)&63));out+=static_cast<char>(0x80|(cp&63));}
  else{out+=static_cast<char>(0xf0|(cp>>18));out+=static_cast<char>(0x80|((cp>>12)&63));out+=static_cast<char>(0x80|((cp>>6)&63));out+=static_cast<char>(0x80|(cp&63));}
  return out;
}
} // namespace

int main() {
  const std::vector<std::string> pieces={
    "a","plain text ","0123456789",R"(\")",R"(\\)",R"(\/)",R"(\b)",R"(\f)",R"(\n)",R"(\r)",R"(\t)",
    R"(\u0000)",R"(\u001f)",R"(A)",R"(é)",R"(界)",R"(\ud83d)",R"(\ude00)",R"(😀)",
    R"(😀)",R"(􏿿)","é","界","😀","👩‍💻",wtf8(0xd83d),wtf8(0xde00),wtf8(0xd800)+wtf8(0xdc00),
    "\x7f",std::string("\x01"),"\x80","\xc3","\xe2\x82","\xc0\x80","\xf4\x90\x80\x80","\xf8\x80",R"(\x)",R"(\u12)",R"(\uzzzz)",
    "\t","\n","\"",
  };
  std::vector<std::string> corpus={
    R"("")",R"({})",R"([])",R"({"a":1,"a":2})",R"({"b":{"c":[1,2,{"d":"e"}]}})",R"(  [ true , false , null ]  )",
    R"("unterminated)",R"("x" trailing)",R"({"k" "v"})",R"([1,])",R"(-0)",R"(1.5e3)",R"(9223372036854775808)",R"(01)",
    R"({"type":"content_block_delta","index":0,"delta":{"type":"text_delta","text":"Hello, world"}})",
  };
  std::mt19937 random(680);
  for(int i=0;i<20000;++i) {
    std::string body;
    for(auto n=random()%12;n>0;--n)body+=pieces[random()%pieces.size()];
    corpus.push_back("\""+body+"\"");
    corpus.push_back("{\""+body+"\":[\""+body+"\",1]}");
  }
  for(int i=0;i<2000;++i) {
    std::string body;
    for(auto n=random()%400;n>0;--n)body+=pieces[random()%3];
    corpus.push_back("\""+body+"\"");
  }
  for(const auto& text:corpus)check(text);
  if(failures){std::cerr<<failures<<" mismatches\n";return 1;}
  std::cout<<"json parse identity: "<<corpus.size()<<" documents\n";
  return 0;
}
