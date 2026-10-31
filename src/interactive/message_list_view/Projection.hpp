#pragma once
#include "interactive/message_list_view/Surface.hpp"
#include "interactive/message_view/Projection.hpp"

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
};
ftxui::Element projectSurface(const SurfacePlan& plan,const ProjectionImports& imports);
} // namespace batbox::interactive::message_list_view
