#include "interactive/markdown/AnsiProjection.hpp"
#include "runtime/Encoding.hpp"
#include <algorithm>
#include <optional>
#include <sstream>
#include <utility>

namespace batbox::interactive::markdown {
namespace {
struct Style {
  bool bold=false,italic=false,underline=false,dim=false;
  std::optional<ftxui::Color> foreground;
  std::string link;
};
ftxui::Element run(const std::string& text,const Style& style){
  auto element=ftxui::paragraph(value::encodeUtf8(text));
  if(style.bold)element=element|ftxui::bold;
  if(style.italic)element=element|ftxui::italic;
  if(style.underline)element=element|ftxui::underlined;
  if(style.dim)element=element|ftxui::dim;
  if(style.foreground)element=element|ftxui::color(*style.foreground);
  if(!style.link.empty())element=element|ftxui::hyperlink(style.link);
  return element;
}
void applySgr(Style& style,const std::string& raw){
  std::vector<int> values;
  std::stringstream stream(raw);
  std::string value;
  while(std::getline(stream,value,';')){
    try{values.push_back(value.empty()?0:std::stoi(value));}
    catch(...){values.push_back(0);}
  }
  if(values.empty())values.push_back(0);
  for(std::size_t i=0;i<values.size();++i){
    const auto code=values[i];
    if(code==0){
      auto link=std::move(style.link);
      style=Style{};
      style.link=std::move(link);
    }
    else if(code==1)style.bold=true;
    else if(code==2)style.dim=true;
    else if(code==3)style.italic=true;
    else if(code==4)style.underline=true;
    else if(code==22){style.bold=false;style.dim=false;}
    else if(code==23)style.italic=false;
    else if(code==24)style.underline=false;
    else if(code==39)style.foreground.reset();
    else if(code>=30&&code<=37)
      style.foreground=ftxui::Color(static_cast<ftxui::Color::Palette16>(code-30));
    else if(code>=90&&code<=97)
      style.foreground=ftxui::Color(static_cast<ftxui::Color::Palette16>(code-90+8));
    else if(code==38&&i+2<values.size()&&values[i+1]==5){
      style.foreground=ftxui::Color(static_cast<ftxui::Color::Palette256>(
        std::clamp(values[i+2],0,255)));
      i+=2;
    }else if(code==38&&i+4<values.size()&&values[i+1]==2){
      style.foreground=ftxui::Color::RGB(
        std::clamp(values[i+2],0,255),std::clamp(values[i+3],0,255),
        std::clamp(values[i+4],0,255));
      i+=4;
    }
  }
}
} // namespace

ftxui::Element projectAnsi(const std::string& source,bool dim){
  Style style;
  std::vector<ftxui::Element> lines,parts;
  std::string runText;
  const auto flush=[&](){
    if(!runText.empty()){
      parts.push_back(run(runText,style));runText.clear();
    }
  };
  const auto newLine=[&](){
    flush();
    lines.push_back(parts.empty()?ftxui::text(""):ftxui::hflow(std::move(parts)));
    parts.clear();
  };
  for(std::size_t i=0;i<source.size();){
    if(source[i]=='\n'){newLine();++i;continue;}
    if(source[i]!='\x1b'){runText+=source[i++];continue;}
    if(i+1<source.size()&&source[i+1]=='['){
      const auto end=source.find('m',i+2);
      if(end==std::string::npos){runText+=source[i++];continue;}
      flush();applySgr(style,source.substr(i+2,end-(i+2)));i=end+1;
      continue;
    }
    if(i+1<source.size()&&source[i+1]==']'){
      const auto end=source.find('\x07',i+2);
      if(end==std::string::npos){runText+=source[i++];continue;}
      const auto command=source.substr(i+2,end-(i+2));
      if(command.starts_with("8;;")){
        flush();style.link=command.substr(3);
      }
      i=end+1;continue;
    }
    runText+=source[i++];
  }
  newLine();
  auto element=ftxui::vbox(std::move(lines));
  return dim?element|ftxui::dim:element;
}
} // namespace batbox::interactive::markdown
