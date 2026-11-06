#pragma once
#include "app_state/State.hpp"

namespace batbox::app_state {
using AppStateUpdater=std::function<AppStateRef(const AppStateRef&)>;
} // namespace batbox::app_state
