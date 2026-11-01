#pragma once
#include "runtime/JsonValue.hpp"
#include "intl/Intl.hpp"
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
namespace batbox::text_width {
struct EmojiRegex {
  std::function<void()> reset;
  std::function<bool(const std::string&)> test;
};
using BunWidth=std::function<double(const Json&,const Json&)>;
struct Services {
  std::function<std::shared_ptr<EmojiRegex>()> emojiRegex;
  std::function<BunWidth()> resolveBunStringWidth;
  std::function<std::string(const std::string&)> stripAnsi;
  std::function<double(std::uint32_t)> eastAsianWidth;
  std::function<std::shared_ptr<intl::Segmenter>()> getGraphemeSegmenter;
};
bool needsSegmentation(const std::string& text);
double getEmojiWidth(const std::string& grapheme);
bool isZeroWidth(std::uint32_t codePoint);
class StringWidth {
public:
  explicit StringWidth(Services services);
  double stringWidth(const Json& text)const;
  // stringWidth for UTF-8 text, answering printable ASCII without boxing.
  double stringWidthUtf8(std::string_view text)const;
  double stringWidthJavaScript(const Json& text)const;
private:
  Services services_;
  std::shared_ptr<EmojiRegex> emoji_;
  BunWidth bun_;
  Json bunOptions_={{"ambiguousIsNarrow",true}};
};
Services indigenousServices(std::shared_ptr<intl::Stash> intl);
}
