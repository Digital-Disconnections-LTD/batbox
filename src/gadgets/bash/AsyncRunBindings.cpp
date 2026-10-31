#include "gadgets/bash/AsyncRunShellCommand.hpp"
#include "shell/LoopMissionOutput.hpp"
#include <utility>

namespace batbox::bash_gadget {
std::function<event_loop::Deferred<AsyncShellCommand>(const AsyncExecRequest&)>
bindManagedExec(event_loop::Loop& loop,std::shared_ptr<shell::ManagedShell> shell,
  shell::RunOptions baseOptions){
  if(!shell)throw std::invalid_argument("managed shell required");
  return [&loop,shell=std::move(shell),baseOptions=std::move(baseOptions)](
    const AsyncExecRequest& request){
    auto options=baseOptions;
    options.command=request.command;options.timeoutMs=request.timeoutMs;
    options.preventCwdChanges=request.preventCwdChanges;
    options.useSandbox=request.useSandbox;
    options.autoBackgroundEnabled=request.shouldAutoBackground;
    return execManagedOnLoop(loop,shell,std::move(options),request.shouldAutoBackground);
  };
}
std::function<std::function<void()>(const AsyncShellCommand&,
  std::function<void(const shell::PollSample&)>,
  std::function<void(std::exception_ptr)>)>
bindMissionOutputPoller(event_loop::Loop& loop,shell::MissionOutputPoller& poller){
  return [&loop,&poller](const AsyncShellCommand& command,
    std::function<void(const shell::PollSample&)> progress,
    std::function<void(std::exception_ptr)> error){
    auto binding=std::make_shared<shell::LoopMissionOutput>(loop,poller,
      command.missionId,command.outputPath,std::move(progress),std::move(error));
    binding->start();
    return std::function<void()>([binding]{binding->stop();});
  };
}
} // namespace batbox::bash_gadget
