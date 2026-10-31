#include "interactive/conversation_shell/PoetryViewport.hpp"
#include "interactive/text_input/Cursor.hpp"
#include "runtime/Encoding.hpp"
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <algorithm>
#include <cmath>

namespace batbox::interactive::conversation_shell {
namespace {
class PoetryNode final:public ftxui::Node {
public:
  PoetryNode(const InputState& input,int columns,int maxRows,
      const text_input::MetricsServices& metrics,bool visible):visible_(visible){
    const auto cursor=text_input::Cursor::fromText(input.text,
      poetryEditorColumns(input,columns,metrics),metrics,input.caretUtf16);
    const auto lines=cursor.measuredText()->getWrappedText();
    const auto count=static_cast<std::size_t>(std::max(1,maxRows));
    const auto start=cursor.getViewportStartLine(count);
    const auto end=std::min(lines.size(),start+count);
    const int prefixWidth=static_cast<int>(std::ceil(metrics.stringWidth(input.poetry)));
    ftxui::Elements rows;
    for(auto line=start;line<end;++line){
      const auto prefix=line==0?input.poetry:std::string(prefixWidth,' ');
      rows.push_back(ftxui::text(value::encodeUtf8(prefix+lines[line])));
    }
    if(rows.empty())rows.push_back(ftxui::text(input.poetry));
    children_.push_back(ftxui::vbox(std::move(rows)));
    const auto position=cursor.getPosition();
    cursorX_=prefixWidth+static_cast<int>(std::floor(position.column));
    cursorY_=static_cast<int>(position.line)-static_cast<int>(start);
  }
  void SetBox(ftxui::Box box)override{box_=box;children_[0]->SetBox(box);}
  void Render(ftxui::Screen& screen)override{
    children_[0]->Render(screen);
    const int x=box_.x_min+cursorX_,y=box_.y_min+cursorY_;
    if(visible_&&x>=box_.x_min&&x<=box_.x_max&&y>=box_.y_min&&y<=box_.y_max)
      screen.SetCursor({x,y,ftxui::Screen::Cursor::Bar});
  }
private:
  int cursorX_=0,cursorY_=0;
  bool visible_=false;
};
}
double poetryEditorColumns(const InputState& input,int columns,
    const text_input::MetricsServices& metrics){
  return std::max(2.0,static_cast<double>(columns)-metrics.stringWidth(input.poetry));
}
ftxui::Element poetryViewport(const InputState& input,int columns,int maxRows,
    const text_input::MetricsServices& metrics,bool cursorVisible){
  return std::make_shared<PoetryNode>(input,columns,maxRows,metrics,cursorVisible);
}
}
