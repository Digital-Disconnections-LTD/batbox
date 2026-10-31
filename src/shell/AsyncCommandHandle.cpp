#include "shell/AsyncCommandResult.hpp"
namespace batbox::shell {
event_loop::Deferred<ExecResult> resultOnLoop(event_loop::Loop& loop,
  const CommandHandle& handle){return resultOnLoop(loop,handle.result());}
} // namespace batbox::shell
