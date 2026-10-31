#include "message_view_fixture.hpp"
#include "message_view_services.hpp"
#include "interactive/message_list_view/Projection.hpp"
#include "interactive/viewport/Controller.hpp"
#include "ftxui/dom/node.hpp"
#include "ftxui/screen/screen.hpp"
#include <cassert>
#include <iostream>
using namespace message_view_fixture;
namespace {
std::string draw(const std::optional<ftxui::Element>& element,int width=64){
  assert(element);auto screen=ftxui::Screen::Create(
    ftxui::Dimension::Fixed(width),ftxui::Dimension::Fixed(14));
  ftxui::Render(screen,*element);std::string text;
  for(int y=0;y<screen.dimy();++y){
    for(int x=0;x<screen.dimx();++x)text+=screen.CellAt(x,y).character;
    text+='\n';
  }
  return text;
}
}
int main(){
  auto render=services();MessageImports imports;
  imports.fullscreen=[]{return true;};imports.feature=[](const std::string&){return false;};
  imports.logError=[](const std::string&){assert(false);};
  auto message=std::make_shared<Json>(Json{{"type","assistant"},{"uuid","a"},{"timestamp","time"},
    {"message",{{"model","native"},{"content",Json::array({
      {{"type","thinking"},{"thinking","hidden reasoning"}},
      {{"type","text"},{"text","final answer"}}})}}}});
  RowRequest request{RenderItem(message),{},std::make_shared<MessageLookups>()};
  request.screen="transcript";request.options.columns=64;request.options.lastThinkingBlockId="new:0";
  auto plan=prepareRow(request,imports);assert(plan.hasMetadata&&plan.displayMessage==message);
  auto text=draw(projectRow(plan,render));
  assert(text.find("04:05 PM")<text.find("final answer"));
  assert(text.find("native")<text.find("final answer"));assert(text.find("hidden reasoning")==std::string::npos);
  assert(plan.message.blocks[1].original.owner==message);
  (*message)["message"]["content"][1]["text"]="updated answer";
  text=draw(projectRow(plan,render));assert(text.find("updated answer")!=std::string::npos);

  auto user=std::make_shared<Json>(Json{{"type","user"},{"uuid","u"},{"imagePasteIds",Json::array({72})},
    {"message",{{"content",Json::array({{{"type","text"},{"text","question"}},{{"type","image"}},
      {{"type","tool_result"},{"tool_use_id","t"},{"content","broken tool"},{"is_error",true}}})}}}});
  auto lookups=std::make_shared<MessageLookups>();
  auto owner=std::make_shared<Json>(Json{{"type","assistant"},{"message",{{"content",Json::array({
    {{"type","tool_use"},{"id","t"},{"name","Read"},{"input",Json::object()}}})}}}});
  lookups->gadgetUseByGadgetUseID["t"]={owner,0};render.lookups=lookups;
  int imageCalls=0;
  render.remaining.images.getStoredImagePath=[&](int id){
    assert(id==72);++imageCalls;
    return std::optional<std::string>{};
  };
  MessageOptions options;options.columns=64;options.containerWidth=64;options.addMargin=false;
  text=draw(projectMessage(prepareMessage(RenderItem(user),options,imports),render));
  assert(imageCalls==1&&text.find("question")<text.find("[Image #72]"));
  assert(text.find("[Image #72]")<text.find("broken tool"));

  auto local=std::make_shared<Json>(Json{{"type","system"},{"uuid","local"},{"subtype","local_command"},
    {"content","command complete"},{"planContent","must not become user plan"}});
  text=draw(projectMessage(prepareMessage(RenderItem(local),options,imports),render));
  assert(text.find("command complete")!=std::string::npos);
  auto boundary=std::make_shared<Json>(Json{{"type","system"},{"subtype","compact_boundary"}});
  assert(!projectMessage(prepareMessage(RenderItem(boundary),options,imports),render));
  auto info=std::make_shared<Json>(Json{{"type","system"},{"subtype","local_command"},{"content","ready"}});
  assert(draw(projectMessage(prepareMessage(RenderItem(info),options,imports),render)).find("ready")!=std::string::npos);
  namespace list=batbox::interactive::message_list_view;
  auto prepared=std::make_shared<PreparedMessageList>();
  prepared->renderable=std::make_shared<RenderItems>(RenderItems{RenderItem(message)});
  prepared->normalizedMessages={message};prepared->lookups=*lookups;
  list::SurfaceState surfaceState;list::SurfaceOptions surfaceOptions;
  surfaceOptions.hideLogo=true;surfaceOptions.columns=64;surfaceOptions.streamingText="streaming answer";
  auto surface=list::prepareSurface(prepared,surfaceOptions,surfaceState,1000,imports,
    {[](const std::string&,const Json&){return false;}});
  list::ProjectionImports surfaceRender;surfaceRender.messages=render;
  surfaceRender.renderStreamingMarkdown=render.assistant.projection.renderMarkdown;
  text=draw(list::projectSurface(surface,surfaceRender));
  assert(text.find("updated answer")<text.find("streaming answer"));
  batbox::event_loop::Loop loop;
  int redraws=0;
  auto viewport=batbox::interactive::viewport::Controller::create([&]{++redraws;});
  auto history=std::make_shared<PreparedMessageList>();
  history->renderable=std::make_shared<RenderItems>();
  for(int index=0;index<1000;++index){
    auto row=std::make_shared<Json>(Json{{"type","assistant"},{"uuid",std::to_string(index)},
      {"message",{{"content",Json::array({{{"type","text"},{"text","row "+std::to_string(index)}}})}}}});
    history->renderable->push_back(RenderItem(row));history->normalizedMessages.push_back(row);
  }
  surfaceOptions.virtualScroll=true;
  surfaceRender.renderVirtualList=[&](const list::SurfacePlan& plan,const list::RowRenderer& renderRow){
    return viewport->projectList(plan,renderRow);
  };
  const auto frame=[&]{
    auto plan=list::prepareSurface(history,surfaceOptions,surfaceState,1000,imports,
      {[](const std::string&,const Json&){return false;}});
    auto element=viewport->wrap(list::projectSurface(plan,surfaceRender),14);
    auto result=draw(element,surfaceOptions.columns);viewport->committed();return result;
  };
  text=frame();assert(text.find("row 999")!=std::string::npos);
  assert(viewport->visible().mounted.end-viewport->visible().mounted.start<=300);
  viewport->scrollToIndex(300);
  for(int pass=0;pass<40&&viewport->ranges().itemTop(300)<0;++pass)text=frame();
  assert(viewport->ranges().itemTop(300)>=0&&text.find("row ")!=std::string::npos);
  surfaceOptions.columns=48;frame();frame();frame();
  assert(viewport->visible().mounted.end-viewport->visible().mounted.start<=300&&redraws>0);
  std::cout<<"7 native message/list/viewport workflows and reference-identity checks pass\n";
}
