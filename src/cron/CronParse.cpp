#include "cron/CronExpression.hpp"
#include "runtime/JsonValue.hpp"
#include <cmath>
#include <regex>
#include <set>
#include <string>
#include <vector>

namespace batbox::cron {
namespace {
struct FieldRange {int min,max;};
constexpr FieldRange fieldRanges[]={{0,59},{0,23},{1,31},{1,12},{0,6}};
std::vector<std::string> split(const std::string& text,char separator){
  std::vector<std::string> result;
  std::size_t start=0;
  for(std::size_t i=0;i<=text.size();++i)
    if(i==text.size()||text[i]==separator){
      result.push_back(text.substr(start,i-start));start=i+1;
    }
  return result;
}
std::vector<std::string> splitWhitespace(const std::string& text){
  const auto stripped=value::trim(text);
  const auto units=value::utf16CodeUnits(stripped);
  std::vector<std::string> result;
  std::u16string current;
  bool previousSpace=false;
  for(const auto unit:units){
    const auto piece=value::fromUtf16(std::u16string_view(&unit,1));
    const bool isSpace=value::trim(piece).empty();
    if(isSpace){
      if(!previousSpace){result.push_back(value::fromUtf16(current));
        current.clear();}
      previousSpace=true;
    }else {current.push_back(unit);previousSpace=false;}
  }
  result.push_back(value::fromUtf16(current));
  return result;
}
std::optional<std::vector<int>> expandField(const std::string& field,
  FieldRange range){
  const auto min=range.min,max=range.max;
  std::set<int> out;
  static const std::regex stepPattern(R"(^\*(?:/(\d+))?$)");
  static const std::regex rangePattern(R"(^(\d+)-(\d+)(?:/(\d+))?$)");
  static const std::regex singlePattern(R"(^\d+$)");
  for(const auto& part:split(field,',')){
    std::smatch match;
    if(std::regex_match(part,match,stepPattern)){
      const double step=match[1].matched?value::parseInt10(match[1].str()):1;
      if(step<1)return std::nullopt;
      for(double i=min;i<=max;i+=step)out.insert(static_cast<int>(i));
      continue;
    }
    if(std::regex_match(part,match,rangePattern)){
      const double lo=value::parseInt10(match[1].str());
      const double hi=value::parseInt10(match[2].str());
      const double step=match[3].matched?value::parseInt10(match[3].str()):1;
      const bool isDow=min==0&&max==6;
      const int effectiveMax=isDow?7:max;
      if(lo>hi||step<1||lo<min||hi>effectiveMax)
        return std::nullopt;
      for(double i=lo;i<=hi;i+=step)
        out.insert(isDow&&i==7?0:static_cast<int>(i));
      continue;
    }
    if(std::regex_match(part,singlePattern)){
      double n=value::parseInt10(part);
      if(min==0&&max==6&&n==7)n=0;
      if(n<min||n>max)return std::nullopt;
      out.insert(static_cast<int>(n));continue;
    }
    return std::nullopt;
  }
  if(out.empty())return std::nullopt;
  return std::vector<int>(out.begin(),out.end());
}
}
std::optional<CronFields> parseCronExpression(const std::string& expression){
  const auto parts=splitWhitespace(expression);
  if(parts.size()!=5)return std::nullopt;
  std::vector<std::vector<int>> expanded;
  for(std::size_t i=0;i<5;++i){
    const auto result=expandField(parts[i],fieldRanges[i]);
    if(!result)return std::nullopt;
    expanded.push_back(*result);
  }
  return CronFields{expanded[0],expanded[1],expanded[2],expanded[3],
    expanded[4]};
}
}
