#include "bootstrap/BootstrapState.hpp"
#include "runtime/Platform.hpp"
#include <cassert>
#include <filesystem>
#include <regex>

int main(){
  auto services=batbox::bootstrap::makeIndigenousBootstrapInitialServices();
  const auto before=batbox::platform::nowMillis();
  auto state=batbox::bootstrap::initialBootstrapState(services);
  const auto after=batbox::platform::nowMillis();
  const auto expected=std::filesystem::canonical(
    std::filesystem::current_path()).string();
  assert(state.identity.directories.cwd==expected);
  assert(state.identity.directories.originalCwd==expected);
  assert(state.identity.directories.projectRoot==expected);
  assert(state.costs.startTime>=before&&state.costs.startTime<=after);
  assert(state.interaction->lastInteractionTime>=before&&
    state.interaction->lastInteractionTime<=after);
  assert(std::regex_match(state.identity.flightId,std::regex(
    "[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-"
    "[0-9a-f]{12}")));
  return 0;
}
