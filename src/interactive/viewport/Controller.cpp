#include "interactive/viewport/Controller.hpp"
#include <ftxui/dom/elements.hpp>
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace batbox::interactive::viewport {
class Controller::MeasuredNode final:public ftxui::Node {
public:
  MeasuredNode(ftxui::Element child,std::shared_ptr<Geometry> geometry,
      std::shared_ptr<Controller> owner):Node({std::move(child)}),
      geometry_(std::move(geometry)),owner_(std::move(owner)){}
  void SetBox(ftxui::Box box)override{
    box_=box;children_[0]->SetBox(box);
    geometry_->top=box.y_min-owner_->viewportTop_+std::floor(owner_->scroll_.top);
    geometry_->height=std::max(0,box.y_max-box.y_min+1);
    geometry_->width=std::max(0,box.x_max-box.x_min+1);
  }
private:
  std::shared_ptr<Geometry> geometry_;
  std::shared_ptr<Controller> owner_;
};
class Controller::ScrollNode final:public ftxui::Node {
public:
  ScrollNode(ftxui::Element child,int height,std::shared_ptr<Controller> owner):
    Node({std::move(child)}),height_(height),owner_(std::move(owner)){}
  void ComputeRequirement()override{
    Node::ComputeRequirement();requirement_.min_y=height_;
    requirement_.flex_grow_y=0;requirement_.flex_shrink_y=1;
  }
  void SetBox(ftxui::Box box)override{
    box_=box;
    // Flex paragraphs first report an unwrapped height. Resolve their layout
    // at the actual viewport width before clamping a detached scroll position;
    // otherwise that provisional one-line height consumes the user's scroll.
    ftxui::Node::Status status{1,true};
    while(status.need_iteration&&status.iteration<20){
      children_[0]->ComputeRequirement();
      auto measured=box;
      measured.y_min=0;
      measured.y_max=std::max(children_[0]->requirement().min_y,height_)-1;
      children_[0]->SetBox(measured);
      status.need_iteration=false;++status.iteration;
      children_[0]->Check(&status);
    }
    const auto contentHeight=children_[0]->requirement().min_y;
    owner_->layout(box,contentHeight);
    const auto assign=[&]{
      auto content=box;content.y_min-=static_cast<int>(std::floor(owner_->scroll_.top));
      content.y_max=content.y_min+std::max(contentHeight,box.y_max-box.y_min+1)-1;
      children_[0]->SetBox(content);
    };
    assign();
    if(owner_->anchor_&&owner_->anchor_->width>0){
      owner_->scroll_.top=std::clamp(owner_->anchor_->top+owner_->anchorOffset_,0.0,
        std::max(0.0,contentHeight-owner_->scroll_.height));
      owner_->scroll_.pendingDelta=0;owner_->anchor_.reset();assign();
    }
  }
  void Render(ftxui::Screen& screen)override{
    const auto previous=screen.stencil;
    screen.stencil=ftxui::Box::Intersection(previous,box_);
    children_[0]->Render(screen);screen.stencil=previous;
  }
private:
  int height_;
  std::shared_ptr<Controller> owner_;
};
Controller::Controller(std::function<void()> requestRedraw):requestRedraw_(std::move(requestRedraw)){
  if(!requestRedraw_)throw std::invalid_argument("Virtual viewport redraw callback required");
}
std::shared_ptr<Controller> Controller::create(std::function<void()> requestRedraw){
  return std::shared_ptr<Controller>(new Controller(std::move(requestRedraw)));
}
ftxui::Element Controller::projectList(const message_list_view::SurfacePlan& plan,
    const message_list_view::RowRenderer& renderRow){
  if(!renderRow)throw std::invalid_argument("Virtual row renderer required");
  std::vector<std::string> keys;keys.reserve(plan.rows.size());
  for(const auto& row:plan.rows)keys.push_back(row.key);
  ranges_.setKeys(std::move(keys));columns_=plan.options.columns;
  visible_=ranges_.prepare(columns_,scroll_);active_.clear();
  const auto spacer=[](double height){return ftxui::vbox(ftxui::Elements{})|
    ftxui::size(ftxui::HEIGHT,ftxui::EQUAL,static_cast<int>(std::ceil(height)));};
  ftxui::Elements elements;
  elements.push_back(std::make_shared<MeasuredNode>(spacer(visible_.topSpacer),spacer_,shared_from_this()));
  for(auto index=visible_.mounted.start;index<visible_.mounted.end;++index){
    const auto& key=plan.rows[index].key;active_.insert(key);
    auto element=renderRow(index);if(!element)element=ftxui::vbox(ftxui::Elements{});
    elements_[key]=element;
    elements.push_back(std::make_shared<MeasuredNode>(std::move(element),ranges_.mount(key),shared_from_this()));
  }
  if(visible_.bottomSpacer>0)elements.push_back(spacer(visible_.bottomSpacer));
  return ftxui::vbox(std::move(elements));
}
ftxui::Element Controller::wrap(ftxui::Element content,int rows){
  if(!content||rows<=0)throw std::invalid_argument("Virtual viewport content/height required");
  return std::make_shared<ScrollNode>(std::move(content),rows,shared_from_this());
}
void Controller::layout(const ftxui::Box& viewport,int contentHeight){
  contentHeight_=contentHeight;
  viewportTop_=viewport.y_min;scroll_.height=std::max(0,viewport.y_max-viewport.y_min+1);
  const auto maximum=std::max(0.0,contentHeight-scroll_.height);
  if(scroll_.sticky){scroll_.top=maximum;scroll_.pendingDelta=0;return;}
  const auto requested=std::clamp(std::max(0.0,scroll_.top)+scroll_.pendingDelta,0.0,maximum);
  auto top=requested;
  if(visible_.clampMin)top=std::max(top,*visible_.clampMin);
  if(visible_.clampMax)top=std::min(top,*visible_.clampMax);
  scroll_.top=std::clamp(top,0.0,maximum);scroll_.pendingDelta=requested-scroll_.top;
  // Reaching the real end by scrolling resumes follow mode. New output while
  // detached leaves the viewport anchored at the reader's current position.
  if(scroll_.top>=maximum&&scroll_.pendingDelta>=0){
    scroll_.sticky=true;scroll_.pendingDelta=0;requestRedraw_();
  }
}
bool Controller::atBottom()const{
  return scroll_.sticky||scroll_.top+scroll_.pendingDelta>=
    std::max(0.0,contentHeight_-scroll_.height);
}
void Controller::committed(){
  for(const auto& key:previous_)if(!active_.contains(key)){ranges_.unmount(key);elements_.erase(key);}
  previous_=active_;
  ranges_.committed(spacer_->width>0?spacer_->top:ranges_.origin());
  if(scroll_.pendingDelta!=0)requestRedraw_();
}
void Controller::scrollBy(double rows){
  if(!std::isfinite(rows))throw std::invalid_argument("Finite scroll delta required");
  scroll_.pendingDelta+=rows;scroll_.sticky=false;requestRedraw_();
}
void Controller::scrollTo(double row){
  if(!std::isfinite(row))throw std::invalid_argument("Finite scroll target required");
  scroll_.top=std::max(0.0,row);scroll_.pendingDelta=0;scroll_.sticky=false;requestRedraw_();
}
void Controller::scrollToBottom(){scroll_.sticky=true;scroll_.pendingDelta=0;requestRedraw_();}
void Controller::scrollToIndex(std::size_t index){
  if(index<ranges_.keys().size())scrollTo(ranges_.offset(index)+ranges_.origin());
}
bool Controller::scrollToMounted(std::size_t index,double offset){
  const auto geometry=ranges_.itemGeometry(index);
  if(!geometry||geometry->width<=0)return false;
  anchor_=geometry;anchorOffset_=offset;scroll_.sticky=false;scroll_.pendingDelta=0;requestRedraw_();return true;
}
std::optional<std::vector<terminal_render::MatchPosition>> Controller::scanItem(
    std::size_t index,const std::string& query)const{
  const auto geometry=ranges_.itemGeometry(index);
  if(!geometry||geometry->height<=0||geometry->width<=0||index>=ranges_.keys().size())return {};
  const auto found=elements_.find(ranges_.keys()[index]);if(found==elements_.end())return {};
  const auto& element=found->second;if(!element)return {};
  return terminal_ftxui::scanElement(element,static_cast<int>(std::ceil(geometry->width)),
    static_cast<int>(std::ceil(geometry->height)),query);
}
} // namespace batbox::interactive::viewport
