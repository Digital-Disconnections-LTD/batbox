#include "intl/IndigenousInternal.hpp"
#include "runtime/JsonValue.hpp"
#include <unicode/brkiter.h>
#include <unicode/locid.h>
#include <unicode/ubrk.h>
#include <unicode/unistr.h>
#include <limits>
#include <stdexcept>
namespace batbox::intl {
namespace {
struct Cursor {
  std::unique_ptr<icu::BreakIterator> iterator;
  icu::UnicodeString text;
  std::string input;
  bool word;
  int32_t start=0;
};
}
std::shared_ptr<Segmenter> indigenousSegmenter(const std::string& granularity){
  UErrorCode status=U_ZERO_ERROR;const bool word=granularity=="word";
  if(!word&&granularity!="grapheme")throw std::invalid_argument("Unsupported segment granularity");
  std::shared_ptr<icu::BreakIterator> prototype(word?
    icu::BreakIterator::createWordInstance(icu::Locale::getDefault(),status):
    icu::BreakIterator::createCharacterInstance(icu::Locale::getDefault(),status));
  if(U_FAILURE(status)||!prototype)throw std::runtime_error(u_errorName(status));
  auto result=std::make_shared<Segmenter>();
  result->segment=[prototype,word](const std::string& input){
    const auto units=value::utf16CodeUnits(input);
    if(units.size()>static_cast<std::size_t>(std::numeric_limits<int32_t>::max()))
      throw std::length_error("Segment input exceeds ICU limits");
    auto cursor=std::make_shared<Cursor>();cursor->word=word;cursor->input=input;
    cursor->text=icu::UnicodeString(reinterpret_cast<const UChar*>(units.data()),static_cast<int32_t>(units.size()));
    cursor->iterator.reset(prototype->clone());
    if(!cursor->iterator)throw std::bad_alloc();
    cursor->iterator->setText(cursor->text);
    cursor->start=cursor->iterator->first();
    return SegmentCursor{[cursor]()->std::optional<Segment>{
      const auto end=cursor->iterator->next();if(end==icu::BreakIterator::DONE)return {};
      const auto start=cursor->start;cursor->start=end;
      const auto piece=cursor->text.tempSubStringBetween(start,end);
      const auto text=value::fromUtf16(std::u16string_view(reinterpret_cast<const char16_t*>(piece.getBuffer()),static_cast<std::size_t>(piece.length())));
      std::optional<bool> isWord;
      if(cursor->word)isWord=cursor->iterator->getRuleStatus()>=UBRK_WORD_NUMBER;
      return Segment{text,cursor->input,static_cast<std::size_t>(start),isWord};
    }};
  };
  return result;
}
} // namespace batbox::intl
