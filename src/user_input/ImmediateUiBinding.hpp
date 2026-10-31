#pragma once
#include "interactive/submit/PoetrySubmit.hpp"
#include "user_input/UiCommandRouter.hpp"

namespace batbox::user_input {
using ImmediateUiLoader=std::function<interactive::CommandUi::Invoke(
  const CommandEntry&,const std::string& args,
  const interactive::submit::ImmediateContext&)>;
using ImmediateUiInvoker=decltype(interactive::submit::SubmitServices::invokeImmediateUi);

// Share the same CommandUi flight as regular local commands. The loader
// supplies the real command implementation and calls Ready when its call
// completes, including when it returns no view.
ImmediateUiInvoker bindImmediateUi(event_loop::Loop& loop,
  std::shared_ptr<interactive::CommandUi> ui,ImmediateUiLoader loader);
} // namespace batbox::user_input
