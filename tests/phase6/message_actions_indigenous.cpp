#include "interactive/message_actions/MessageActions.hpp"
#include <cassert>
#include <iostream>

using namespace batbox;
using namespace batbox::interactive;
using namespace batbox::interactive::message_actions;

int main(){
  event_loop::Loop loop;
  const auto user=std::make_shared<Json>(Json{{"type","user"},{"uuid","u"},
    {"message",Json{{"content",Json::array({Json{{"type","text"},
      {"text","<system-reminder>hint</system-reminder>  hello"}}})}}}});
  const auto gadget=std::make_shared<Json>(Json{{"type","assistant"},
    {"uuid","a"},{"message",Json{{"content",Json::array({
      Json{{"type","tool_use"},{"name","Read"},
        {"input",Json{{"file_path","/tmp/file"}}}}})}}}});
  const auto result=std::make_shared<Json>(Json{{"type","user"},{"uuid","r"},
    {"message",Json{{"content",Json::array({Json{{"type","tool_result"},
      {"content",Json::array({Json{{"type","text"},{"text","found"}}})}}})}}}});
  const auto grouped=std::make_shared<Json>(Json{{"type","grouped_tool_use"},
    {"uuid","g"},{"toolName","Read"}});
  const auto hidden=std::make_shared<Json>(Json{{"type","system"},
    {"uuid","h"},{"subtype","api_metrics"}});
  auto items=std::make_shared<RenderItems>();
  items->emplace_back(hidden);
  items->emplace_back(user);
  items->emplace_back(gadget);
  items->emplace_back(grouped,MessageRefs{gadget},MessageRefs{result});
  assert(stripSystemReminders("  <system-reminder>hint</system-reminder>  hello")=="hello");
  assert(copyTextOf(items->at(1))=="hello");
  assert(copyTextOf(items->at(3))=="found");
  assert(primaryInputOf(*gadgetCallOf(items->at(2)))=="/tmp/file");
  std::vector<std::string> copied;
  std::optional<Cursor> published;
  bool repinned=false,edited=false;
  Services services(loop);
  services.itemHeight=[](std::size_t){return std::optional<double>{};};
  services.scrollToBottom=[&]{repinned=true;};
  services.onCursorChanged=[&](std::optional<Cursor> value){published=value;};
  services.copy=[&](const std::string& value){copied.push_back(value);};
  services.edit=[&](MessageRef message){
    assert(message==user);
    edited=true;
    event_loop::Deferred<event_loop::Unit> d(loop);d.resolve({});return d;
  };
  services.onDetachedError=[](std::exception_ptr error){if(error)
    std::rethrow_exception(error);};
  MessageActionsController actions(std::move(services));
  actions.setItems(items);
  actions.enterCursor();
  assert(actions.selectedIndex()==1&&published->uuid=="u");
  actions.dispatch("messageActions:enter");
  assert(edited&&!actions.cursor());
  actions.navigateTop();
  assert(actions.selectedIndex()==1);
  actions.navigateNext();
  assert(actions.selectedIndex()==2);
  const auto menu=applicableActions(*actions.cursor());
  assert(menu.size()==2&&menu[1].label=="copy path");
  actions.dispatch("messageActions:p");
  assert(copied.back()=="/tmp/file"&&!actions.cursor());
  actions.navigateBottom();
  assert(actions.selectedIndex()==3);
  actions.dispatch("messageActions:enter");
  assert(actions.cursor()->expanded);
  actions.dispatch("messageActions:escape");
  assert(!actions.cursor()->expanded);
  actions.dispatch("messageActions:c");
  assert(copied.back()=="found");
  actions.navigateBottom();
  actions.navigateNext();
  assert(repinned&&!actions.cursor());
  loop.runReady();
  std::cout<<"message actions integration passed\n";
}
