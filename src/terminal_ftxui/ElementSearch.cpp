#include "terminal_ftxui/ElementSearch.hpp"
#include "runtime/JsonValue.hpp"
#include <ftxui/screen/string.hpp>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace batbox::terminal_ftxui {
namespace {
std::vector<terminal_render::MatchPosition> scan(ftxui::Screen& screen,
    ftxui::Box box,const std::string& query){
  box=ftxui::Box::Intersection(box,{0,screen.dimx()-1,0,screen.dimy()-1});
  const auto positions=terminal_search::scanRows(box.x_max-box.x_min+1,
    box.y_max-box.y_min+1,terminal_search::lowercase(value::utf16CodeUnits(query)),
    [&](double column,double row){
      const auto& cell=screen.CellAt(box.x_min+static_cast<int>(column),box.y_min+static_cast<int>(row));
      if(!cell.character.empty())return terminal_search::Cell{cell.character,false};
      const auto col=box.x_min+static_cast<int>(column);
      const bool wideTail=col>box.x_min&&ftxui::string_width(screen.CellAt(col-1,
        box.y_min+static_cast<int>(row)).character)==2;
      return terminal_search::Cell{wideTail?"":" ",wideTail};
    });
  auto result=positions;
  for(auto& position:result){position.col+=box.x_min;position.row+=box.y_min;}
  return result;
}
class OverlayNode final:public ftxui::Node {
public:
  OverlayNode(ftxui::Element content,std::function<std::string()> query,
      std::function<std::optional<PositionedMatches>()> current):Node({std::move(content)}),
      query_(std::move(query)),current_(std::move(current)){}
  void SetBox(ftxui::Box box)override{box_=box;children_[0]->SetBox(box);}
  void Render(ftxui::Screen& screen)override{
    children_[0]->Render(screen);
    const auto visible=ftxui::Box::Intersection(box_,screen.stencil);
    for(const auto& position:scan(screen,visible,query_()))
      for(int col=static_cast<int>(position.col);col<position.col+position.len;++col)
        screen.CellAt(col,static_cast<int>(position.row)).inverted=true;
    const auto selected=current_();if(!selected)return;
    const auto index=selected->currentIndex;
    if(index<0||index>=static_cast<double>(selected->positions.size()))return;
    if(!std::isfinite(index)||std::trunc(index)!=index)throw std::runtime_error("Cannot read properties of undefined (reading 'row')");
    const auto& position=selected->positions[static_cast<std::size_t>(index)];
    const auto row=static_cast<int>(std::floor(position.row+selected->rowOffset));
    if(row<visible.y_min||row>visible.y_max)return;
    for(int col=static_cast<int>(position.col);col<position.col+position.len;++col){
      if(col<visible.x_min||col>visible.x_max)continue;
      auto& cell=screen.CellAt(col,row);cell.foreground_color=ftxui::Color::Yellow;
      cell.background_color=ftxui::Color::Default;cell.inverted=true;cell.bold=true;cell.underlined=true;
    }
  }
private:
  std::function<std::string()> query_;
  std::function<std::optional<PositionedMatches>()> current_;
};
}
std::vector<terminal_render::MatchPosition> scanElement(const ftxui::Element& element,
    int columns,int rows,const std::string& query){
  if(!element||columns<=0||rows<=0)return {};
  auto screen=ftxui::Screen::Create(ftxui::Dimension::Fixed(columns),ftxui::Dimension::Fixed(rows));
  ftxui::Render(screen,element);
  return scan(screen,{0,columns-1,0,rows-1},query);
}
ftxui::Element withSearchOverlay(ftxui::Element content,std::function<std::string()> query,
    std::function<std::optional<PositionedMatches>()> current){
  if(!content||!query||!current)throw std::invalid_argument("Native search overlay providers required");
  return std::make_shared<OverlayNode>(std::move(content),std::move(query),std::move(current));
}
} // namespace batbox::terminal_ftxui
