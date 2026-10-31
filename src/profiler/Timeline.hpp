#pragma once
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace batbox::profiler {
struct Mark {std::string name;double startTime;};
struct MemoryUsage {double rss=0,heapUsed=0;};

// One owner-thread timeline shared by query and headless profilers;
// both use the same clock and event sequence.
class Timeline {
public:
  explicit Timeline(std::function<double()> now);
  double now() const;
  void mark(const std::string& name);
  void clearMarks(const std::optional<std::string>& name=std::nullopt);
  const std::vector<Mark>& marks() const{return marks_;}
private:
  std::function<double()> now_;
  std::vector<Mark> marks_;
};

std::string formatMs(double ms);
std::string formatTimelineLine(double totalMs,double deltaMs,
    const std::string& name,const std::optional<MemoryUsage>& memory,
    std::size_t totalPad,std::size_t deltaPad,const std::string& extra="");
}
