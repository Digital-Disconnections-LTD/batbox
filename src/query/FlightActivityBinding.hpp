#pragma once
#include "model/QueryModel.hpp"
#include "flights/Activity.hpp"
#include <memory>
#include <stdexcept>

namespace batbox::query {

// Captures the actual local flight-activity instance. QueryModel owns the
// start and cleanup call positions; OwnerModelCursor dispatches them
// onto the same query owner loop as other local state callbacks.
inline void bindFlightActivity(model::QueryServices& services,
    std::shared_ptr<flights::Activity> activity){
  if(!activity)throw std::invalid_argument("Session activity instance is required");
  services.startFlightActivity=[activity](const std::string& reason){
    activity->startFlightActivity(reason);
  };
  services.stopFlightActivity=[activity](const std::string& reason){
    activity->stopFlightActivity(reason);
  };
}

} // namespace batbox::query
