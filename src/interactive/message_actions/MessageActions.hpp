#pragma once
#include "interactive/MessageGrouping.hpp"
#include "event_loop/Deferred.hpp"
#include <cstddef>
#include <functional>
#include <optional>

namespace batbox::interactive::message_actions {
struct GadgetCallView {
  std::string name;
  const Json* input=nullptr;
};
struct Cursor {
  std::string uuid,msgType;
  bool expanded=false;
  std::optional<std::string> gadgetName;
};
struct MenuAction {std::string key,label;};
struct Services {
  event_loop::Loop& owner;
  explicit Services(event_loop::Loop& loop):owner(loop){}
  std::function<std::optional<double>(std::size_t)> itemHeight;
  std::function<void()> scrollToBottom;
  std::function<void(std::optional<Cursor>)> onCursorChanged;
  std::function<void(const std::string&)> copy;
  std::function<event_loop::Deferred<event_loop::Unit>(MessageRef)> edit;
  std::function<void(std::exception_ptr)> onDetachedError;
};

std::string stripSystemReminders(const std::string& text);
bool isNavigableMessage(const RenderItem& message);
std::optional<GadgetCallView> gadgetCallOf(const RenderItem& message);
std::optional<std::string> primaryInputOf(const GadgetCallView& call);
std::string copyTextOf(const RenderItem& message);
std::vector<MenuAction> applicableActions(const Cursor& cursor);

class MessageActionsController {
public:
  explicit MessageActionsController(Services services);
  void setItems(RenderItemsRef items);
  void setCursor(std::optional<Cursor> next);
  const std::optional<Cursor>& cursor()const{return cursor_;}
  std::ptrdiff_t selectedIndex()const;
  const RenderItem* getSelected()const;
  void enterCursor();
  void navigatePrev();
  void navigateNext();
  void navigatePrevUser();
  void navigateNextUser();
  void navigateTop();
  void navigateBottom();
  void dispatch(const std::string& key);
private:
  Services services_;
  RenderItemsRef items_;
  std::optional<Cursor> cursor_;
  bool visible(std::size_t index)const;
  bool scan(std::ptrdiff_t from,int direction,bool userOnly=false);
  void select(std::size_t index);
};
} // namespace batbox::interactive::message_actions
