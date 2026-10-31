#pragma once
#include "ansi_tokenize/Types.hpp"
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace batbox::terminal_screen {
using StylesRef=std::shared_ptr<ansi_tokenize::Codes>;
class StylePool {
public:
  StylePool();
  std::int32_t intern(StylesRef styles);
  StylesRef get(std::int32_t id)const;
  std::string transition(std::int32_t fromId,std::int32_t toId);
  std::int32_t withInverse(std::int32_t baseId);
  std::int32_t withCurrentMatch(std::int32_t baseId);
  void setSelectionBg(ansi_tokenize::CodeRef background);
  std::int32_t withSelectionBg(std::int32_t baseId);
  std::int32_t none()const{return none_;}
  std::size_t size()const{return styles_.size();}
private:
  std::unordered_map<std::string,std::int32_t> ids_;
  std::vector<StylesRef> styles_;
  std::unordered_map<double,std::string> transitionStash_;
  std::unordered_map<std::int32_t,std::int32_t> inverseStash_,
    currentMatchStash_,selectionBgStash_;
  ansi_tokenize::CodeRef selectionBgCode_;
  std::int32_t none_=0;
};
bool hasVisibleSpaceEffect(const ansi_tokenize::Codes& styles);
} // namespace batbox::terminal_screen
