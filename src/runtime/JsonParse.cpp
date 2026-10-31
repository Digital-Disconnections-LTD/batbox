#include "runtime/JsonValue.hpp"
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <locale.h>

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
  std::string string() {
    if(!take('"'))fail();
    std::u16string units;
    while(position<input.size()) {
      auto c=static_cast<unsigned char>(input[position++]);
      if(c=='"')return fromUtf16(units);
      if(c<0x20)fail();
      if(c=='\\') {
        if(position==input.size())fail();
        const char escape=input[position++];
        switch(escape) {
          case '"':case '\\':case '/':units+=static_cast<char16_t>(escape);break;
          case 'b':units+='\b';break;case 'f':units+='\f';break;case 'n':units+='\n';break;
          case 'r':units+='\r';break;case 't':units+='\t';break;
          case 'u':{unsigned value=0;for(int i=0;i<4;++i)value=(value<<4)|hex();units+=static_cast<char16_t>(value);break;}
          default:fail();
        }
      } else if(c<0x80)units+=static_cast<char16_t>(c);
      else {
        const auto start=position-1;
        const auto width=(c&0xe0)==0xc0?2:(c&0xf0)==0xe0?3:(c&0xf8)==0xf0?4:0;
        if(!width||start+width>input.size())fail();
        try{units+=utf16CodeUnits(input.substr(start,width));}catch(const std::invalid_argument&){fail();}
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
#if defined(_WIN32)
    static _locale_t numericLocale=::_create_locale(LC_NUMERIC,"C");
#else
    static locale_t numericLocale=::newlocale(LC_NUMERIC_MASK,"C",nullptr);
#endif
    if(!numericLocale)throw std::bad_alloc();
    const auto text=std::string(input.substr(begin,position-begin));
#if defined(_WIN32)
    const double value=::_strtod_l(text.c_str(),nullptr,numericLocale);
#else
    const double value=::strtod_l(text.c_str(),nullptr,numericLocale);
#endif
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
