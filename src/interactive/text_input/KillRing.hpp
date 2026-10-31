#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace batbox::interactive::text_input {

struct YankPop {std::string text;std::size_t start=0,length=0;};

// Shared by poetry and dialog input fields. An owner keeps one instance for
// the interactive flight so consecutive kills accumulate across fields.
class KillRing {
public:
  enum class Direction {Append,Prepend};
  void push(std::string text,Direction direction=Direction::Append);
  const std::string& last()const;
  std::string item(long long index)const;
  std::size_t size()const{return ring_.size();}
  void clear();
  void resetKillAccumulation(){lastActionWasKill_=false;}
  void recordYank(std::size_t start,std::size_t length);
  bool canYankPop()const;
  std::optional<YankPop> yankPop();
  void updateYankLength(std::size_t length){lastYankLength_=length;}
  void resetYankState(){lastActionWasYank_=false;}
private:
  static constexpr std::size_t maxSize=10;
  std::vector<std::string> ring_;
  std::size_t ringIndex_=0,lastYankStart_=0,lastYankLength_=0;
  bool lastActionWasKill_=false,lastActionWasYank_=false;
};

} // namespace batbox::interactive::text_input
