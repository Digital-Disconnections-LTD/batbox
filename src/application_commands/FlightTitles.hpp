#pragma once
#include "application_flights/Flight.hpp"
#include "application_model/LocalModelService.hpp"
#include "event_loop/Deferred.hpp"
#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace batbox::application_commands {
using FlightTitleGenerator=std::function<
  event_loop::Deferred<std::optional<std::string>>(const std::string& description)>;
// Asks the Flight's current model for a short (3-7 word) title through a
// non-streaming side query. Any backend or parsing failure resolves to
// nullopt so callers can stay silent.
FlightTitleGenerator makeFlightTitleGenerator(event_loop::Loop& owner,
  std::shared_ptr<application_model::LocalModelService> model,
  std::function<std::string()> currentModel,bool nonInteractive,
  std::function<void(const std::string&)> log);
// Reads {"title": "..."} from a model reply, tolerating code fences, prose
// around the JSON object, or a bare one-line title.
std::optional<std::string> parseGeneratedTitle(const std::string& reply);
// Queues a custom-title, ai-title or agent-name line for the current Flight
// and stashes it so resume and clean shutdown re-append it near the end.
event_loop::Deferred<event_loop::Unit> saveFlightMetadataTitle(
  application_flights::Flight& flight,const std::string& type,const std::string& title);
}
