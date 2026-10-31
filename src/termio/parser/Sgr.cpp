#include "termio/parser/Parser.hpp"
#include <array>

namespace batbox::termio::parser {
namespace {
struct Param {std::optional<double> value;std::vector<double> subparams;bool colon=false;};
std::vector<Param> parseParams(const std::string& text){
  if(text.empty())return {Param{0,{},false}};
  std::vector<Param> result;Param current;std::string num;bool inSub=false;
  for(std::size_t i=0;i<=text.size();++i){
    const auto c=i<text.size()?text[i]:'\0';
    if(c==';'||i==text.size()){
      const auto n=num.empty()?std::nullopt:std::optional<double>(value::parseInt10(num));
      if(inSub){if(n)current.subparams.push_back(*n);}
      else current.value=n;
      result.push_back(current);current={};num.clear();inSub=false;
    }else if(c==':'){
      const auto n=num.empty()?std::nullopt:std::optional<double>(value::parseInt10(num));
      if(!inSub){current.value=n;current.colon=true;inSub=true;}
      else if(n)current.subparams.push_back(*n);
      num.clear();
    }else if(c>='0'&&c<='9')num+=c;
  }
  return result;
}
std::optional<Color> extendedColor(const std::vector<Param>& params,std::size_t index){
  if(index>=params.size())return std::nullopt;
  const auto& p=params[index];
  if(p.colon&&!p.subparams.empty()){
    if(p.subparams[0]==5&&p.subparams.size()>=2){
      Color color;color.type="indexed";color.index=p.subparams[1];return color;
    }
    if(p.subparams[0]==2&&p.subparams.size()>=4){
      const auto off=p.subparams.size()>=5?1:0;
      Color color;color.type="rgb";
      color.r=p.subparams[1+off];color.g=p.subparams[2+off];color.b=p.subparams[3+off];
      return color;
    }
  }
  if(index+1>=params.size())return std::nullopt;
  const auto& next=params[index+1];
  if(next.value==5&&index+2<params.size()&&params[index+2].value){
    Color color;color.type="indexed";color.index=*params[index+2].value;return color;
  }
  if(next.value==2&&index+4<params.size()&&params[index+2].value&&
     params[index+3].value&&params[index+4].value){
    Color color;color.type="rgb";
    color.r=*params[index+2].value;color.g=*params[index+3].value;color.b=*params[index+4].value;
    return color;
  }
  return std::nullopt;
}
Color namedColor(std::size_t index){
  static constexpr std::array<const char*,16> names={
    "black","red","green","yellow","blue","magenta","cyan","white",
    "brightBlack","brightRed","brightGreen","brightYellow","brightBlue",
    "brightMagenta","brightCyan","brightWhite"};
  Color result;result.type="named";result.name=names[index];return result;
}
std::string underlineStyle(double index){
  static constexpr std::array<const char*,6> names={
    "none","single","double","curly","dotted","dashed"};
  if(index<0||index>=names.size()||index!=static_cast<int>(index))return "single";
  return names[static_cast<std::size_t>(index)];
}
}
TextStyle applySGR(const std::string& paramStr,const TextStyle& style){
  const auto params=parseParams(paramStr);auto s=style;std::size_t i=0;
  while(i<params.size()){
    const auto& p=params[i];const double code=p.value.value_or(0);
    if(code==0){s=defaultStyle();++i;continue;}
    if(code==1){s.bold=true;++i;continue;}
    if(code==2){s.dim=true;++i;continue;}
    if(code==3){s.italic=true;++i;continue;}
    if(code==4){s.underline=p.colon?underlineStyle(p.subparams.empty()?-1:p.subparams[0]):"single";++i;continue;}
    if(code==5||code==6){s.blink=true;++i;continue;}
    if(code==7){s.inverse=true;++i;continue;}
    if(code==8){s.hidden=true;++i;continue;}
    if(code==9){s.strikethrough=true;++i;continue;}
    if(code==21){s.underline="double";++i;continue;}
    if(code==22){s.bold=false;s.dim=false;++i;continue;}
    if(code==23){s.italic=false;++i;continue;}
    if(code==24){s.underline="none";++i;continue;}
    if(code==25){s.blink=false;++i;continue;}
    if(code==27){s.inverse=false;++i;continue;}
    if(code==28){s.hidden=false;++i;continue;}
    if(code==29){s.strikethrough=false;++i;continue;}
    if(code==53){s.overline=true;++i;continue;}
    if(code==55){s.overline=false;++i;continue;}

    if(code>=30&&code<=37){s.fg=namedColor(static_cast<std::size_t>(code-30));++i;continue;}
    if(code==39){s.fg={};++i;continue;}
    if(code>=40&&code<=47){s.bg=namedColor(static_cast<std::size_t>(code-40));++i;continue;}
    if(code==49){s.bg={};++i;continue;}
    if(code>=90&&code<=97){s.fg=namedColor(static_cast<std::size_t>(code-90+8));++i;continue;}
    if(code>=100&&code<=107){s.bg=namedColor(static_cast<std::size_t>(code-100+8));++i;continue;}
    if(code==38||code==48||code==58){
      const auto color=extendedColor(params,i);
      if(color){
        if(code==38)s.fg=*color;
        else if(code==48)s.bg=*color;
        else s.underlineColor=*color;
        i+=p.colon?1:color->type=="indexed"?3:5;
        continue;
      }
    }
    if(code==59){s.underlineColor={};++i;continue;}
    ++i;
  }
  return s;
}
}
