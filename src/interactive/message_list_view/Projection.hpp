#pragma once
#include "interactive/message_list_view/Surface.hpp"
#include "interactive/message_view/Projection.hpp"
#include "interactive/markdown/Renderer.hpp"

namespace batbox::interactive::message_list_view {
enum class DividerKind {ShowPrevious,HidePrevious,Unseen};
struct DividerRequest {DividerKind kind;std::ptrdiff_t count;int columns;};
using RowRenderer=std::function<ftxui::Element(std::size_t)>;
struct ProjectionImports {
  message_view::ProjectionServices messages;
  std::function<ftxui::Element(const std::string&,bool)> renderStreamingMarkdown;
  std::function<ftxui::Element()> renderLogo;
  std::function<ftxui::Element(const DividerRequest&)> renderDivider;
  std::function<ftxui::Element(const SurfacePlan&,const RowRenderer&)>
    renderVirtualList;
  // The live answer and reasoning as pieces, one row each; without them the
  // answer is one row from renderStreamingMarkdown.
  const std::vector<markdown::MarkdownPiece>* responsePieces=nullptr;
  const std::vector<markdown::MarkdownPiece>* thinkingPieces=nullptr;
  // A queued poetry row; the first row of the stack carries the margin.
  std::function<ftxui::Element(const QueuedInput&,bool margin,int columns)>
    renderQueuedInput;
};
ftxui::Element projectSurface(const SurfacePlan& plan,const ProjectionImports& imports);
} // namespace batbox::interactive::message_list_view
