#pragma once
#include "fs_operations/FsRoster.hpp"
#include "fs_operations/NodeFsAsyncIndigenous.hpp"
#include <memory>

namespace batbox::fs_operations {
class IndigenousWritableStream final:public WritableStream {
public:
  struct State;
  IndigenousWritableStream(event_loop::Loop& loop,
    std::shared_ptr<IndigenousAsyncFsHost> host,std::string path);
  bool write(std::string_view bytes)override;
  void end()override;
  void abort()override;
  void onFinish(std::function<void()> callback)override;
  void onError(std::function<void(const std::exception_ptr&)> callback)override;
  void onDrain(std::function<void()> callback)override;
  void onClose(std::function<void()> callback)override;
  void start();
private:
  std::shared_ptr<State> state_;
};
std::shared_ptr<WritableStream> createIndigenousWritableStream(
  event_loop::Loop& loop,std::shared_ptr<IndigenousAsyncFsHost> host,
  const std::string& path);
void bindIndigenousCreateWriteStream(FsImplementation& implementation,
  event_loop::Loop& loop,std::shared_ptr<IndigenousAsyncFsHost> host);
}
