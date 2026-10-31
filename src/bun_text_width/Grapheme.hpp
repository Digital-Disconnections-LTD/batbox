#pragma once
#include <cstdint>

namespace batbox::bun_text_width {
enum class BreakState : std::uint8_t {
  defaultValue, regionalIndicator, extendedPictographic,
  indicConsonant, indicLinker
};
bool graphemeBreak(std::uint32_t previous,std::uint32_t next,BreakState& state);

class GraphemeState {
public:
  void reset(std::uint32_t cp,bool ambiguousAsWide);
  void add(std::uint32_t cp,bool ambiguousAsWide);
  std::uint32_t width()const;
  bool active()const{return count_!=0;}
private:
  std::uint32_t first_=0,last_=0,nonEmojiWidth_=0;
  std::uint8_t baseWidth_=0,count_=0;
  bool emojiBase_=false,keycap_=false,regionalIndicator_=false;
  bool skinTone_=false,zwj_=false,vs15_=false,vs16_=false;
};
bool isZeroWidth(std::uint32_t cp);
std::uint8_t codepointWidth(std::uint32_t cp,bool ambiguousAsWide);
} // namespace batbox::bun_text_width
