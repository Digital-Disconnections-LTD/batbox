#pragma once

#include "event_loop/Loop.hpp"
#include "interactive/cards_system/SystemAPIErrorCard.hpp"
#include <functional>
#include <memory>

namespace batbox::interactive::cards_system {

// One retained card instance. Its timer lives on the caller's owner loop.
class APIErrorTicker {
public:
  APIErrorTicker(event_loop::Loop& owner,
                 std::function<void(const APIErrorCard&)> changed);
  ~APIErrorTicker();
  APIErrorTicker(const APIErrorTicker&) = delete;
  APIErrorTicker& operator=(const APIErrorTicker&) = delete;

  APIErrorCard setInput(APIErrorInput input);
  APIErrorCard current() const;
private:
  struct Core;
  std::shared_ptr<Core> core_;
};

} // namespace batbox::interactive::cards_system
