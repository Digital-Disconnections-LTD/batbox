#include "services/api/batbox.hpp"
namespace batbox {
const Json EMPTY_USAGE = {
  {"input_tokens", 0}, {"cache_creation_input_tokens", 0}, {"cache_read_input_tokens", 0}, {"output_tokens", 0},
  {"server_tool_use", {{"web_search_requests", 0}, {"web_fetch_requests", 0}}},
  {"service_tier", "standard"},
  {"cache_creation", {{"ephemeral_1h_input_tokens", 0}, {"ephemeral_5m_input_tokens", 0}}},
  {"inference_geo", ""}, {"iterations", Json::array()}, {"speed", "standard"}
};
} // namespace batbox
