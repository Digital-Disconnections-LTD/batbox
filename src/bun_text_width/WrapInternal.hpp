#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace batbox::bun_text_width {
struct WrapOptions {
  bool hard=false,wordWrap=true,trim=true,ambiguousIsNarrow=true;
};
struct WrapContext {
  std::size_t columns=0;
  bool latin1=false;
  WrapOptions options;
  std::size_t width(std::u16string_view text)const;
};
struct Row {
  std::u16string data;
  std::size_t width(const WrapContext& context)const;
  void trimLeadingSpaces();
};
using Rows=std::vector<Row>;
void wrapWord(Rows& rows,std::u16string_view word,const WrapContext& context);
void trimRowTrailingSpaces(Row& row,const WrapContext& context);
void joinRowsWithAnsiPreservation(const Rows& rows,std::u16string& result);
void processLine(std::u16string_view line,const WrapContext& context,Rows& rows);
std::u16string wrapUnits(std::u16string_view input,const WrapContext& context);
} // namespace batbox::bun_text_width
