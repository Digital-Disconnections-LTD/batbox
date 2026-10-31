#include "ansi_tokenize/AnsiTokenize.hpp"
#include <iostream>
#include <iterator>

using namespace batbox;
using namespace batbox::ansi_tokenize;
namespace {
Codes makeCodes(const Json& raw){
  Codes result;
  for(const auto& item:raw){
    const auto code=item.is_string()?item.get<std::string>():item.at("code").get<std::string>();
    const auto end=item.is_object()&&item.contains("endCode")?
      item.at("endCode").get<std::string>():getEndCode(code);
    result.push_back(std::make_shared<AnsiCode>(AnsiCode{"ansi",code,end}));
  }
  return result;
}
Json renderCodes(const Codes& codes,const Codes& original){
  Json result=Json::array();
  for(const auto& code:codes){
    int identity=-1;
    for(std::size_t i=0;i<original.size();++i)
      if(code==original[i]){identity=static_cast<int>(i);break;}
    result.push_back(Json{{"code",code->code},{"endCode",code->endCode},
      {"identity",identity}});
  }
  return result;
}
Json renderToken(const Token& token){
  if(token.type=="ansi")return Json{{"type","ansi"},{"code",token.ansi->code},
    {"endCode",token.ansi->endCode}};
  if(token.type=="control")return Json{{"type","control"},{"code",token.controlCode}};
  return Json{{"type","char"},{"value",token.value.value_or("")},
    {"fullWidth",token.fullWidth}};
}
}
int main(){
  const auto cases=value::parse(std::string(std::istreambuf_iterator<char>(std::cin),{}));
  Json output=Json::array();
  for(const auto& item:cases){
    try{
      const auto method=item.at("method").get<std::string>();
      if(method=="tokenize"){
        const auto end=value::property(item,"endChar");
        const auto tokens=end.is_number()?tokenizeUntil(item.at("text"),end.get<double>()):
          tokenize(item.at("text"));
        Json rendered=Json::array();for(const auto& token:tokens)rendered.push_back(renderToken(token));
        output.push_back(rendered);
      }else if(method=="getEnd")output.push_back(getEndCode(item.at("code")));
      else if(method=="link")output.push_back(getLinkStartCode(item.at("url"),value::property(item,"params")));
      else if(method=="codesToString")output.push_back(ansiCodesToString(makeCodes(item.at("codes"))));
      else if(method=="styled"){
        const auto chars=styledCharsFromTokens(tokenize(item.at("text")));
        Json rendered=Json::array();
        for(const auto& c:chars){
          Json styles=Json::array();
          for(const auto& style:c.styles)styles.push_back(Json{{"code",style->code},
            {"endCode",style->endCode}});
          rendered.push_back(Json{{"value",c.value},{"fullWidth",c.fullWidth},
            {"styles",styles}});
        }
        output.push_back(Json{{"chars",rendered},{"text",styledCharsToString(chars)}});
      }else if(method=="reduce"||method=="undo"){
        auto codes=makeCodes(item.at("codes"));
        output.push_back(renderCodes(method=="reduce"?reduceAnsiCodes(codes):
          undoAnsiCodes(codes),codes));
      }else if(method=="incremental"||method=="diff"){
        auto first=makeCodes(item.at(method=="diff"?"from":"codes"));
        auto second=makeCodes(item.at(method=="diff"?"to":"next"));
        Codes original=first;original.insert(original.end(),second.begin(),second.end());
        output.push_back(renderCodes(method=="diff"?diffAnsiCodes(first,second):
          reduceAnsiCodesIncremental(first,second),original));
      }
    }catch(const std::exception& e){output.push_back(Json{{"error",e.what()}});}
  }
  std::cout<<value::stringify(output)<<'\n';
}
