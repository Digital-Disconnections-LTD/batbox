#include "flight_restore/WorktreeFlightState.hpp"

namespace batbox::flight_restore {
Json getCurrentWorktreeFlight(const WorktreeFlightState& state){return state.current;}
void restoreWorktreeFlight(WorktreeFlightState& state,const Json& flight){
  state.current=flight;
}
} // namespace batbox::flight_restore
