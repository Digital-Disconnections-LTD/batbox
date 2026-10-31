#pragma once
#include "fs_operations/NodeFsAsync.hpp"
#include "fs_operations/NodeFsBytes.hpp"
#include "fs_operations/NodeFsSync.hpp"
#include <functional>
#include <memory>
#include <string>
#include <string_view>

namespace batbox::fs_operations {
// The application supplies the writable-stream implementation.
class WritableStream {
public:
  virtual ~WritableStream()=default;
  virtual bool write(std::string_view bytes)=0;
  virtual void end()=0;
  virtual void abort()=0;
  virtual void onFinish(std::function<void()> callback)=0;
  virtual void onError(std::function<void(const std::exception_ptr&)> callback)=0;
  virtual void onDrain(std::function<void()> callback)=0;
  virtual void onClose(std::function<void()> callback)=0;
};
struct FsImplementation {
  std::shared_ptr<NodeFsSync> sync;
  std::shared_ptr<NodeFsAsync> async;
  std::shared_ptr<NodeFsBytes> bytes;
  std::function<std::shared_ptr<WritableStream>(const std::string&)>
    createWriteStream;
};
class FsRoster {
public:
  explicit FsRoster(std::shared_ptr<FsImplementation> original);
  void setFsImplementation(std::shared_ptr<FsImplementation> implementation);
  std::shared_ptr<FsImplementation> getFsImplementation()const;
  void setOriginalFsImplementation();
private:
  std::shared_ptr<FsImplementation> original_;
  std::shared_ptr<FsImplementation> active_;
};
std::shared_ptr<WritableStream> createWriteStream(
  const FsImplementation& implementation,const std::string& path);
}
