#pragma once
#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace batbox::glob::detail {
struct PicoToken {
  PicoToken(std::string kind={},std::u16string text={},
    std::optional<std::u16string> emitted=std::nullopt,
    std::u16string suffixText={},std::size_t previous=0,
    bool isStar=false,bool isPosix=false,bool isExtglob=false)
    :type(std::move(kind)),value(std::move(text)),output(std::move(emitted)),
      suffix(std::move(suffixText)),prev(previous),star(isStar),
      posix(isPosix),extglob(isExtglob){}
  std::string type;
  std::u16string value;
  std::optional<std::u16string> output;
  std::u16string suffix;
  std::size_t prev=0;
  bool star=false,posix=false,extglob=false;
};
struct PicoExtglob {
  std::string type;
  std::u16string open,close,inner,output;
  std::size_t prev=0,tokensIndex=0,startIndex=0;
  int parens=0,conditions=1;
};
struct PicoBrace {
  std::size_t tokenIndex=0,outputIndex=0;
  bool comma=false,dots=false;
};
struct PicoState {
  std::u16string output;
  bool negated=false,globstar=false,negatedExtglob=false;
};
class PicoParser {
public:
  PicoParser(std::u16string pattern,bool dot);
  PicoState parse();
private:
  std::u16string input_,output_,consumed_;
  std::ptrdiff_t index_=-1;
  std::size_t start_=0,prev_=0;
  bool dot_=true,backtrack_=false,negated_=false,globstar_=false,
    negatedExtglob_=false;
  int brackets_=0,bracesCount_=0,parens_=0,quotes_=0;
  std::vector<PicoToken> tokens_;
  std::vector<PicoExtglob> extglobs_;
  std::vector<PicoBrace> braces_;
  std::vector<std::string> stack_;

  bool eos()const;
  char16_t peek(std::size_t n=1)const;
  char16_t advance();
  std::u16string remaining()const;
  void consume(std::u16string_view value,std::size_t count=0);
  PicoToken& prev(){return tokens_[prev_];}
  const PicoToken& prev()const{return tokens_[prev_];}
  void push(PicoToken token);
  void append(const PicoToken& token);
  void increment(const char* type);
  void decrement(const char* type);
  bool handleEscape(std::u16string& value);
  bool handleBracketBody(std::u16string& value);
  bool handleQuote(std::u16string& value);
  bool handleGroup(char16_t value);
  bool handleBrace(char16_t value);
  bool handleSpecial(char16_t value);
  bool handleStar();
  void extglobOpen(const std::string& type,char16_t value);
  void extglobClose(PicoExtglob token);
  void closeUnbalanced();
  friend std::u16string escapePicoRegex(std::u16string_view);
};
std::u16string escapePicoRegex(std::u16string_view value);
std::u16string escapePicoLast(std::u16string value,char16_t character);
} // namespace batbox::glob::detail
