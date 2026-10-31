#include "interactive/message_list_view/MarkdownBinding.hpp"
#include <stdexcept>

namespace batbox::interactive::message_list_view {
void bindMarkdown(ProjectionImports& target,std::shared_ptr<markdown::Renderer> renderer,
    std::shared_ptr<markdown::StreamingState> streaming){
  if(!renderer||!streaming)throw std::invalid_argument("Indigenous Markdown renderer/state required");
  target.messages.assistant.projection.renderMarkdown=[renderer](const std::string& text,bool dim){
    return renderer->render(text,dim);
  };
  target.renderStreamingMarkdown=[renderer,streaming](const std::string& text,bool dim){
    return renderer->renderStreaming(*streaming,text,dim);
  };
}
} // namespace batbox::interactive::message_list_view
