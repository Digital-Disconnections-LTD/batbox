#include "providers/cloud/Clock.hpp"
#include <cctype>

namespace batbox::providers::cloud {
namespace {
// Howard Hinnant's days_from_civil.
long long daysFromCivil(long long y, unsigned m, unsigned d) {
  y -= m <= 2;
  const long long era = (y >= 0 ? y : y-399)/400;
  const auto yoe = static_cast<unsigned>(y-era*400);
  const unsigned doy = (153*(m+(m > 2 ? -3 : 9))+2)/5+d-1;
  const unsigned doe = yoe*365+yoe/4-yoe/100+doy;
  return era*146097+static_cast<long long>(doe)-719468;
}
bool digits(std::string_view text, std::size_t at, std::size_t count, int& out) {
  if (at+count > text.size()) return false;
  out = 0;
  for (std::size_t i = 0; i < count; ++i) {
    if (!std::isdigit(static_cast<unsigned char>(text[at+i]))) return false;
    out = out*10+(text[at+i]-'0');
  }
  return true;
}
} // namespace
std::optional<std::chrono::system_clock::time_point> parseTimestamp(std::string_view text) {
  int year, month, day, hour, minute, second;
  if (!digits(text,0,4,year) || text.size() < 19 || text[4] != '-' || !digits(text,5,2,month) || text[7] != '-' ||
      !digits(text,8,2,day) || (text[10] != 'T' && text[10] != 't' && text[10] != ' ') || !digits(text,11,2,hour) ||
      text[13] != ':' || !digits(text,14,2,minute) || text[16] != ':' || !digits(text,17,2,second))
    return std::nullopt;
  if (month < 1 || month > 12 || day < 1 || day > 31 || hour > 23 || minute > 59 || second > 60) return std::nullopt;
  std::size_t at = 19;
  if (at < text.size() && text[at] == '.') {
    ++at;
    while (at < text.size() && std::isdigit(static_cast<unsigned char>(text[at]))) ++at;
  }
  long long offset = 0;
  if (at < text.size() && (text[at] == 'Z' || text[at] == 'z')) ++at;
  else if (at < text.size() && (text[at] == '+' || text[at] == '-')) {
    int hours, minutes;
    if (!digits(text,at+1,2,hours) || at+3 >= text.size() || text[at+3] != ':' || !digits(text,at+4,2,minutes)) return std::nullopt;
    offset = (hours*60LL+minutes)*60LL*(text[at] == '+' ? 1 : -1);
    at += 6;
  }
  if (at != text.size()) return std::nullopt;
  const auto seconds = daysFromCivil(year,static_cast<unsigned>(month),static_cast<unsigned>(day))*86400LL+hour*3600LL+minute*60LL+second-offset;
  return std::chrono::system_clock::time_point(std::chrono::seconds(seconds));
}
} // namespace batbox::providers::cloud
