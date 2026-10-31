#include "terminal_output/Output.hpp"
#include <utility>

namespace batbox::terminal_output {
Output::Output(double w,double h,std::shared_ptr<StylePool> styles,
    ScreenRef target,DebugWarning warning):width(w),height(h),
    stylePool(std::move(styles)),screen(std::move(target)),
    warning_(std::move(warning)){
  terminal_screen::resetScreen(*screen,width,height,warning_);
}
void Output::reset(double w,double h,ScreenRef target){
  width=w;height=h;screen=std::move(target);
  operations.clear();
  terminal_screen::resetScreen(*screen,width,height,warning_);
  if(charStash.size()>16384)charStash.clear();
}
void Output::blit(ScreenRef src,double x,double y,double w,double h){
  operations.emplace_back(BlitOperation{std::move(src),x,y,w,h});
}
void Output::shift(double top,double bottom,double n){
  operations.emplace_back(ShiftOperation{top,bottom,n});
}
void Output::clear(std::shared_ptr<Rectangle> region,
    std::optional<bool> fromAbsolute){
  operations.emplace_back(ClearOperation{std::move(region),fromAbsolute});
}
void Output::noSelect(std::shared_ptr<Rectangle> region){
  operations.emplace_back(NoSelectOperation{std::move(region)});
}
void Output::write(double x,double y,std::string text,
    std::shared_ptr<std::vector<bool>> softWrap){
  if(text.empty())return;
  operations.emplace_back(WriteOperation{x,y,std::move(text),
    std::move(softWrap)});
}
void Output::clip(ClipRef value){
  operations.emplace_back(ClipOperation{std::move(value)});
}
void Output::unclip(){operations.emplace_back(UnclipOperation{});}
} // namespace batbox::terminal_output
