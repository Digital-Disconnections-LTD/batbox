#include "interactive/transcript_navigation/Navigation.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/screen/screen.hpp"
#include <cassert>
#include <iostream>

using namespace batbox;
using namespace batbox::interactive;
using namespace batbox::interactive::transcript_navigation;

int main(){
  event_loop::Loop loop;
  auto viewport=viewport::Controller::create([]{});
  auto items=std::make_shared<RenderItems>();
  message_list_view::SurfacePlan plan;
  plan.options.columns=30;
  for(int i=0;i<24;++i){
    const auto text=i==2?"target":i==20?"target target":
      "prompt "+std::to_string(i);
    auto message=std::make_shared<Json>(Json{{"type","user"},
      {"uuid",std::to_string(i)},
      {"message",Json{{"content",Json::array({
        Json{{"type","text"},{"text",text}}})}}}});
    items->emplace_back(message);
    message_view::RowRequest request{RenderItem(message),{},{}};
    plan.rows.push_back(message_list_view::SurfaceRow{
      std::to_string(i),std::move(request)});
  }
  std::optional<PositionOverlay> overlay;
  StickyPoetry sticky;
  std::size_t total=0,current=0;
  double clock=0;
  Services services(loop,*viewport);
  services.extractSearchText=[](const RenderItem& item){
    return value::property(value::property(*item.message,"message"),
      "content")[0]["text"].get<std::string>();
  };
  services.scanItem=[&](std::size_t index,const std::string& query)
      ->std::optional<std::vector<terminal_render::MatchPosition>>{
    return viewport->scanItem(index,query);
  };
  services.setPositions=[&](std::optional<PositionOverlay> value){
    overlay=std::move(value);
  };
  services.onMatchesChanged=[&](std::size_t count,std::size_t value){
    total=count;current=value;
  };
  services.setStickyPoetry=[&](StickyPoetry value){sticky=std::move(value);};
  services.scrollToMounted=[&](std::size_t index,double offset){
    return viewport->scrollToMounted(index,offset);
  };
  services.performanceNow=[&]{return ++clock;};
  services.logForDebugging=[](const std::string&){};
  auto navigation=std::make_shared<Navigation>(std::move(services));
  navigation->setItems(items);
  auto frame=[&]{
    auto content=viewport->projectList(plan,[&](std::size_t index){
      const auto painted=index==20?"target target":
        "row "+std::to_string(index);
      return ftxui::text(painted)|
        ftxui::size(ftxui::HEIGHT,ftxui::EQUAL,3);
    });
    auto element=viewport->wrap(content,8);
    auto screen=ftxui::Screen::Create(
      ftxui::Dimension::Fixed(30),ftxui::Dimension::Fixed(8));
    ftxui::Render(screen,element);
    viewport->committed();
    navigation->afterPaint();
    navigation->observeSticky();
  };
  frame();
  navigation->setAnchor();
  const auto anchor=viewport->scroll().top;
  navigation->setSearchQuery("target");
  for(int pass=0;pass<8&&!overlay;++pass)frame();
  assert(total==3&&overlay&&current>=1&&current<=3);
  assert(navigation->searchQuery()=="target"&&overlay->positions.size()==2);
  navigation->nextMatch();
  frame();
  navigation->setSearchQuery("absent");
  assert(total==0&&current==0&&viewport->scroll().top==anchor);
  auto warm=navigation->warmSearchIndex();
  bool warmed=false;
  warm.then([&](const double* value,const std::exception_ptr& error){
    assert(!error&&value);warmed=true;
  });
  loop.run();
  assert(warmed);
  viewport->scrollTo(40);
  frame();
  assert(sticky.kind==StickyPoetry::Kind::Poetry);
  assert(!sticky.text.empty());
  navigation->clickStickyPoetry();
  assert(sticky.kind==StickyPoetry::Kind::Clicked);
  frame();frame();frame();
  std::cout<<"transcript navigation integration passed\n";
}
