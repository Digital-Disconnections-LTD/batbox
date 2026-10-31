#pragma once
#include "interactive/viewport/Range.hpp"
#include "interactive/message_list_view/Projection.hpp"
#include "terminal_ftxui/ElementSearch.hpp"
#include <ftxui/dom/node.hpp>
#include <functional>
#include <unordered_set>

namespace batbox::interactive::viewport {
class Controller final:public std::enable_shared_from_this<Controller> {
public:
  static std::shared_ptr<Controller> create(std::function<void()> requestRedraw);
  ftxui::Element projectList(const message_list_view::SurfacePlan& plan,
    const message_list_view::RowRenderer& renderRow);
  ftxui::Element wrap(ftxui::Element content,int rows);
  void committed();
  void scrollBy(double rows);
  void scrollTo(double row);
  void scrollToBottom();
  void scrollToIndex(std::size_t index);
  bool scrollToMounted(std::size_t index,double offset=1);
  std::optional<std::vector<terminal_render::MatchPosition>> scanItem(
    std::size_t index,const std::string& query)const;
  const RangeModel& ranges()const{return ranges_;}
  const ScrollSnapshot& scroll()const{return scroll_;}
  const RangeResult& visible()const{return visible_;}
  bool atBottom()const;
  double viewportTop()const{return viewportTop_;}
private:
  class ScrollNode;
  class MeasuredNode;
  explicit Controller(std::function<void()> requestRedraw);
  void layout(const ftxui::Box& viewport,int contentHeight);
  std::function<void()> requestRedraw_;
  RangeModel ranges_;
  ScrollSnapshot scroll_;
  RangeResult visible_;
  std::shared_ptr<Geometry> spacer_=std::make_shared<Geometry>();
  std::unordered_set<std::string> active_,previous_;
  std::unordered_map<std::string,ftxui::Element> elements_;
  double viewportTop_=0;
  double contentHeight_=0;
  int columns_=80;
  std::shared_ptr<Geometry> anchor_;
  double anchorOffset_=0;
};
} // namespace batbox::interactive::viewport
