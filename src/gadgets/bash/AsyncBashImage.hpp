#pragma once
#include "event_loop/Deferred.hpp"
#include <cstdint>
#include <functional>
#include <optional>
#include <string>

namespace batbox::bash_gadget {
struct BashImageTransform {std::string buffer,mediaType;};
struct AsyncBashImageOps {
  std::function<event_loop::Deferred<std::uintmax_t>(const std::string&)> stat;
  std::function<event_loop::Deferred<std::string>(const std::string&)> readFile;
  std::function<event_loop::Deferred<BashImageTransform>(const std::string&,
    const std::string&)> resizeBuffer;
};
// Resize shell image output through owner-loop continuations. Filesystem and
// image-processing services are injected.
event_loop::Deferred<std::optional<std::string>> resizeShellImageOutputAsync(
  event_loop::Loop& loop,std::string stdoutText,
  std::optional<std::string> outputFilePath,
  std::optional<std::uintmax_t> outputFileSize,const AsyncBashImageOps& operations);
// Indigenous imports use real FS workers and the media image pipeline. Link this
// binding in a target that also links batbox_media to avoid a shell/media cycle.
AsyncBashImageOps indigenousBashImageOps(event_loop::Loop& loop);
std::function<event_loop::Deferred<std::optional<std::string>>(
  const std::string&,const std::optional<std::string>&,
  const std::optional<std::uintmax_t>&)> indigenousBashImageCallback(event_loop::Loop& loop);
} // namespace batbox::bash_gadget
