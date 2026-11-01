#include "interactive/markdown/Lexer.hpp"
#include <cmark-gfm.h>
#include <cmark-gfm-extension_api.h>
#include <cmark-gfm-core-extensions.h>
#include <algorithm>
#include <memory>
#include <stdexcept>

namespace batbox::interactive::markdown {
namespace {
std::string string(const char* value){return value?value:"";}
Tokens childrenOf(cmark_node* parent);
Token fromNode(cmark_node* node){
  Token token;
  const auto kind=cmark_node_get_type(node);
  const auto type=string(cmark_node_get_type_string(node));
  token.text=string(cmark_node_get_literal(node));
  if(type=="table"){
    token.kind=Kind::Table;
    if(auto* align=cmark_gfm_extensions_get_table_alignments(node)){
      const auto columns=cmark_gfm_extensions_get_table_columns(node);
      for(std::size_t i=0;i<columns;++i){
        token.align.push_back(align[i]=='c'?"center":
          align[i]=='r'?"right":"left");
      }
    }
    for(auto* row=cmark_node_first_child(node);row;
        row=cmark_node_next(row)){
      std::vector<std::vector<Token>> cells;
      for(auto* cell=cmark_node_first_child(row);cell;
          cell=cmark_node_next(cell))cells.push_back(childrenOf(cell));
      if(cmark_gfm_extensions_get_table_row_is_header(row))
        token.header=std::move(cells);
      else token.rows.push_back(std::move(cells));
    }
    return token;
  }
  switch(kind){
  case CMARK_NODE_BLOCK_QUOTE:token.kind=Kind::BlockQuote;break;
  case CMARK_NODE_CODE_BLOCK:
    token.kind=Kind::Code;
    token.language=string(cmark_node_get_fence_info(node));break;
  case CMARK_NODE_CODE:token.kind=Kind::CodeSpan;break;
  case CMARK_NODE_EMPH:token.kind=Kind::Emphasis;break;
  case CMARK_NODE_STRONG:token.kind=Kind::Strong;break;
  case CMARK_NODE_HEADING:
    token.kind=Kind::Heading;
    token.depth=cmark_node_get_heading_level(node);break;
  case CMARK_NODE_THEMATIC_BREAK:token.kind=Kind::Rule;break;
  case CMARK_NODE_IMAGE:
    token.kind=Kind::Image;token.href=string(cmark_node_get_url(node));break;
  case CMARK_NODE_LINK:
    token.kind=Kind::Link;token.href=string(cmark_node_get_url(node));break;
  case CMARK_NODE_LIST:
    token.kind=Kind::List;token.ordered=cmark_node_get_list_type(node)==CMARK_ORDERED_LIST;
    token.start=cmark_node_get_list_start(node);break;
  case CMARK_NODE_ITEM:token.kind=Kind::ListItem;break;
  case CMARK_NODE_PARAGRAPH:token.kind=Kind::Paragraph;break;
  case CMARK_NODE_LINEBREAK:token.kind=Kind::Break;break;
  case CMARK_NODE_SOFTBREAK:token.kind=Kind::Text;token.text="\n";break;
  case CMARK_NODE_TEXT:token.kind=Kind::Text;break;
  case CMARK_NODE_HTML_INLINE:
  case CMARK_NODE_HTML_BLOCK:token.kind=Kind::Html;break;
  default:token.kind=Kind::Text;break;
  }
  if(kind==CMARK_NODE_LIST){
    for(auto* child=cmark_node_first_child(node);child;
        child=cmark_node_next(child))token.items.push_back(fromNode(child));
  }else token.children=childrenOf(node);
  return token;
}
Tokens childrenOf(cmark_node* parent){
  Tokens tokens;
  for(auto* child=cmark_node_first_child(parent);child;
      child=cmark_node_next(child))tokens.push_back(fromNode(child));
  return tokens;
}
std::string normalizeLines(const std::string& input){
  std::string result;result.reserve(input.size());
  for(std::size_t i=0;i<input.size();++i){
    if(input[i]=='\r'){
      if(i+1<input.size()&&input[i+1]=='\n')++i;
      result+='\n';
    }else result+=input[i];
  }
  return result;
}
} // namespace

Tokens lex(const std::string& source){
  const auto input=normalizeLines(source);
  cmark_gfm_core_extensions_ensure_registered();
  std::unique_ptr<cmark_parser,decltype(&cmark_parser_free)> parser(
    cmark_parser_new(CMARK_OPT_DEFAULT),&cmark_parser_free);
  if(!parser)throw std::runtime_error("Markdown parser unavailable");
  for(const char* name:{"table","autolink","tasklist"}){
    auto* extension=cmark_find_syntax_extension(name);
    if(!extension||!cmark_parser_attach_syntax_extension(parser.get(),extension))
      throw std::runtime_error("Markdown syntax extension unavailable");
  }
  cmark_parser_feed(parser.get(),input.data(),input.size());
  std::unique_ptr<cmark_node,decltype(&cmark_node_free)> document(
    cmark_parser_finish(parser.get()),&cmark_node_free);
  if(!document)throw std::runtime_error("Markdown parse failed");
  std::vector<std::size_t> lineStarts{0};
  for(std::size_t i=0;i<input.size();++i)
    if(input[i]=='\n')lineStarts.push_back(i+1);
  std::vector<std::pair<cmark_node*,std::size_t>> blocks;
  for(auto* node=cmark_node_first_child(document.get());node;
      node=cmark_node_next(node)){
    const auto line=cmark_node_get_start_line(node);
    const auto index=line>0?static_cast<std::size_t>(line-1):0;
    blocks.emplace_back(node,lineStarts[std::min(index,lineStarts.size()-1)]);
  }
  Tokens result;
  if(blocks.empty()){
    if(!input.empty()){
      Token space;space.kind=Kind::Space;space.raw=input;
      result.push_back(std::move(space));
    }
    return result;
  }
  if(blocks.front().second>0){
    Token space;space.kind=Kind::Space;
    space.raw=input.substr(0,blocks.front().second);
    result.push_back(std::move(space));
  }
  for(std::size_t i=0;i<blocks.size();++i){
    auto token=fromNode(blocks[i].first);
    const auto start=blocks[i].second;
    const auto end=i+1<blocks.size()?blocks[i+1].second:input.size();
    token.raw=input.substr(start,end-start);
    std::string separator;
    if(token.kind!=Kind::Heading){
      const auto last=token.raw.find_last_not_of('\n');
      const auto run=last==std::string::npos?token.raw.size():
        token.raw.size()-last-1;
      if(run>=2){
        separator=token.raw.substr(token.raw.size()-run);
        token.raw.resize(token.raw.size()-run);
      }
    }
    result.push_back(std::move(token));
    if(!separator.empty()){
      Token space;space.kind=Kind::Space;space.raw=std::move(separator);
      result.push_back(std::move(space));
    }
  }
  return result;
}

bool hasMarkdownSyntax(const std::string& input){
  const auto sample=input.substr(0,500);
  if(sample.find("\n\n")!=std::string::npos)return true;
  if(sample.find_first_of("#*`|[>\\-_~")!=std::string::npos)return true;
  for(std::size_t i=0;i<sample.size();++i){
    if(i!=0&&sample[i-1]!='\n')continue;
    auto n=i;
    while(n<sample.size()&&sample[n]>='0'&&sample[n]<='9')++n;
    if(n>i&&n+1<sample.size()&&sample[n]=='.'&&sample[n+1]==' ')
      return true;
  }
  return false;
}
namespace {
std::size_t tokenBytes(const Tokens& tokens){
  std::size_t total=tokens.capacity()*sizeof(Token);
  for(const auto& token:tokens){
    total+=token.raw.capacity()+token.text.capacity()+token.language.capacity()+
      token.href.capacity()+tokenBytes(token.children)+tokenBytes(token.items);
    for(const auto& cell:token.header)total+=tokenBytes(cell);
    for(const auto& row:token.rows)for(const auto& cell:row)total+=tokenBytes(cell);
    for(const auto& align:token.align)total+=align.capacity();
  }
  return total;
}
}
Tokens tokensFor(const std::string& content){
  if(hasMarkdownSyntax(content))return lex(content);
  Token paragraph;paragraph.kind=Kind::Paragraph;
  paragraph.raw=paragraph.text=content;
  Token text;text.kind=Kind::Text;text.raw=text.text=content;
  paragraph.children.push_back(std::move(text));
  Tokens plain;plain.push_back(std::move(paragraph));
  return plain;
}
const Tokens& TokenStash::get(const std::string& content){
  if(!hasMarkdownSyntax(content)){
    scratch_=tokensFor(content);
    return scratch_;
  }
  if(auto found=byContent_.find(content);found!=byContent_.end()){
    entries_.splice(entries_.end(),entries_,found->second);
    return found->second->tokens;
  }
  auto tokens=lex(content);
  const auto bytes=content.capacity()+tokenBytes(tokens);
  if(bytes>maxBytes_){
    scratch_=std::move(tokens);
    return scratch_;
  }
  while(!entries_.empty()&&(entries_.size()>=maxEntries||bytes_+bytes>maxBytes_)){
    byContent_.erase(entries_.front().content);
    bytes_-=entries_.front().bytes;
    entries_.pop_front();
  }
  entries_.push_back(Entry{content,std::move(tokens),bytes});
  auto entry=std::prev(entries_.end());
  byContent_.emplace(entry->content,entry);
  bytes_+=bytes;
  return entry->tokens;
}
void TokenStash::clear(){entries_.clear();byContent_.clear();bytes_=0;scratch_.clear();}
} // namespace batbox::interactive::markdown
