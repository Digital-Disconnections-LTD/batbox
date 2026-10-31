#pragma once
#include "application_file_history/History.hpp"
#include "interactive/CommandUi.hpp"
#include "interactive/lifecycle/MessageState.hpp"
#include <functional>
#include <memory>

namespace batbox::application_frontend {
struct RewindModalInputs {
  interactive::UiRequest list;
  std::vector<interactive::TurnMessageRef> messages;
  std::size_t preselectedIndex=0;
  bool preselected=false;
  std::shared_ptr<application_file_history::History> history;
  std::function<void()> beforeRestore,close;
  std::function<void(interactive::TurnMessageRef)> restoreConversation;
  interactive::CommandUi::Done done;
  interactive::CommandUi::Ready ready;
  interactive::CommandUi::Error error;
};
class RewindModal final:public std::enable_shared_from_this<RewindModal> {
public:
  explicit RewindModal(RewindModalInputs inputs):inputs_(std::move(inputs)){}
  void start();
private:
  void showList();
  void choose(std::size_t index);
  void showOptions(interactive::TurnMessageRef message,
    application_file_history::DiffStats stats);
  void selectOption(interactive::TurnMessageRef message,
    const std::string& option);
  void complete();
  void fail(const std::string& message);
  RewindModalInputs inputs_;
  bool active_=true;
};
} // namespace batbox::application_frontend
