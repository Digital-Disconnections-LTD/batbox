#include "runtime/JsonValue.hpp"
#include "runtime/Decimal.hpp"
#include <cmath>
#include <cstdint>
#include <limits>

namespace batbox::value {
namespace {
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
  // Writes UTF-16 code units as fromUtf16 would: a high surrogate waits for
  // the next unit and pairs with a low one, otherwise it is written alone.
  struct Units {
    std::string out;
    char16_t high=0;
    void encode(std::uint32_t cp) {
      if(cp<0x80)out+=static_cast<char>(cp);
      else if(cp<0x800){out+=static_cast<char>(0xc0|(cp>>6));out+=static_cast<char>(0x80|(cp&63));}
      else if(cp<0x10000){out+=static_cast<char>(0xe0|(cp>>12));out+=static_cast<char>(0x80|((cp>>6)&63));out+=static_cast<char>(0x80|(cp&63));}
      else{out+=static_cast<char>(0xf0|(cp>>18));out+=static_cast<char>(0x80|((cp>>12)&63));out+=static_cast<char>(0x80|((cp>>6)&63));out+=static_cast<char>(0x80|(cp&63));}
    }
    void settle(){if(high){encode(high);high=0;}}
    void unit(char16_t value) {
      if(high&&value>=0xdc00&&value<=0xdfff){encode(0x10000+((high-0xd800)<<10)+(value-0xdc00));high=0;return;}
      settle();
      if(value>=0xd800&&value<=0xdbff)high=value;else encode(value);
    }
  };
  std::string string() {
    if(!take('"'))fail();
    Units units;
    while(position<input.size()) {
      const auto run=position;
      while(position<input.size()) {
        const auto b=static_cast<unsigned char>(input[position]);
        if(b<0x20||b>=0x80||b=='"'||b=='\\')break;
        ++position;
      }
      if(position>run){units.settle();units.out.append(input.data()+run,position-run);}
      if(position==input.size())break;
      auto c=static_cast<unsigned char>(input[position++]);
      if(c=='"'){units.settle();return std::move(units.out);}
      if(c<0x20)fail();
      if(c=='\\') {
        if(position==input.size())fail();
        const char escape=input[position++];
        switch(escape) {
          case '"':case '\\':case '/':units.unit(static_cast<char16_t>(escape));break;
          case 'b':units.unit('\b');break;case 'f':units.unit('\f');break;case 'n':units.unit('\n');break;
          case 'r':units.unit('\r');break;case 't':units.unit('\t');break;
          case 'u':{unsigned value=0;for(int i=0;i<4;++i)value=(value<<4)|hex();units.unit(static_cast<char16_t>(value));break;}
          default:fail();
        }
      } else {
        const auto start=position-1;
        const auto width=(c&0xe0)==0xc0?2:(c&0xf0)==0xe0?3:(c&0xf8)==0xf0?4:0;
        if(!width||start+width>input.size())fail();
        std::u16string decoded;
        try{decoded=utf16CodeUnits(input.substr(start,width));}catch(const std::invalid_argument&){fail();}
        for(const auto unit:decoded)units.unit(unit);
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
    const double value=detail::decimalNumber(input.substr(begin,position-begin));
    if(std::isfinite(value)&&!(value==0&&std::signbit(value))&&std::trunc(value)==value&&value>=-9223372036854775808.0&&value<9223372036854775808.0)
      return static_cast<std::int64_t>(value);
    return value;
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
} // namespace
Json parse(std::string_view text,bool allowExceptions) {
  try{Parser parser{text};auto value=parser.value();parser.whitespace();if(parser.position!=text.size())parser.fail();return value;}
  catch(const Json::parse_error&){if(allowExceptions)throw;return Json(Json::value_t::discarded);}
}
} // namespace batbox::value
