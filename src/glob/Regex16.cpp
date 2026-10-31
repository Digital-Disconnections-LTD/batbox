#define PCRE2_CODE_UNIT_WIDTH 16
#include <pcre2.h>
#include "glob/Regex16.hpp"
#include "runtime/JsonValue.hpp"
#include <stdexcept>

namespace batbox::glob {
namespace {
std::u16string jsCharacterClasses(std::u16string_view source){
  // JavaScript permits the empty class [] (match nothing) and [^] (match
  // any UTF-16 code unit). PCRE2 rejects them without these equivalents.
  std::u16string output;bool inClass=false;
  for(std::size_t i=0;i<source.size();i++){
    if(source[i]==u'\\'&&i+1<source.size()){
      output+=source[i];output+=source[++i];continue;
    }
    if(!inClass&&source[i]==u'['){
      if(i+1<source.size()&&source[i+1]==u']'){
        output+=u"(?!)";i++;continue;
      }
      if(i+2<source.size()&&source[i+1]==u'^'&&source[i+2]==u']'){
        output+=u"[\\s\\S]";i+=2;continue;
      }
      inClass=true;
    }else if(inClass&&source[i]==u']')inClass=false;
    output+=source[i];
  }
  return output;
}
}
struct Regex16::Code {
  pcre2_code* compiled=nullptr;
  ~Code(){if(compiled)pcre2_code_free(compiled);}
};
std::u16string units(std::string_view value){return value::utf16CodeUnits(value);}
std::string text(std::u16string_view value){return value::fromUtf16(value);}
Regex16::Regex16(std::u16string source,bool ignoreCase):code_(std::make_unique<Code>()){
  source=jsCharacterClasses(source);
  int error=0;PCRE2_SIZE offset=0;
  // JavaScript's non-multiline dollar assertion requires the actual end of
  // the string. PCRE's default also accepts a final newline.
  const auto flags=PCRE2_DOLLAR_ENDONLY|(ignoreCase?(PCRE2_CASELESS|PCRE2_UCP):0);
  code_->compiled=pcre2_compile(reinterpret_cast<PCRE2_SPTR>(source.data()),
    source.size(),flags,&error,&offset,nullptr);
  if(code_->compiled)return;
  PCRE2_UCHAR buffer[256]={};
  pcre2_get_error_message(error,buffer,256);
  throw std::invalid_argument("Regex compile at "+std::to_string(offset)+": "+
    text(std::u16string_view(reinterpret_cast<const char16_t*>(buffer))));
}
Regex16::~Regex16()=default;
Regex16::Regex16(Regex16&&)noexcept=default;
Regex16& Regex16::operator=(Regex16&&)noexcept=default;
bool Regex16::search(std::u16string_view input,std::size_t offset,RegexMatch* result)const{
  if(offset>input.size())return false;
  auto* data=pcre2_match_data_create_from_pattern(code_->compiled,nullptr);
  if(!data)throw std::bad_alloc();
  const int rc=pcre2_match(code_->compiled,
    reinterpret_cast<PCRE2_SPTR>(input.data()),input.size(),offset,0,data,nullptr);
  if(rc==PCRE2_ERROR_NOMATCH){pcre2_match_data_free(data);return false;}
  if(rc<0){pcre2_match_data_free(data);
    throw std::runtime_error("Regex match error "+std::to_string(rc));}
  if(result){
    auto* positions=pcre2_get_ovector_pointer(data);
    result->start=positions[0];result->end=positions[1];
    result->groups.clear();result->groups.reserve(static_cast<std::size_t>(rc));
    for(int i=0;i<rc;i++){
      const auto begin=positions[2*i],end=positions[2*i+1];
      if(begin==PCRE2_UNSET)result->groups.push_back(std::nullopt);
      else result->groups.emplace_back(input.substr(begin,end-begin));
    }
  }
  pcre2_match_data_free(data);return true;
}
std::u16string Regex16::replace(std::u16string_view input,
  const std::function<std::u16string(const RegexMatch&)>& callback,bool global)const{
  std::u16string output;
  std::size_t consumed=0,from=0;
  RegexMatch match;
  while(search(input,from,&match)){
    output.append(input.substr(consumed,match.start-consumed));
    output+=callback(match);
    consumed=match.end;
    if(!global)break;
    from=match.end>match.start?match.end:match.end+1;
    if(from>input.size())break;
  }
  output.append(input.substr(consumed));
  return output;
}
} // namespace batbox::glob
