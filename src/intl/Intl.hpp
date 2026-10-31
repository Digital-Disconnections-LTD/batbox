#pragma once
#include <functional>
#include <map>
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
struct RelativeTimeFormat {
  std::function<std::string(double,const std::string&)> format;
};
struct Services {
  std::function<std::shared_ptr<Segmenter>(const std::string&)> createSegmenter;
  std::function<std::shared_ptr<RelativeTimeFormat>(const std::string&,
    const std::string&,const std::string&)> createRelativeTimeFormat;
  std::function<std::string()> resolvedTimeZone,resolvedLocale;
  std::function<std::optional<std::string>(const std::string&)> localeLanguage;
};
class Stash {
public:
  explicit Stash(Services services);
  std::shared_ptr<Segmenter> getGraphemeSegmenter();
  std::string firstGrapheme(const std::string& text);
  std::string lastGrapheme(const std::string& text);
  std::shared_ptr<Segmenter> getWordSegmenter();
  std::shared_ptr<RelativeTimeFormat> getRelativeTimeFormat(
    const std::string& style,const std::string& numeric);
  std::string getTimeZone();
  std::optional<std::string> getSystemLocaleLanguage();
private:
  Services services_;
  std::shared_ptr<Segmenter> grapheme_,word_;
  std::map<std::string,std::shared_ptr<RelativeTimeFormat>> rtf_;
  std::optional<std::string> timeZone_;
  std::optional<std::optional<std::string>> language_;
};
Services indigenousServices();
} // namespace batbox::intl
