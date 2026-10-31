#pragma once

#include "terminal_renderer/Frame.hpp"
#include "terminal_screen/StylePool.hpp"
#include "ftxui/dom/node.hpp"
#include <memory>

namespace batbox::terminal_ftxui {

// Projects one existing Batbox frame into FTXUI's render tree. The caller
// retains ownership of the frame and its pools until the next draw completes.
ftxui::Element projectFrame(const terminal_renderer::Frame& frame,
                            std::shared_ptr<terminal_screen::StylePool> styles);

} // namespace batbox::terminal_ftxui
