#pragma once
#include "diagnostics/SlowOperations.hpp"

namespace batbox::diagnostics {
// Bind synchronous open, write, flush, and close operations
// to the local POSIX filesystem. Write orchestration stays in
// SlowOperations::writeFileSyncDeprecated.
void bindIndigenousSlowFileOperations(SlowOperationImports& imports);
}
