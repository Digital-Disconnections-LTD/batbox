#include "terminal_ftxui/TextFit.hpp"
#include "runtime/JsonValue.hpp"
#include <ftxui/dom/node.hpp>
#include <ftxui/screen/string.hpp>
#include <algorithm>
#include <utility>

namespace batbox::terminal_ftxui {
namespace {
class FitText final:public ftxui::Node {
public:
  explicit FitText(std::string text):text_(std::move(text)){}
  void ComputeRequirement()override{
    requirement_={};
    requirement_.min_x=displayWidth(text_);
    requirement_.min_y=1;
    requirement_.flex_shrink_x=1;
  }
  void SetBox(ftxui::Box box)override{
    Node::SetBox(box);
    const int width=std::max(0,box.x_max-box.x_min+1);
    // Children receive the final text so selection and copy see what is drawn.
    children_={ftxui::text(truncateColumns(text_,width))};
    children_.front()->ComputeRequirement();
    children_.front()->SetBox(box);
  }
private:
  std::string text_;
};
} // namespace

int displayWidth(std::string_view text){
  return ftxui::string_width(std::string(text));
}
std::string truncateColumns(std::string_view text,int columns,
    std::string_view ellipsis){
  if(columns<=0)return {};
  if(displayWidth(text)<=columns)return std::string(text);
  const int room=std::max(0,columns-displayWidth(ellipsis));
  std::string result;
  int width=0;
  for(const auto& glyph:ftxui::Utf8ToGlyphs(std::string(text))){
    if(glyph.empty())continue;
    const int next=ftxui::string_width(glyph);
    if(width+next>room)break;
    result+=glyph;width+=next;
  }
  if(displayWidth(ellipsis)>columns)return result;
  return result+std::string(ellipsis);
}
namespace {
// Length of the UTF-8 sequence at at, or 1 for a byte that does not start one.
std::size_t sequenceLength(std::string_view text,std::size_t at){
  const auto lead=static_cast<unsigned char>(text[at]);
  const std::size_t length=lead>=0xc2&&lead<=0xdf?2:lead>=0xe0&&lead<=0xef?3:
    lead>=0xf0&&lead<=0xf4?4:1;
  if(length==1||at+length>text.size())return 1;
  for(std::size_t i=1;i<length;++i){
    const auto next=static_cast<unsigned char>(text[at+i]);
    if(next<0x80||next>0xbf)return 1;
  }
  return length;
}
// The next grapheme as it appears in text, or one code point where the
// glyph cells differ from the bytes (dropped control bytes). Segmenting only a
// short window keeps long unbroken words linear.
std::size_t nextGrapheme(std::string_view text,std::size_t at){
  constexpr std::size_t window=64;
  const auto rest=text.substr(at,window);
  for(const auto& glyph:ftxui::Utf8ToGlyphs(rest)){
    if(glyph.empty())continue;
    if(rest.starts_with(glyph))return glyph.size();
    break;
  }
  return sequenceLength(text,at);
}
// Rows of at most width columns, except a lone grapheme wider than that.
std::vector<std::string> splitWord(std::string_view word,int width){
  std::vector<std::string> rows(1);
  int used=0;
  for(std::size_t at=0;at<word.size();){
    const auto length=nextGrapheme(word,at);
    const auto piece=word.substr(at,length);
    const int cells=displayWidth(piece);
    if(used>0&&used+cells>width){rows.emplace_back();used=0;}
    rows.back()+=piece;used+=cells;
    at+=length;
  }
  return rows;
}
} // namespace

std::vector<std::string> wrapColumns(std::string_view raw,int width){
  // Width measurement rejects malformed UTF-8; show it as U+FFFD instead.
  const auto clean=value::decodeUtf8(raw);
  const std::string_view line=clean;
  if(width<=0)return {clean};
  std::vector<std::string> rows;
  std::string current;
  int used=0;
  std::size_t start=0;
  const auto flush=[&]{rows.push_back(current);current.clear();used=0;};
  while(start<=line.size()){
    const auto end=line.find(' ',start);
    auto word=std::string(line.substr(start,end==std::string_view::npos?
      std::string_view::npos:end-start));
    int wordWidth=displayWidth(word);
    if(wordWidth>width){
      if(used>0)flush();
      auto pieces=splitWord(word,width);
      for(std::size_t i=0;i+1<pieces.size();++i)rows.push_back(std::move(pieces[i]));
      word=std::move(pieces.back());
      wordWidth=displayWidth(word);
    }
    if(used>0&&used+1+wordWidth>width)flush();
    if(used>0){current+=' ';++used;}
    current+=word;used+=wordWidth;
    if(end==std::string_view::npos)break;
    start=end+1;
  }
  if(!current.empty()||rows.empty())rows.push_back(current);
  return rows;
}
ftxui::Element fitText(std::string text){
  return std::make_shared<FitText>(std::move(text));
}
} // namespace batbox::terminal_ftxui
