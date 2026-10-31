#pragma once
#include "cli/structured/ProcessLine.hpp"
#include "cli/structured/RequestProducer.hpp"
#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace batbox::cli::structured {
struct InputReaderServices {
  std::function<event_loop::Deferred<std::optional<std::string>>()> nextChunk;
  std::function<void(const std::string&)> logParsedMessageType;
  ProcessLineServices processLine;
};

class InputReader:public std::enable_shared_from_this<InputReader> {
public:
  InputReader(event_loop::Loop& loop,std::shared_ptr<ControlResponses> control,
              std::shared_ptr<RequestProducer> producer,
              InputReaderServices services);
  void prependUserMessage(const std::string& content);
  event_loop::Deferred<StreamItem<Json>> next();
private:
  void pump();
  void consumeLine(std::string line,bool fromSplit);
  void deliver(Json message,bool fromSplit);
  void finish();
  void fail(std::exception_ptr error);
  event_loop::Loop& loop_;
  std::shared_ptr<ControlResponses> control_;
  std::shared_ptr<RequestProducer> producer_;
  InputReaderServices services_;
  std::deque<event_loop::Deferred<StreamItem<Json>>> waiting_;
  std::vector<std::string> prependedLines_;
  std::string content_;
  bool splitPhase_=true,eof_=false,tailProcessed_=false;
  bool processing_=false,closed_=false;
};
}
