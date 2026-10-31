#pragma once
#include "termio/Clipboard.hpp"

namespace batbox::interactive::navigation {
struct CopyNotification {
  std::string key="selection-copied",text="copied",color="success",
    priority="immediate";
  int timeoutMs=2000;
};
struct CopyServices {
  event_loop::Loop& owner;
  explicit CopyServices(event_loop::Loop& loop):owner(loop){}
  std::function<event_loop::Deferred<std::string>(const std::string&)>
    setClipboard;
  std::function<void(const std::string&)> writeTerminal;
  std::function<void(const CopyNotification&)> addNotification;
  std::function<void(std::exception_ptr)> onDetachedError;
};
void copyMessageText(const std::string& text,const CopyServices& services);
void bindClipboard(CopyServices& services,termio::ClipboardModule& clipboard);
} // namespace batbox::interactive::navigation
