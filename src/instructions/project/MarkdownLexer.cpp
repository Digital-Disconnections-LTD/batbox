#include "instructions/project/MarkdownLexer.hpp"
#include <cmark-gfm.h>
#include <algorithm>
#include <memory>
#include <vector>

namespace batbox::instructions::project {
namespace {
std::string normalizeLineEndings(const std::string& text){
  std::string normalized;
  normalized.reserve(text.size());
  for(std::size_t i=0;i<text.size();++i){
    if(text[i]=='\r'){
      if(i+1<text.size()&&text[i+1]=='\n')++i;
      normalized+='\n';
    }else normalized+=text[i];
  }
  return normalized;
}
std::string literal(cmark_node* node){
  const auto* value=cmark_node_get_literal(node);
  return value?std::string(value):std::string{};
}
Json inlineTokens(cmark_node* parent);
Json inlineToken(cmark_node* node){
  const auto type=cmark_node_get_type(node);
  Json out=Json::object();
  switch(type){
  case CMARK_NODE_TEXT:out["type"]="text";out["text"]=literal(node);break;
  case CMARK_NODE_CODE:out["type"]="codespan";out["text"]=literal(node);break;
  case CMARK_NODE_HTML_INLINE:
    out["type"]="html";out["raw"]=literal(node);out["text"]=literal(node);break;
  case CMARK_NODE_SOFTBREAK:
  case CMARK_NODE_LINEBREAK:
    out["type"]="text";out["text"]="\n";break;
  default:
    out["type"]="container";
    out["tokens"]=inlineTokens(node);
    break;
  }
  return out;
}
Json inlineTokens(cmark_node* parent){
  Json out=Json::array();
  for(auto* child=cmark_node_first_child(parent);child;
      child=cmark_node_next(child))out.push_back(inlineToken(child));
  return out;
}
Json blockToken(cmark_node* node,const std::string& raw){
  const auto type=cmark_node_get_type(node);
  Json out={{"raw",raw}};
  switch(type){
  case CMARK_NODE_HTML_BLOCK:
    out["type"]="html";out["text"]=literal(node);break;
  case CMARK_NODE_CODE_BLOCK:
    out["type"]="code";out["text"]=literal(node);break;
  case CMARK_NODE_LIST:{
    out["type"]="list";Json items=Json::array();
    for(auto* item=cmark_node_first_child(node);item;item=cmark_node_next(item))
      items.push_back(blockToken(item,""));
    out["items"]=items;break;
  }
  case CMARK_NODE_ITEM:
    out["type"]="list_item";
    [[fallthrough]];
  default:{
    if(!out.contains("type")){
      if(type==CMARK_NODE_PARAGRAPH)out["type"]="paragraph";
      else if(type==CMARK_NODE_HEADING)out["type"]="heading";
      else if(type==CMARK_NODE_BLOCK_QUOTE)out["type"]="blockquote";
      else out["type"]="container";
    }
    Json children=Json::array();
    for(auto* child=cmark_node_first_child(node);child;child=cmark_node_next(child)){
      const auto childType=cmark_node_get_type(child);
      if((childType&CMARK_NODE_TYPE_MASK)==CMARK_NODE_TYPE_BLOCK)
        children.push_back(blockToken(child,""));
      else children.push_back(inlineToken(child));
    }
    out["tokens"]=children;
    break;
  }
  }
  return out;
}
}
Json lexMarkdown(const std::string& content){
  const auto normalized=normalizeLineEndings(content);
  std::unique_ptr<cmark_node,decltype(&cmark_node_free)> document(
    cmark_parse_document(normalized.data(),normalized.size(),CMARK_OPT_DEFAULT),
    &cmark_node_free);
  std::vector<std::size_t> lineStarts={0};
  for(std::size_t i=0;i<normalized.size();++i)
    if(normalized[i]=='\n')lineStarts.push_back(i+1);
  std::vector<std::pair<cmark_node*,std::size_t>> blocks;
  for(auto* node=cmark_node_first_child(document.get());node;
      node=cmark_node_next(node)){
    const auto line=cmark_node_get_start_line(node);
    const auto index=line>0?static_cast<std::size_t>(line-1):0;
    blocks.emplace_back(node,lineStarts[std::min(index,lineStarts.size()-1)]);
  }
  Json out=Json::array();
  if(blocks.empty()){
    if(!normalized.empty())out.push_back({{"type","space"},{"raw",normalized}});
    return out;
  }
  if(blocks.front().second>0)out.push_back({{"type","space"},
    {"raw",normalized.substr(0,blocks.front().second)}});
  for(std::size_t i=0;i<blocks.size();++i){
    const auto start=blocks[i].second;
    const auto end=i+1<blocks.size()?blocks[i+1].second:normalized.size();
    out.push_back(blockToken(blocks[i].first,normalized.substr(start,end-start)));
  }
  return out;
}
}
