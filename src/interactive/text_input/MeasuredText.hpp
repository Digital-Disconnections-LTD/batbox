#pragma once

#include "intl/Intl.hpp"
#include "text_width/StringWidth.hpp"
#include "text_width/WrapAnsi.hpp"
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace batbox::interactive::text_input {

struct Position {std::ptrdiff_t line=0;double column=0;};
struct WrappedLine {
  std::string text;
  std::size_t startOffset=0;
  bool isPrecededByNewline=false,endsWithNewline=false;
  bool equals(const WrappedLine& other)const{
    return text==other.text&&startOffset==other.startOffset;
  }
  std::size_t length()const;
};
struct WordBoundary {
  std::size_t start=0,end=0;
  bool isWordLike=false;
};
struct MetricsServices {
  std::function<std::string(const std::string&)> normalizeNfc;
  std::function<double(const std::string&)> stringWidth;
  std::function<std::string(const std::string&,double)> wrapAnsi;
  std::shared_ptr<intl::Segmenter> grapheme,word;
};
MetricsServices indigenousMetricsServices(
    std::shared_ptr<intl::Stash> intl,
    std::shared_ptr<text_width::StringWidth> width,
    text_width::WrapAnsiRef wrap);

class MeasuredText {
public:
  MeasuredText(std::string text,double columns,MetricsServices services);
  const std::string& text()const{return text_;}
  double columns()const{return columns_;}
  const MetricsServices& services()const{return services_;}
  std::size_t length()const{return length_;}
  const std::vector<WordBoundary>& getWordBoundaries()const;
  const std::vector<WrappedLine>& getWrappedLines()const;
  std::vector<std::string> getWrappedText()const;
  std::size_t lineCount()const{return getWrappedLines().size();}
  double stringIndexToDisplayWidth(const std::string& text,
                                   std::size_t index)const;
  std::size_t displayWidthToStringIndex(const std::string& text,
                                        double targetWidth)const;
  std::size_t getOffsetFromPosition(Position position)const;
  double getLineLength(std::ptrdiff_t line)const;
  Position getPositionFromOffset(std::size_t offset)const;
  std::size_t nextOffset(std::size_t offset)const;
  std::size_t prevOffset(std::size_t offset)const;
  std::size_t snapToGraphemeBoundary(std::size_t offset)const;
  double width(const std::string& text)const{return services_.stringWidth(text);}
private:
  const std::vector<std::size_t>& graphemeBoundaries()const;
  std::size_t binarySearchBoundary(const std::vector<std::size_t>& boundaries,
      std::size_t target,bool findNext)const;
  std::size_t offsetAtDisplayWidth(double targetWidth)const;
  std::vector<WrappedLine> measureWrappedText()const;
  const WrappedLine& getLine(std::ptrdiff_t line)const;
  std::string text_;
  std::u16string units_;
  double columns_;
  std::size_t length_;
  MetricsServices services_;
  mutable std::optional<std::vector<WrappedLine>> wrappedLines_;
  mutable std::optional<std::vector<WordBoundary>> wordBoundaries_;
  mutable std::optional<std::vector<std::size_t>> graphemeBoundaries_;
};

} // namespace batbox::interactive::text_input
