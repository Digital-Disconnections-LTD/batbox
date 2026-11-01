#pragma once

#include "interactive/markdown/Tokens.hpp"
#include <functional>
#include <optional>

namespace batbox::interactive::markdown {

struct FormatServices {
  std::function<std::string(const std::string&)> bold,italic,underline,dim,
    permission;
  std::function<std::string(const std::string&)> stripAnsi;
  std::function<int(const std::string&)> stringWidth;
  std::function<bool()> supportsHyperlinks;
  std::function<std::string(const std::string&,
                            const std::optional<std::string>&)> createHyperlink;
  std::function<bool(const std::string&)> supportsLanguage;
  std::function<std::string(const std::string&,const std::string&)> highlight;
  std::function<void(const std::string&)> debug;
  std::string blockQuoteBar="│";
  std::function<std::string(const std::string&)> listMarker,strongLeadIn;
  // Optional role styling. Each wraps already formatted text in SGR codes.
  std::function<std::string(const std::string&,int depth)> heading;
  std::function<std::string(const std::string&)> link,inlineCode;
  std::function<std::string(bool checked)> taskMarker;
  // Block chrome painted by the terminal renderer: code frames, rules and
  // list markers. Values are SGR sequences so this layer stays toolkit-free.
  struct BlockStyle {
    std::string codeFrame,codeLabel,codeBackground,rule,listMarker;
    // Prose wraps at this width (0: the full terminal width).
    int readingWidth=0;
  };
  std::function<BlockStyle()> blockStyle;
};

std::string formatToken(const Token& token,const FormatServices& services,
    int listDepth=0,std::optional<int> orderedListNumber={},
    const Token* parent=nullptr);
std::string formatTokens(const Tokens& tokens,const FormatServices& services);
std::string padAligned(const std::string& content,int displayWidth,
    int targetWidth,const std::string& align);
std::string stripPoetryXMLTags(const std::string& content);

// Core terminal styles and OSC 8 links. The caller supplies its capability,
// theme permission color, Unicode width and optional highlighter providers.
FormatServices indigenousFormatServices(bool colors,bool hyperlinks,
    std::string permissionSgr,
    std::function<int(const std::string&)> width);

} // namespace batbox::interactive::markdown
