#pragma once
#include "terminal_renderer/Frame.hpp"
#include <memory>

namespace batbox::terminal_optimizer {
using DiffRef=std::shared_ptr<terminal_renderer::Diff>;
// Return the input array unchanged for length zero or one.
DiffRef optimize(const DiffRef& diff);
} // namespace batbox::terminal_optimizer
