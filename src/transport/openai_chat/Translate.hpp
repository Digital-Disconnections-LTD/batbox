#pragma once
#include "runtime/JsonValue.hpp"
#include "transport/Errors.hpp"
#include "transport/WireClient.hpp"
#include <map>
#include <set>
#include <string>

namespace batbox::transport::openai_chat {

// Ids sent to the server for the tool_use ids of one request. "any" keeps
// Anthropic ids (shortened past 40 characters); "short" sends exactly nine
// letters and digits, which Mistral-family servers require.
class ToolIdMap {
public:
  explicit ToolIdMap(std::string style = "any");
  std::string outgoing(const std::string& anthropicId);
private:
  std::string style_;
  std::map<std::string, std::string> ids_;
  std::set<std::string> used_;
};

// A tool_use id for a call the server named: kept when it is 8 to 40 of
// [A-Za-z0-9_-] and unused in this response, otherwise a fresh toolu_ id.
std::string incomingToolId(const std::string& serverId, std::set<std::string>& seen);
std::string randomId(const std::string& prefix);

// Anthropic Messages request body -> Chat Completions request body.
Json translateRequest(const Json& anthropicBody, bool stream, const ChatQuirks& quirks);
// Chat Completions response body -> Anthropic Messages response body.
// model is echoed so resume can match the profile.
Json translateResponse(const Json& chatBody, const std::string& model);
// Chat Completions usage -> Anthropic usage (always has input_tokens and
// output_tokens).
Json translateUsage(const Json& usage);
std::string stopReason(const std::string& finishReason, bool sawToolCall);
// Non-2xx reply or in-stream error object -> APIError carrying
// {"type":"error","error":{"type","message"}} with the status kept.
APIError translateError(std::optional<long> status, const Json& body, const std::string& raw,
                        const Json& headers, const std::string& baseUrl);
std::string anthropicErrorType(std::optional<long> status, const Json& body);

} // namespace batbox::transport::openai_chat
