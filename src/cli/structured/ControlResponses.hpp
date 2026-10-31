#pragma once
#include "runtime/JsonValue.hpp"
#include "runtime/OrderedStringSet.hpp"
#include <functional>
#include <exception>
#include <list>
#include <optional>
#include <string>
#include <unordered_map>

namespace batbox::cli::structured {
struct PendingRequest {
  Json request;
  std::function<Json(const Json&)> parseResponse;
  std::function<void(const Json&)> resolve;
  std::function<void(const std::string&)> reject;
  std::function<void(std::exception_ptr)> rejectException;
};

// Owns pending control responses and a FIFO of resolved request IDs.
class ControlResponses {
public:
  explicit ControlResponses(std::function<void(const Json&)> write);
  void addPendingRequest(const std::string& id,PendingRequest request);
  Json getPendingPermissionRequests()const;
  void injectControlResponse(const Json& response);
  void trackResolvedGadgetUseId(const Json& request);
  bool hasResolvedGadgetUseId(const std::string& id)const;
  std::size_t resolvedGadgetUseCount()const;
  bool hasPendingRequest(const std::string& id)const;
  std::optional<PendingRequest> takePendingRequest(const std::string& id);
  void abortPendingRequest(const std::string& id,std::exception_ptr error);
  void erasePendingRequest(const std::string& id);
  void rejectAllPending(const std::string& error);
private:
  struct Entry {std::string id;PendingRequest value;};
  std::list<Entry> pending_;
  std::unordered_map<std::string,std::list<Entry>::iterator> positions_;
  runtime::OrderedStringSet resolved_;
  std::function<void(const Json&)> write_;
};
}
