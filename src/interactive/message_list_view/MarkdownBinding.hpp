#pragma once
#include "interactive/message_list_view/Projection.hpp"
#include "interactive/markdown/Renderer.hpp"
#include <memory>

namespace batbox::interactive::message_list_view {
void bindMarkdown(ProjectionImports& target,std::shared_ptr<markdown::Renderer> renderer,
  std::shared_ptr<markdown::StreamingState> streaming);
} // namespace batbox::interactive::message_list_view
