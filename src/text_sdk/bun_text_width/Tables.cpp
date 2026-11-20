// Generated from Bun v1.3.11 commit a04817ce2b7f1a1e8b7cbf8af8f2c027ab072f1d.
// Inputs: src/string/immutable/{visible,grapheme,grapheme_tables}.zig.
// Emoji property input: Unicode 15.1 emoji-data.txt (Bun ICU 75.1).
// See LICENSE.upstream.md. Do not edit by hand.
#include "text_sdk/bun_text_width/Tables.hpp"
#include <algorithm>
namespace batbox::text_sdk::bun_text_width {
namespace {
struct Range {std::uint32_t first,last;};
constexpr std::uint16_t stage1[]={
#include "text_sdk/bun_text_width/data/stage1-00.inc"
#include "text_sdk/bun_text_width/data/stage1-01.inc"
};
constexpr std::uint8_t stage2[]={
#include "text_sdk/bun_text_width/data/stage2-00.inc"
#include "text_sdk/bun_text_width/data/stage2-01.inc"
#include "text_sdk/bun_text_width/data/stage2-02.inc"
#include "text_sdk/bun_text_width/data/stage2-03.inc"
#include "text_sdk/bun_text_width/data/stage2-04.inc"
#include "text_sdk/bun_text_width/data/stage2-05.inc"
};
constexpr std::uint8_t stage3[]={
#include "text_sdk/bun_text_width/data/stage3-00.inc"
};
constexpr Range wide[]={
#include "text_sdk/bun_text_width/data/wide-00.inc"
};
constexpr Range ambiguous[]={
#include "text_sdk/bun_text_width/data/ambiguous-00.inc"
};
constexpr Range emojiProperty[]={
#include "text_sdk/bun_text_width/data/emojiProperty-00.inc"
};
template<std::size_t N> bool contains(const Range (&table)[N],std::uint32_t cp){
  const auto* it=std::lower_bound(table,table+N,cp,
    [](const Range& range,std::uint32_t value){return range.last<value;});
  return it!=table+N&&it->first<=cp;
}
}
bool isFullWidth(std::uint32_t cp){return cp>=0x1100&&contains(wide,cp);}
bool isAmbiguous(std::uint32_t cp){return contains(ambiguous,cp);}
bool hasEmojiProperty(std::uint32_t cp){return contains(emojiProperty,cp);}
std::uint8_t graphemeProperty(std::uint32_t cp){
  if(cp>0x10ffff)return 0;
  return stage3[stage2[stage1[cp>>8]+(cp&255)]];
}
} // namespace batbox::text_sdk::bun_text_width
