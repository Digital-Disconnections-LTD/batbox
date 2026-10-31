#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace batbox::glob {
// JavaScript's non-u RegExp operates on UTF-16 code units. This wrapper uses
// PCRE2-16 without UTF/UCP mode for the matcher SDK's generated expressions.
struct RegexMatch {
  std::size_t start=0,end=0;
  std::vector<std::optional<std::u16string>> groups;
};
class Regex16 {
public:
  explicit Regex16(std::u16string source,bool ignoreCase=false);
  ~Regex16();
  Regex16(const Regex16&)=delete;
  Regex16& operator=(const Regex16&)=delete;
  Regex16(Regex16&&)noexcept;
  Regex16& operator=(Regex16&&)noexcept;
  bool search(std::u16string_view text,std::size_t offset=0,
    RegexMatch* match=nullptr)const;
  std::u16string replace(std::u16string_view text,
    const std::function<std::u16string(const RegexMatch&)>& callback,
    bool global=false)const;
private:
  struct Code;
  std::unique_ptr<Code> code_;
};
std::u16string units(std::string_view value);
std::string text(std::u16string_view value);
} // namespace batbox::glob
