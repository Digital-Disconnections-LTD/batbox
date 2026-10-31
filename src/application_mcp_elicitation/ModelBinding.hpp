#pragma once
#include "application_mcp_elicitation/DateTimeParser.hpp"
#include "application_model/LocalModelService.hpp"

namespace batbox::application_mcp_elicitation {
// Uses the configured small-model profile and the existing cancellation-aware
// Messages client. No separate vendor account or hidden fallback endpoint.
DateTimeQuery makeDateTimeQuery(event_loop::Loop& owner,
  std::shared_ptr<application_model::LocalModelService> model);
}
