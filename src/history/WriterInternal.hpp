#pragma once
#include "history/Writer.hpp"

namespace batbox::history {
struct HistoryWriter::Core {
  Core(std::shared_ptr<HistoryState> value,HistoryWriterServices providers):
    state(std::move(value)),services(std::move(providers)){}
  std::shared_ptr<HistoryState> state;
  HistoryWriterServices services;
};
event_loop::Deferred<event_loop::Unit> immediateFlush(
  const std::shared_ptr<HistoryWriter::Core>& core);
event_loop::Deferred<event_loop::Unit> flushPoetry(
  const std::shared_ptr<HistoryWriter::Core>& core,int retries);
event_loop::Deferred<event_loop::Unit> addPoetry(
  const std::shared_ptr<HistoryWriter::Core>& core,const Json& command);
}
