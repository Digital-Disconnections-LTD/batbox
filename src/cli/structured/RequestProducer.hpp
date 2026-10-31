#pragma once
#include "cli/structured/AsyncStream.hpp"
#include "cli/structured/ControlResponses.hpp"
#include <functional>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>

namespace batbox::cli::structured {
class RequestAbortError:public std::runtime_error {
public:
  RequestAbortError():std::runtime_error(""){}
};
struct RequestProducerServices {
  std::function<std::string()> randomUUID;
  std::function<void(const Json&)> onControlRequestSent;
};
class RequestProducer {
public:
  RequestProducer(event_loop::Loop& loop,std::shared_ptr<ControlResponses> control,
                  std::shared_ptr<AsyncStream<Json>> outbound,
                  RequestProducerServices services);
  event_loop::Deferred<Json> sendRequest(
    const Json& request,std::function<Json(const Json&)> parseResponse,
    std::stop_token signal={},
    const std::optional<std::string>& requestId={});
  void closeInput();
  bool inputClosed()const{return inputClosed_;}
private:
  event_loop::Loop& loop_;
  std::shared_ptr<ControlResponses> control_;
  std::shared_ptr<AsyncStream<Json>> outbound_;
  RequestProducerServices services_;
  bool inputClosed_=false;
};
}
