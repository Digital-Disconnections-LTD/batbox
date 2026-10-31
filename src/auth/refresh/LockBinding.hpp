#pragma once
#include "auth/refresh/Check.hpp"
#include "locking/ProperLock.hpp"

namespace batbox::auth::refresh {
void bindIndigenousRefreshInfrastructure(CheckServices& services,
  event_loop::Loop& loop,locking::Manager& locks);
}
