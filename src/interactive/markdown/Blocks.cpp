#include "interactive/markdown/Blocks.hpp"
#include "interactive/markdown/AnsiProjection.hpp"
#include "runtime/Encoding.hpp"
#include "runtime/JsonValue.hpp"
#include <ftxui/dom/node.hpp>
#include <ftxui/screen/screen.hpp>
#include <ftxui/screen/string.hpp>
#include <algorithm>
#include <utility>

namespace batbox::interactive::markdown {
namespace {
struct Glyph {
  std::string text;
  int width=1;
  const AnsiStyle* style=nullptr;
};
using GlyphLine=std::vector<Glyph>;

std::string fenceLanguage(const std::string& info){
  const auto end=info.find_first_of(" \t{");
  return value::trim(info.substr(0,end));
}

class CodeBlockNode final:public ftxui::Node {
public:
  CodeBlockNode(std::string label,std::vector<std::vector<AnsiRun>> lines,
      std::optional<ftxui::Color> frame,std::optional<ftxui::Color> labelColor,
      std::optional<ftxui::Color> background,bool dim,int columns):
      label_(std::move(label)),runs_(std::move(lines)),frame_(frame),
      labelColor_(labelColor),background_(background),dim_(dim),
      width_(std::max(minimumWidth,columns)){
    for(const auto& line:runs_){
      GlyphLine glyphs;
      for(const auto& run:line){
        const auto split=ftxui::Utf8ToGlyphs(run.text);
        for(std::size_t i=0;i<split.size();++i){
          if(split[i].empty())continue;
          const bool wide=i+1<split.size()&&split[i+1].empty();
          glyphs.push_back({split[i],wide?2:1,&run.style});
        }
      }
      lines_.push_back(std::move(glyphs));
    }
  }
  void ComputeRequirement()override{
    requirement_={};
    requirement_.min_x=minimumWidth;
    requirement_.min_y=static_cast<int>(rows(width_).size())+2;
  }
  void SetBox(ftxui::Box box)override{
    Node::SetBox(box);
    const int width=std::max(minimumWidth,box.x_max-box.x_min+1);
    if(width!=width_){width_=width;relayout_=true;}
  }
  void Check(Status* status)override{
    status->need_iteration|=relayout_||status->iteration==0;
    relayout_=false;
  }
  void Render(ftxui::Screen& screen)override{
    const int left=box_.x_min,right=box_.x_min+width_-1;
    const auto wrapped=rows(width_);
    const auto paintFrame=[&](int x,int y,const std::string& glyph){
      auto& cell=screen.CellAt(x,y);
      cell.character=glyph;
      if(frame_)cell.foreground_color=*frame_;
      if(dim_||!frame_)cell.dim=true;
    };
    // Top edge with the language label.
    int y=box_.y_min;
    paintFrame(left,y,"╭");
    for(int x=left+1;x<right;++x)paintFrame(x,y,"─");
    paintFrame(right,y,"╮");
    if(!label_.empty()&&width_>static_cast<int>(label_.size())+6){
      int x=left+2;
      screen.CellAt(x++,y).character=" ";
      for(const auto& glyph:ftxui::Utf8ToGlyphs(label_)){
        if(glyph.empty())continue;
        auto& cell=screen.CellAt(x++,y);
        cell.character=glyph;
        cell.dim=dim_;
        if(labelColor_)cell.foreground_color=*labelColor_;
      }
      screen.CellAt(x,y).character=" ";
    }
    for(const auto& row:wrapped){
      ++y;
      paintFrame(left,y,"│");
      paintFrame(right,y,"│");
      for(int x=left+1;x<right;++x){
        auto& cell=screen.CellAt(x,y);
        cell.character=" ";
        if(background_)cell.background_color=*background_;
      }
      int x=left+2;
      for(const auto& glyph:row){
        auto& cell=screen.CellAt(x,y);
        cell.character=glyph.text;
        const auto& style=*glyph.style;
        cell.bold=style.bold;cell.italic=style.italic;
        cell.underlined=style.underline;cell.strikethrough=style.strike;
        cell.dim=dim_||style.dim;
        if(style.foreground)cell.foreground_color=*style.foreground;
        if(style.background)cell.background_color=*style.background;
        if(glyph.width==2&&x+1<right){
          auto& next=screen.CellAt(x+1,y);
          next.character="";
          if(background_)next.background_color=*background_;
        }
        x+=glyph.width;
      }
    }
    ++y;
    paintFrame(left,y,"╰");
    for(int x=left+1;x<right;++x)paintFrame(x,y,"─");
    paintFrame(right,y,"╯");
  }
private:
  static constexpr int minimumWidth=8;
  std::vector<GlyphLine> rows(int width)const{
    const int inner=std::max(1,width-4);
    std::vector<GlyphLine> result;
    for(const auto& line:lines_){
      result.emplace_back();
      int used=0;
      for(const auto& glyph:line){
        if(used+glyph.width>inner){result.emplace_back();used=0;}
        result.back().push_back(glyph);used+=glyph.width;
      }
    }
    if(result.empty())result.emplace_back();
    return result;
  }
  std::string label_;
  std::vector<std::vector<AnsiRun>> runs_;
  std::vector<GlyphLine> lines_;
  std::optional<ftxui::Color> frame_,labelColor_,background_;
  bool dim_=false,relayout_=true;
  int width_=minimumWidth;
};

ftxui::Element marker(const std::string& text,const FormatServices::BlockStyle& style){
  auto element=ftxui::text(value::encodeUtf8(text));
  if(const auto color=sgrColor(style.listMarker))element=element|ftxui::color(*color);
  return element;
}
std::string listNumber(int depth,int value){
  if(depth%3==1){
    std::string result;
    while(value>0){--value;result.insert(result.begin(),static_cast<char>('a'+value%26));value/=26;}
    return result;
  }
  return std::to_string(value);
}
} // namespace

ftxui::Element projectCodeBlock(const Token& code,const FormatServices& services,
    const FormatServices::BlockStyle& style,int columns,bool dim){
  auto text=code.text;
  if(!text.empty()&&text.back()=='\n')text.pop_back();
  const auto language=fenceLanguage(code.language);
  std::string body=text;
  if(services.highlight&&!language.empty()&&services.supportsLanguage&&
      services.supportsLanguage(language))
    body=services.highlight(text,language);
  // Tabs expand to four columns inside code so indentation stays visible.
  std::string expanded;
  for(const auto ch:body){
    if(ch=='\t')expanded+="    ";
    else expanded+=ch;
  }
  return std::make_shared<CodeBlockNode>(language,parseAnsiLines(expanded),
    sgrColor(style.codeFrame),sgrColor(style.codeLabel),
    sgrColor(style.codeBackground,true),dim,columns);
}

ftxui::Element projectRule(const FormatServices::BlockStyle& style,int columns){
  const int width=style.readingWidth>0?std::min(columns,style.readingWidth):columns;
  std::string line;
  for(int i=0;i<std::max(3,width);++i)line+="─";
  auto element=ftxui::text(line);
  if(const auto color=sgrColor(style.rule))element=element|ftxui::color(*color);
  else element=element|ftxui::dim;
  return element|ftxui::size(ftxui::WIDTH,ftxui::LESS_THAN,std::max(3,width));
}

ftxui::Element projectList(const Token& list,const FormatServices& services,
    const FormatServices::BlockStyle& style,bool dim,int depth){
  ftxui::Elements items;
  for(std::size_t index=0;index<list.items.size();++index){
    const auto& item=list.items[index];
    std::string label;
    if(item.checked>=0)label=item.checked==1?"[✓] ":"[ ] ";
    else if(list.ordered)label=listNumber(depth,list.start+static_cast<int>(index))+". ";
    else label=depth%2==0?"• ":"◦ ";
    auto bullet=item.checked==1?ftxui::text(label)|ftxui::dim:marker(label,style);
    ftxui::Elements blocks;
    std::string prose;
    const auto flush=[&]{
      if(prose.empty())return;
      auto element=projectAnsi(value::trim(prose),dim);
      if(item.checked==1)element=element|ftxui::dim;
      blocks.push_back(std::move(element));prose.clear();
    };
    for(const auto& child:item.children){
      if(child.kind==Kind::List){
        flush();
        blocks.push_back(projectList(child,services,style,dim,depth+1));
      }else if(child.kind==Kind::Code){
        flush();
        blocks.push_back(projectCodeBlock(child,services,style,
          std::max(8,style.readingWidth),dim));
      }else{
        prose+=formatToken(child,services,depth+1,{},nullptr);
      }
    }
    flush();
    if(blocks.empty())blocks.push_back(ftxui::text(""));
    items.push_back(ftxui::hbox({bullet,ftxui::vbox(std::move(blocks))|ftxui::xflex}));
  }
  return ftxui::vbox(std::move(items));
}
} // namespace batbox::interactive::markdown
