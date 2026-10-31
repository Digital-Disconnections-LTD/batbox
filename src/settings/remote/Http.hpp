#pragma once
#include "event_loop/Deferred.hpp"
#include "runtime/JsonValue.hpp"
#include <optional>
#include <stdexcept>
#include <string>

namespace batbox::settings::remote {
struct HttpResponse {int status=0;Json data=nullptr;};
struct HttpError:std::runtime_error {
  std::string kind;
  std::optional<int> status;
  HttpError(std::string message,std::string kind,
    std::optional<int> status=std::nullopt):
    std::runtime_error(std::move(message)),kind(std::move(kind)),status(status){}
};
event_loop::Deferred<HttpResponse> indigenousHttpGet(event_loop::Loop& loop,
  const std::string& url,const Json& headers,double timeoutMs);
}
