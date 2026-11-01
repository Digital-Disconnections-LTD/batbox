#pragma once

#include "interactive/markdown/Tokens.hpp"
#include <list>
#include <string>
#include <string_view>
#include <unordered_map>

namespace batbox::interactive::markdown {

Tokens lex(const std::string& source);
bool hasMarkdownSyntax(const std::string& source);
// lex(), except that text without Markdown syntax becomes one paragraph.
Tokens tokensFor(const std::string& source);

class TokenStash {
public:
  static constexpr std::size_t defaultMaxBytes=8*1024*1024;
  static constexpr std::size_t maxEntries=500;
  explicit TokenStash(std::size_t maxBytes=defaultMaxBytes):maxBytes_(maxBytes){}
  // The reference stays valid until the next get() or clear().
  const Tokens& get(const std::string& content);
  void clear();
  std::size_t size() const{return entries_.size();}
  // Approximate heap bytes held by cached sources and token trees.
  std::size_t bytes() const{return bytes_;}
private:
  struct Entry {std::string content;Tokens tokens;std::size_t bytes=0;};
  std::list<Entry> entries_;
  std::unordered_map<std::string_view,std::list<Entry>::iterator> byContent_;
  std::size_t maxBytes_;
  std::size_t bytes_=0;
  Tokens scratch_;
};

} // namespace batbox::interactive::markdown
