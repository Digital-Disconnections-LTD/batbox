#pragma once
#include "bootstrap/CoreFlags.hpp"
#include "bootstrap/FlightCron.hpp"
#include "bootstrap/Identity.hpp"

namespace batbox::bootstrap {
struct BootstrapState {
  IdentityState identity;
  CoreFlags flags;
  FlightCronState cron;
};
}
