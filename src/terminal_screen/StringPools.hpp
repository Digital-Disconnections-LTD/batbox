#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace batbox::terminal_screen {
class CharPool {
public:
  CharPool();
  std::int32_t intern(const std::string& character);
  std::string get(double index)const;
  std::size_t size()const{return strings_.size();}
private:
  std::vector<std::string> strings_;
  std::unordered_map<std::string,std::int32_t> stringMap_;
  std::array<std::int32_t,128> ascii_;
};
class HyperlinkPool {
public:
  HyperlinkPool();
  std::int32_t intern(const std::optional<std::string>& hyperlink);
  std::optional<std::string> get(double id)const;
  std::size_t size()const{return strings_.size();}
private:
  std::vector<std::string> strings_;
  std::unordered_map<std::string,std::int32_t> stringMap_;
};
} // namespace batbox::terminal_screen
