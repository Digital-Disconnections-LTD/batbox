#pragma once
#include "instructions/hooks/runtime/FlightEnvironment.hpp"

namespace batbox::instructions::hooks::runtime {
struct FlightEnvironment::State {
  event_loop::Loop& loop;
  FlightEnvironmentImports imports;
  std::optional<std::optional<std::string>> script;
  State(event_loop::Loop& owner,FlightEnvironmentImports services):
    loop(owner),imports(std::move(services)){}
};
std::string joinFlightPath(const std::string& first,const std::string& second);
} // namespace batbox::instructions::hooks::runtime
