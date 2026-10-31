#include "interactive/cards_system/SystemAPIErrorTicker.hpp"
#include <stdexcept>
#include <utility>

namespace batbox::interactive::cards_system {

struct APIErrorTicker::Core : std::enable_shared_from_this<Core> {
  Core(event_loop::Loop& owner,
       std::function<void(const APIErrorCard&)> changed)
      : owner(owner), changed(std::move(changed)) {}
  event_loop::Loop& owner;
  std::function<void(const APIErrorCard&)> changed;
  event_loop::Loop::TimerHandle timer;
  APIErrorState state;
  APIErrorInput input;
  APIErrorCard card;
  bool hasInput = false;

  void schedule() {
    timer.cancel();
    if (!card.intervalActive) return;
    std::weak_ptr<Core> weak = shared_from_this();
    timer = owner.setTimeout(1000, [weak] {
      if (auto core = weak.lock()) {
        core->state.intervalTick(core->card);
        core->card = core->state.render(core->input);
        core->schedule();
        core->changed(core->card);
      }
    });
  }
};

APIErrorTicker::APIErrorTicker(
    event_loop::Loop& owner,
    std::function<void(const APIErrorCard&)> changed)
    : core_(std::make_shared<Core>(owner, std::move(changed))) {
  if (!core_->changed)
    throw std::invalid_argument("API error view change callback required");
}

APIErrorTicker::~APIErrorTicker() { core_->timer.cancel(); }

APIErrorCard APIErrorTicker::setInput(APIErrorInput input) {
  if (!core_->owner.isOwnerThread())
    throw std::logic_error("API error view belongs to its owner loop");
  core_->input = std::move(input);
  core_->hasInput = true;
  core_->card = core_->state.render(core_->input);
  core_->schedule();
  core_->changed(core_->card);
  return core_->card;
}

APIErrorCard APIErrorTicker::current() const {
  if (!core_->hasInput)
    throw std::logic_error("API error view has no input");
  return core_->card;
}

} // namespace batbox::interactive::cards_system
