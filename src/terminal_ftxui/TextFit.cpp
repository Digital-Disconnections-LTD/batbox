#include "terminal_ftxui/TextFit.hpp"
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
ftxui::Element fitText(std::string text){
  return std::make_shared<FitText>(std::move(text));
}
} // namespace batbox::terminal_ftxui
