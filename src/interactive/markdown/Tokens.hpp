#pragma once

#include <string>
#include <vector>

namespace batbox::interactive::markdown {

enum class Kind {
  BlockQuote, Code, CodeSpan, Emphasis, Strong, Heading, Rule, Image,
  Link, List, ListItem, Paragraph, Space, Break, Text, Table, Escape,
  Definition, Deleted, Html
};

struct Token {
  Kind kind=Kind::Text;
  std::string raw,text,language,href;
  int depth=0,start=1;
  // Task list items: -1 for a regular item, 0 unchecked, 1 checked.
  int checked=-1;
  bool ordered=false;
  std::vector<Token> children,items;
  std::vector<std::vector<Token>> header;
  std::vector<std::vector<std::vector<Token>>> rows;
  std::vector<std::string> align;
};

using Tokens=std::vector<Token>;

} // namespace batbox::interactive::markdown
