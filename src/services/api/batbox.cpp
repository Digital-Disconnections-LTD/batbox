#include "services/api/batbox.hpp"
namespace batbox {
namespace {
Json nullish(const Json& value, const Json& fallback) { return value.is_null() ? fallback : value; }
Json positive(const Json& value, const Json& fallback) { return value.is_number() && value.get<double>() > 0 ? value : fallback; }
Json nested(const Json& value, const char* parent, const char* field) { return value::property(value::property(value,parent),field); }
double sum(const Json& a, const Json& b, const char* field) { return a.at(field).get<double>() + b.at(field).get<double>(); }
} // namespace
Json updateUsage(const Json& usage, const Json& partUsage, bool stashedMicrocompact) {
  if (!value::truthy(partUsage)) return usage;
  Json result = {
    {"input_tokens", positive(value::property(partUsage,"input_tokens"), usage.at("input_tokens"))},
    {"cache_creation_input_tokens", positive(value::property(partUsage,"cache_creation_input_tokens"), usage.at("cache_creation_input_tokens"))},
    {"cache_read_input_tokens", positive(value::property(partUsage,"cache_read_input_tokens"), usage.at("cache_read_input_tokens"))},
    {"output_tokens", nullish(value::property(partUsage,"output_tokens"),usage.at("output_tokens"))},
    {"server_tool_use", {
      {"web_search_requests", nullish(nested(partUsage,"server_tool_use","web_search_requests"),usage.at("server_tool_use").at("web_search_requests"))},
      {"web_fetch_requests", nullish(nested(partUsage,"server_tool_use","web_fetch_requests"),usage.at("server_tool_use").at("web_fetch_requests"))}
    }},
    {"service_tier", usage.at("service_tier")},
    {"cache_creation", {
      {"ephemeral_1h_input_tokens", nullish(nested(partUsage,"cache_creation","ephemeral_1h_input_tokens"),usage.at("cache_creation").at("ephemeral_1h_input_tokens"))},
      {"ephemeral_5m_input_tokens", nullish(nested(partUsage,"cache_creation","ephemeral_5m_input_tokens"),usage.at("cache_creation").at("ephemeral_5m_input_tokens"))}
    }}
  };
  if (stashedMicrocompact) result["cache_deleted_input_tokens"] = positive(value::property(partUsage,"cache_deleted_input_tokens"),nullish(value::property(usage,"cache_deleted_input_tokens"),0));
  result["inference_geo"] = usage.at("inference_geo");
  result["iterations"] = nullish(value::property(partUsage,"iterations"),usage.at("iterations"));
  result["speed"] = nullish(value::property(partUsage,"speed"),usage.at("speed"));
  return result;
}
Json accumulateUsage(const Json& totalUsage, const Json& messageUsage, bool stashedMicrocompact) {
  Json result = {
    {"input_tokens", sum(totalUsage,messageUsage,"input_tokens")},
    {"cache_creation_input_tokens", sum(totalUsage,messageUsage,"cache_creation_input_tokens")},
    {"cache_read_input_tokens", sum(totalUsage,messageUsage,"cache_read_input_tokens")},
    {"output_tokens", sum(totalUsage,messageUsage,"output_tokens")},
    {"server_tool_use", {
      {"web_search_requests", sum(totalUsage.at("server_tool_use"),messageUsage.at("server_tool_use"),"web_search_requests")},
      {"web_fetch_requests", sum(totalUsage.at("server_tool_use"),messageUsage.at("server_tool_use"),"web_fetch_requests")}
    }},
    {"service_tier", messageUsage.at("service_tier")},
    {"cache_creation", {
      {"ephemeral_1h_input_tokens", sum(totalUsage.at("cache_creation"),messageUsage.at("cache_creation"),"ephemeral_1h_input_tokens")},
      {"ephemeral_5m_input_tokens", sum(totalUsage.at("cache_creation"),messageUsage.at("cache_creation"),"ephemeral_5m_input_tokens")}
    }}
  };
  if (stashedMicrocompact) result["cache_deleted_input_tokens"] = value::numberOr(value::property(totalUsage,"cache_deleted_input_tokens")) + value::numberOr(value::property(messageUsage,"cache_deleted_input_tokens"));
  result["inference_geo"] = messageUsage.at("inference_geo");
  result["iterations"] = messageUsage.at("iterations");
  result["speed"] = messageUsage.at("speed");
  return result;
}
} // namespace batbox
