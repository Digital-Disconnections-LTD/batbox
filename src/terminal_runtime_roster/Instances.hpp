#pragma once
#include "terminal_runtime/TerminalControls.hpp"
#include <memory>
#include <unordered_map>

namespace batbox::terminal_runtime_roster {

// One shared map retains stream keys and terminal values until erase.
// Shared pointer hashing compares the stream object address.
template<class Stream,class Instance> class Instances {
public:
  using StreamRef=std::shared_ptr<Stream>;
  using InstanceRef=std::shared_ptr<Instance>;

  InstanceRef get(const StreamRef& stream)const {
    const auto it=entries_.find(stream);
    return it==entries_.end()?InstanceRef{}:it->second;
  }
  Instances& set(StreamRef stream,InstanceRef instance){
    entries_.insert_or_assign(std::move(stream),std::move(instance));
    return *this;
  }
  bool erase(const StreamRef& stream){return entries_.erase(stream)!=0;}
  std::size_t size()const{return entries_.size();}
private:
  std::unordered_map<StreamRef,InstanceRef> entries_;
};

using TerminalInstances=Instances<terminal_runtime::OutputPort,
  terminal_runtime::TerminalControls>;

// Erase the output stream's instance on unmount.
// The key remains alive until this closure
// and any other application reference are released.
inline std::function<void()> bindDeleteInstance(
    TerminalInstances& instances,
    std::shared_ptr<terminal_runtime::OutputPort> stdout){
  return [&instances,stdout=std::move(stdout)](){instances.erase(stdout);};
}

} // namespace batbox::terminal_runtime_roster
