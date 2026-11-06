#pragma once
#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace batbox::intl {
struct Segment {
  std::string text,input;
  std::size_t index=0;
  std::optional<bool> isWordLike;
};
struct SegmentCursor {std::function<std::optional<Segment>()> next;};
struct Segmenter {std::function<SegmentCursor(const std::string&)> segment;};
struct Services {
  std::function<std::shared_ptr<Segmenter>(const std::string&)> createSegmenter;
};
class Stash {
public:
  explicit Stash(Services services);
  std::shared_ptr<Segmenter> getGraphemeSegmenter();
  std::shared_ptr<Segmenter> getWordSegmenter();
private:
  Services services_;
  std::shared_ptr<Segmenter> grapheme_,word_;
};
Services indigenousServices();
} // namespace batbox::intl
