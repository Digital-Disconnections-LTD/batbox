#pragma once

#include "interactive/markdown/Tokens.hpp"
#include <list>
#include <string>
#include <unordered_map>

namespace batbox::interactive::markdown {

Tokens lex(const std::string& source);
bool hasMarkdownSyntax(const std::string& source);

class TokenStash {
public:
  Tokens get(const std::string& content);
  void clear();
  std::size_t size() const{return entries_.size();}
private:
  using Entry=std::pair<std::string,Tokens>;
  std::list<Entry> entries_;
  std::unordered_map<std::string,std::list<Entry>::iterator> byContent_;
};

} // namespace batbox::interactive::markdown
