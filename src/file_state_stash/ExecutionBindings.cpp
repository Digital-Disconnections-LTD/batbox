#include "file_state_stash/ExecutionBindings.hpp"

namespace batbox::file_state_stash {
void bindExecutionStash(execution::Context& target,
  const query_engine::ProcessContext& source){
  target.readFileState=source.readFileState;
}
} // namespace batbox::file_state_stash
