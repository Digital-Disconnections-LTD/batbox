#include "text_width/AnsiSlice.hpp"
namespace batbox::text_width {
bool isEndCode(const ansi_tokenize::AnsiCode& code){return code.code==code.endCode;}
ansi_tokenize::Codes filterStartCodes(const ansi_tokenize::Codes& codes){
  ansi_tokenize::Codes result;
  for(const auto& code:codes)if(!isEndCode(*code))result.push_back(code);
  return result;
}
std::string sliceAnsi(const std::string& text,double start,std::optional<double> end,
    const SliceServices& s){
  const auto tokens=s.tokenize(text);ansi_tokenize::Codes active;
  double position=0;std::string result;bool include=false;
  for(const auto& token:tokens){
    const auto width=token.type=="ansi"?0:token.fullWidth?2:s.stringWidth(token.value);
    if(end&&position>=*end){if(token.type=="ansi"||width>0||!include)break;}
    if(token.type=="ansi"){
      active.push_back(token.ansi);
      if(include)result+=token.ansi->code;
    }else{
      if(!include&&position>=start){
        if(start>0&&width==0)continue;
        include=true;active=filterStartCodes(s.reduceAnsiCodes(active));
        result=s.ansiCodesToString(active);
      }
      // The selected SDK also emits valueless control tokens. Preserve
      // string concatenation of undefined when its non-ANSI branch includes one.
      if(include)result+=token.value.value_or("undefined");
      position+=width;
    }
  }
  const auto starts=filterStartCodes(s.reduceAnsiCodes(active));
  result+=s.ansiCodesToString(s.undoAnsiCodes(starts));return result;
}
void bindAnsiSlice(WrapServices& wrap,SliceServices services){
  wrap.sliceAnsi=[services=std::move(services)](const std::string& text,double start,double end){
    return sliceAnsi(text,start,end,services);
  };
}
} // namespace batbox::text_width
