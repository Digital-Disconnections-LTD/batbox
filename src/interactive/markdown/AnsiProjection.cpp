#include "interactive/markdown/AnsiProjection.hpp"
#include "runtime/Encoding.hpp"
#include <algorithm>
#include <cstdint>
#include <optional>
#include <sstream>
#include <utility>

namespace batbox::interactive::markdown {
namespace {
using Style=AnsiStyle;
// Words, each carrying its trailing spaces, flow independently so a line
// wraps between any two words, not only where the styling changes.
void words(std::vector<ftxui::Element>& parts,const std::string& text,const Style& style){
  std::size_t start=0;
  while(start<text.size()){
    auto end=text.find(' ',start);
    if(end==start){
      // Leading spaces (indentation) stay together as one piece.
      end=text.find_first_not_of(' ',start);
    }else if(end!=std::string::npos){
      end=text.find_first_not_of(' ',end);
    }
    if(end==std::string::npos)end=text.size();
    parts.push_back(styleAnsiRun(ftxui::text(value::encodeUtf8(text.substr(start,end-start))),
      style));
    start=end;
  }
}
std::optional<ftxui::Color> extended(const std::vector<int>& values,std::size_t& i){
  if(i+2<values.size()&&values[i+1]==5){
    const auto color=ftxui::Color(static_cast<ftxui::Color::Palette256>(
      std::clamp(values[i+2],0,255)));
    i+=2;return color;
  }
  if(i+4<values.size()&&values[i+1]==2){
    const auto color=ftxui::Color::RGB(
      static_cast<std::uint8_t>(std::clamp(values[i+2],0,255)),
      static_cast<std::uint8_t>(std::clamp(values[i+3],0,255)),
      static_cast<std::uint8_t>(std::clamp(values[i+4],0,255)));
    i+=4;return color;
  }
  return {};
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
    else if(code==9)style.strike=true;
    else if(code==22){style.bold=false;style.dim=false;}
    else if(code==23)style.italic=false;
    else if(code==24)style.underline=false;
    else if(code==29)style.strike=false;
    else if(code==39)style.foreground.reset();
    else if(code==49)style.background.reset();
    else if(code>=30&&code<=37)
      style.foreground=ftxui::Color(static_cast<ftxui::Color::Palette16>(code-30));
    else if(code>=90&&code<=97)
      style.foreground=ftxui::Color(static_cast<ftxui::Color::Palette16>(code-90+8));
    else if(code>=40&&code<=47)
      style.background=ftxui::Color(static_cast<ftxui::Color::Palette16>(code-40));
    else if(code>=100&&code<=107)
      style.background=ftxui::Color(static_cast<ftxui::Color::Palette16>(code-100+8));
    else if(code==38){if(auto color=extended(values,i))style.foreground=color;}
    else if(code==48){if(auto color=extended(values,i))style.background=color;}
  }
}
} // namespace

ftxui::Element styleAnsiRun(ftxui::Element element,const AnsiStyle& style){
  if(style.bold)element=element|ftxui::bold;
  if(style.italic)element=element|ftxui::italic;
  if(style.underline)element=element|ftxui::underlined;
  if(style.strike)element=element|ftxui::strikethrough;
  if(style.dim)element=element|ftxui::dim;
  if(style.foreground)element=element|ftxui::color(*style.foreground);
  if(style.background)element=element|ftxui::bgcolor(*style.background);
  if(!style.link.empty())element=element|ftxui::hyperlink(style.link);
  return element;
}
std::optional<ftxui::Color> sgrColor(const std::string& sequence,bool background){
  if(!sequence.starts_with("\x1b[")||!sequence.ends_with("m"))return {};
  Style style;
  applySgr(style,sequence.substr(2,sequence.size()-3));
  return background?style.background:style.foreground;
}
std::vector<std::vector<AnsiRun>> parseAnsiLines(const std::string& source){
  Style style;
  std::vector<std::vector<AnsiRun>> lines(1);
  std::string text;
  const auto flush=[&](){
    if(!text.empty()){lines.back().push_back({std::move(text),style});text.clear();}
  };
  for(std::size_t i=0;i<source.size();){
    if(source[i]=='\n'){flush();lines.emplace_back();++i;continue;}
    if(source[i]!='\x1b'){text+=source[i++];continue;}
    if(i+1<source.size()&&source[i+1]=='['){
      const auto end=source.find('m',i+2);
      if(end==std::string::npos){text+=source[i++];continue;}
      flush();applySgr(style,source.substr(i+2,end-(i+2)));i=end+1;continue;
    }
    if(i+1<source.size()&&source[i+1]==']'){
      const auto end=source.find('\x07',i+2);
      if(end==std::string::npos){text+=source[i++];continue;}
      const auto command=source.substr(i+2,end-(i+2));
      if(command.starts_with("8;;")){flush();style.link=command.substr(3);}
      i=end+1;continue;
    }
    text+=source[i++];
  }
  flush();
  return lines;
}
ftxui::Element projectAnsi(const std::string& source,bool dim){
  Style style;
  std::vector<ftxui::Element> lines,parts;
  std::string runText;
  const auto flush=[&](){
    if(!runText.empty()){
      words(parts,runText,style);runText.clear();
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
