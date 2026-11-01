#include "interactive/markdown/Renderer.hpp"
#include "interactive/markdown/AnsiProjection.hpp"
#include "runtime/JsonValue.hpp"
#include <stdexcept>
#include <utility>

namespace batbox::interactive::markdown {
namespace {
ftxui::Element grouped(const Tokens& tokens,const FormatServices& services,
    const WrapAnsi& wrap,int columns,bool dim){
  std::vector<ftxui::Element> elements;
  std::string nonTable;
  const auto flush=[&](){
    if(!nonTable.empty()){
      elements.push_back(projectAnsi(value::trim(nonTable),dim));
      nonTable.clear();
    }
  };
  for(const auto& token:tokens){
    if(token.kind==Kind::Table){
      flush();
      elements.push_back(projectAnsi(renderTable(token,columns,services,wrap),dim));
    }else nonTable+=formatToken(token,services);
  }
  flush();
  std::vector<ftxui::Element> spaced;
  for(auto& element:elements){
    if(!spaced.empty())spaced.push_back(ftxui::text(""));
    spaced.push_back(std::move(element));
  }
  return ftxui::vbox(std::move(spaced));
}
} // namespace

Renderer::Renderer(FormatServices services,WrapAnsi wrap,int terminalColumns):
    services_(std::move(services)),wrap_(std::move(wrap)),columns_(terminalColumns){
  if(columns_<0)throw std::invalid_argument("Terminal columns must be nonnegative");
}
ftxui::Element Renderer::render(const std::string& content,bool dim){
  const auto stripped=stripPoetryXMLTags(content);
  return grouped(stash_.get(stripped),services_,wrap_,columns_,dim);
}
ftxui::Element Renderer::renderStreaming(StreamingState& state,
    const std::string& content,bool dim){
  const auto stripped=stripPoetryXMLTags(content);
  if(!stripped.starts_with(state.stablePrefix))state.stablePrefix.clear();
  const auto boundary=state.stablePrefix.size();
  const auto tokens=lex(stripped.substr(boundary));
  std::ptrdiff_t last=static_cast<std::ptrdiff_t>(tokens.size())-1;
  while(last>=0&&tokens[static_cast<std::size_t>(last)].kind==Kind::Space)--last;
  std::size_t advance=0;
  for(std::ptrdiff_t i=0;i<last;++i)
    advance+=tokens[static_cast<std::size_t>(i)].raw.size();
  if(advance>0)state.stablePrefix=stripped.substr(0,boundary+advance);
  const auto suffix=stripped.substr(state.stablePrefix.size());
  std::vector<ftxui::Element> elements;
  if(!state.stablePrefix.empty()){
    if(!state.prefixElement||state.prefixColumns!=columns_||state.prefixDim!=dim||
        state.renderedPrefix!=state.stablePrefix){
      state.prefixElement=render(state.stablePrefix,dim);
      state.renderedPrefix=state.stablePrefix;
      state.prefixColumns=columns_;state.prefixDim=dim;
    }
    elements.push_back(state.prefixElement);
  }
  // The suffix changes every frame; caching it would only evict settled text.
  if(!suffix.empty())elements.push_back(grouped(tokensFor(stripPoetryXMLTags(suffix)),
    services_,wrap_,columns_,dim));
  if(elements.size()==2)return ftxui::vbox({std::move(elements[0]),
    ftxui::text(""),std::move(elements[1])});
  return ftxui::vbox(std::move(elements));
}
} // namespace batbox::interactive::markdown
