#include "interactive/markdown/Renderer.hpp"
#include "interactive/markdown/AnsiProjection.hpp"
#include "interactive/markdown/Blocks.hpp"
#include "interactive/PoetryXmlTags.hpp"
#include "runtime/JsonValue.hpp"
#include <stdexcept>
#include <utility>

namespace batbox::interactive::markdown {
namespace {
ftxui::Element grouped(const Tokens& tokens,const FormatServices& services,
    const WrapAnsi& wrap,int columns,bool dim){
  const auto blocks=services.blockStyle?
    std::optional<FormatServices::BlockStyle>(services.blockStyle()):std::nullopt;
  const auto prose=[&](ftxui::Element element){
    return blocks&&blocks->readingWidth>0?element|ftxui::size(ftxui::WIDTH,
      ftxui::LESS_THAN,blocks->readingWidth):element;
  };
  std::vector<ftxui::Element> elements;
  std::string nonTable;
  const auto flush=[&](){
    // Blank separators between blocks are spacing, not content.
    if(!value::trim(nonTable).empty())
      elements.push_back(prose(projectAnsi(value::trim(nonTable),dim)));
    nonTable.clear();
  };
  for(const auto& token:tokens){
    if(token.kind==Kind::Table){
      flush();
      elements.push_back(projectAnsi(renderTable(token,columns,services,wrap),dim));
    }else if(blocks&&token.kind==Kind::Code){
      flush();elements.push_back(projectCodeBlock(token,services,*blocks,columns,dim));
    }else if(blocks&&token.kind==Kind::Rule){
      flush();elements.push_back(projectRule(*blocks,columns));
    }else if(blocks&&token.kind==Kind::List){
      flush();elements.push_back(prose(projectList(token,services,*blocks,dim)));
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
// Builds the pieces of grouped(): the same elements, with plain-block runs
// cut at the newlines that can no longer move.
struct PieceContext {
  const FormatServices& services;
  const WrapAnsi& wrap;
  int columns;
  bool dim;
  std::optional<FormatServices::BlockStyle> blocks;
  ftxui::Element prose(ftxui::Element element)const{
    return blocks&&blocks->readingWidth>0?element|ftxui::size(ftxui::WIDTH,
      ftxui::LESS_THAN,blocks->readingWidth):element;
  }
  bool block(const Token& token)const{
    return token.kind==Kind::Table||(blocks&&(token.kind==Kind::Code||
      token.kind==Kind::Rule||token.kind==Kind::List));
  }
  ftxui::Element blockElement(const Token& token)const{
    if(token.kind==Kind::Table)
      return projectAnsi(renderTable(token,columns,services,wrap),dim);
    if(token.kind==Kind::Code)return projectCodeBlock(token,services,*blocks,columns,dim);
    if(token.kind==Kind::Rule)return projectRule(*blocks,columns);
    return prose(projectList(token,services,*blocks,dim));
  }
};
// Text arriving in one burst can settle as a long run; it is cut every few
// dozen lines so each piece stays about a screen tall.
constexpr std::size_t pieceLines=32;
void runPiece(PieceState& state,std::vector<MarkdownPiece>& out,
    const PieceContext& context,std::string_view text){
  std::size_t start=0,lines=0;
  const auto emit=[&](std::string_view part){
    const bool first=!state.runDrawn;
    out.push_back({context.prose(projectAnsiPart(part,context.dim,state.style)),
      first&&state.elements>0});
    if(first){state.runDrawn=true;++state.elements;}
  };
  for(std::size_t i=0;i<text.size();++i){
    if(text[i]!='\n'||++lines<pieceLines)continue;
    emit(text.substr(start,i-start));
    start=i+1;lines=0;
  }
  emit(text.substr(start));
}
// grouped() trims the run at both ends, so a line is final once text that is
// not whitespace follows it.
void settleRun(PieceState& state,std::vector<MarkdownPiece>& out,
    const PieceContext& context){
  if(!state.runOpen){
    const auto trimmed=value::trimView(state.run);
    if(trimmed.empty()){state.run.clear();return;}
    state.run.erase(0,static_cast<std::size_t>(trimmed.data()-state.run.data()));
    state.runOpen=true;
  }
  const auto trimmed=value::trimView(state.run);
  if(trimmed.empty())return;
  const auto end=static_cast<std::size_t>(trimmed.data()-state.run.data())+trimmed.size();
  const auto newline=state.run.rfind('\n',end-1);
  if(newline==std::string::npos)return;
  runPiece(state,out,context,std::string_view(state.run).substr(0,newline));
  state.run.erase(0,newline+1);
}
void closeRun(PieceState& state,std::vector<MarkdownPiece>& out,
    const PieceContext& context){
  // An open run was trimmed at its start already; only its end is trimmed.
  const auto trimmed=value::trimView(state.run);
  if(!trimmed.empty())runPiece(state,out,context,state.runOpen?
    std::string_view(state.run).substr(0,
      static_cast<std::size_t>(trimmed.data()-state.run.data())+trimmed.size()):trimmed);
  state.run.clear();state.style={};state.runOpen=false;state.runDrawn=false;
}
void addToken(PieceState& state,std::vector<MarkdownPiece>& out,
    const PieceContext& context,const Token& token){
  if(context.block(token)){
    closeRun(state,out,context);
    out.push_back({context.blockElement(token),state.elements>0});
    ++state.elements;
  }else state.run+=formatToken(token,context.services);
}
// A link reference definition changes how earlier text renders, which the
// pieces cannot follow; cmark also folds CR LF.
bool definesReference(const Token& token){
  std::size_t start=0;
  while(start<token.raw.size()){
    auto line=std::string_view(token.raw).substr(start);
    line=line.substr(0,line.find('\n'));
    std::size_t at=0;
    while(at<3&&at<line.size()&&line[at]==' ')++at;
    if(at<line.size()&&line[at]=='['&&line.find("]:",at)!=std::string_view::npos)return true;
    const auto next=token.raw.find('\n',start);
    if(next==std::string::npos)break;
    start=next+1;
  }
  return false;
}
} // namespace

void PieceState::reset(){
  stablePrefix.clear();columns=-1;dim=false;whole=false;
  settled.clear();settledBytes=0;elements=0;run.clear();style={};
  runOpen=false;runDrawn=false;prefixTail.clear();
  ++prefixVersion;prefixPiece.reset();
}

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
Tokens Renderer::settle(PieceState& state,std::string_view region,bool dim){
  const PieceContext context{services_,wrap_,columns_,dim,
    services_.blockStyle?std::optional(services_.blockStyle()):std::nullopt};
  auto tokens=lex(std::string(region));
  // Each block's source must tile the text; where a table takes over a
  // paragraph the slices can overlap, and only whole renders follow.
  std::size_t tiled=0;
  for(const auto& token:tokens){
    if(token.raw.empty()&&token.kind!=Kind::Space)state.whole=true;
    if(definesReference(token))state.whole=true;
    tiled+=token.raw.size();
  }
  if(tiled!=region.size())state.whole=true;
  if(state.whole)return {};
  // A block settles once a blank line and another block follow it: until
  // then later lines can still join it (table rows, list items, a setext
  // underline), and the pieces must match a fresh lex of all the text.
  std::ptrdiff_t last=static_cast<std::ptrdiff_t>(tokens.size())-1;
  while(last>=0&&tokens[static_cast<std::size_t>(last)].kind==Kind::Space)--last;
  while(last>0&&tokens[static_cast<std::size_t>(last-1)].kind!=Kind::Space)--last;
  std::size_t settled=0;
  for(std::ptrdiff_t i=0;i<std::max<std::ptrdiff_t>(0,last);++i){
    const auto& token=tokens[static_cast<std::size_t>(i)];
    addToken(state,state.settled,context,token);
    settled+=token.raw.size();
  }
  settleRun(state,state.settled,context);
  state.settledBytes+=settled;
  tokens.erase(tokens.begin(),tokens.begin()+std::max<std::ptrdiff_t>(0,last));
  return tokens;
}
std::vector<MarkdownPiece> Renderer::openPieces(const PieceState& state,
    const Tokens& tail,bool dim){
  const PieceContext context{services_,wrap_,columns_,dim,
    services_.blockStyle?std::optional(services_.blockStyle()):std::nullopt};
  std::vector<MarkdownPiece> pieces;
  // The open run continues into the tokens that may still change.
  PieceState open;
  open.run=state.run;open.style=state.style;open.runOpen=state.runOpen;
  open.runDrawn=state.runDrawn;open.elements=state.elements;
  for(const auto& token:tail)addToken(open,pieces,context,token);
  closeRun(open,pieces,context);
  return pieces;
}
const MarkdownPiece& Renderer::wholePrefix(PieceState& state,bool dim){
  if(!state.prefixPiece||state.prefixPieceVersion!=state.prefixVersion){
    state.prefixPiece=MarkdownPiece{render(state.stablePrefix,dim),false};
    state.prefixPieceVersion=state.prefixVersion;
  }
  return *state.prefixPiece;
}
std::vector<MarkdownPiece> Renderer::renderStreamingPieces(PieceState& state,
    const std::string& content,bool dim){
  const auto stripped=stripPoetryXMLTags(content);
  if(state.columns!=columns_||state.dim!=dim||!stripped.starts_with(state.stablePrefix)){
    state.reset();state.columns=columns_;state.dim=dim;
  }
  if(stripped.find('\r')!=std::string::npos||hasPoetryXmlTag(content))state.whole=true;
  // The stable prefix moves exactly as in renderStreaming.
  const auto boundary=state.stablePrefix.size();
  const auto tokens=lex(stripped.substr(boundary));
  std::ptrdiff_t last=static_cast<std::ptrdiff_t>(tokens.size())-1;
  while(last>=0&&tokens[static_cast<std::size_t>(last)].kind==Kind::Space)--last;
  std::size_t advance=0;
  for(std::ptrdiff_t i=0;i<last;++i)advance+=tokens[static_cast<std::size_t>(i)].raw.size();
  // render() lexes the prefix only when its opening has Markdown syntax;
  // otherwise it is one paragraph, drawn whole.
  const auto lexed=[&]{
    return !state.whole&&hasMarkdownSyntax(std::string(
      value::trimView(state.stablePrefix).substr(0,500)));
  };
  if(advance>0){
    state.stablePrefix=stripped.substr(0,boundary+advance);
    ++state.prefixVersion;
    // Pieces for the prefix: settled blocks, then the rest of the prefix
    // drawn on a copy, as render(prefix) closes it.
    if(lexed()){
      const auto rest=settle(state,std::string_view(state.stablePrefix).substr(
        state.settledBytes),dim);
      if(!state.whole)state.prefixTail=openPieces(state,rest,dim);
    }
  }
  std::vector<MarkdownPiece> pieces;
  if(!state.stablePrefix.empty()){
    if(!lexed())pieces.push_back(wholePrefix(state,dim));
    else{
      pieces=state.settled;
      pieces.insert(pieces.end(),state.prefixTail.begin(),state.prefixTail.end());
    }
  }
  const auto suffix=stripped.substr(state.stablePrefix.size());
  if(!suffix.empty())pieces.push_back({grouped(tokensFor(stripPoetryXMLTags(suffix)),
    services_,wrap_,columns_,dim),!state.stablePrefix.empty()});
  return pieces;
}
std::vector<MarkdownPiece> Renderer::renderGrowingPieces(PieceState& state,
    const std::string& content,bool dim){
  const auto stripped=stripPoetryXMLTags(content);
  if(state.columns!=columns_||state.dim!=dim||!stripped.starts_with(state.stablePrefix)){
    state.reset();state.columns=columns_;state.dim=dim;
  }
  if(stripped.find('\r')!=std::string::npos||hasPoetryXmlTag(content))state.whole=true;
  Tokens tail;
  if(!state.whole){
    const auto before=state.settledBytes;
    tail=settle(state,std::string_view(stripped).substr(before),dim);
    state.stablePrefix.append(stripped,before,state.settledBytes-before);
  }
  // render() lexes text only when its opening has Markdown syntax.
  if(state.whole||!hasMarkdownSyntax(stripped))return {{render(content,dim),false}};
  auto pieces=state.settled;
  auto open=openPieces(state,tail,dim);
  pieces.insert(pieces.end(),open.begin(),open.end());
  return pieces;
}
} // namespace batbox::interactive::markdown
