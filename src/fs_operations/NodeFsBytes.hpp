#pragma once
#include "event_loop/Deferred.hpp"
#include "runtime/JsonValue.hpp"
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace batbox::fs_operations {
using ByteBuffer=std::vector<std::uint8_t>;
struct AsyncByteHandle {
  std::function<event_loop::Deferred<Json>()> stat;
  // The imported read operation writes into buffer before its promise settles.
  std::function<event_loop::Deferred<Json>(
    const std::shared_ptr<ByteBuffer>& buffer,std::size_t offset,
    std::size_t length,std::size_t position)> read;
  std::function<event_loop::Deferred<event_loop::Unit>()> close;
};
struct NodeFsBytesImports {
  event_loop::Loop* loop=nullptr;
  std::function<event_loop::Deferred<std::shared_ptr<AsyncByteHandle>>(
    const std::string&,const std::string&)> open;
  std::function<double(const Json&)> toNumber;
};
}
