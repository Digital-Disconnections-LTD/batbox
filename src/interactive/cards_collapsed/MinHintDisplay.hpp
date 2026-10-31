#pragma once

#include "event_loop/Loop.hpp"
#include <functional>
#include <memory>
#include <optional>

namespace batbox::interactive::cards_collapsed {

// Owner-loop equivalent of a retained minimum-display-time hook. `render`
// reads current state; `committed` runs the effect after view commit.
class MinHintDisplay final : public std::enable_shared_from_this<MinHintDisplay> {
public:
  using Value=std::optional<std::string>;
  static std::shared_ptr<MinHintDisplay> create(event_loop::Loop& loop,
      std::function<double()> nowMs,std::function<void()> onChanged);
  ~MinHintDisplay();
  Value render(const Value& incoming,int minMs);
  void committed(const Value& incoming,int minMs);
  void cancel();
private:
  MinHintDisplay(event_loop::Loop& loop,std::function<double()> nowMs,
                 std::function<void()> onChanged);
  event_loop::Loop& loop_;
  std::function<double()> nowMs_;
  std::function<void()> onChanged_;
  Value displayed_,effectValue_;
  int effectMinMs_=0;
  bool initialized_=false,effectInitialized_=false;
  double lastShownAt_=0;
  event_loop::Loop::TimerHandle timer_;
};
}
