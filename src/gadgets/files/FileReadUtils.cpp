#include "gadgets/files/FileRead.hpp"
#include "runtime/JsonValue.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <limits>
#include <regex>
#include <unordered_set>

namespace batbox::gadgets::files {
namespace {
const std::unordered_set<std::string> blocked = {
  "/dev/zero", "/dev/random", "/dev/urandom", "/dev/full",
  "/dev/stdin", "/dev/tty", "/dev/console", "/dev/stdout",
  "/dev/stderr", "/dev/fd/0", "/dev/fd/1", "/dev/fd/2"
};
bool endsWith(const std::string& s, const std::string& suffix) {
  return s.size() >= suffix.size() && s.compare(s.size()-suffix.size(), suffix.size(), suffix)==0;
}
std::string lower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
  return s;
}
} // namespace

bool isBlockedDevicePath(const std::string& filePath) {
  if (blocked.contains(filePath)) return true;
  return filePath.starts_with("/proc/") &&
    (endsWith(filePath,"/fd/0") || endsWith(filePath,"/fd/1") || endsWith(filePath,"/fd/2"));
}

bool isPDFExtension(const std::string& extension) {
  return lower(extension.starts_with('.') ? extension.substr(1) : extension) == "pdf";
}

std::optional<PdfPageRange> parsePDFPageRange(const std::string& pages) {
  const auto trimmed=value::trim(pages);
  if (trimmed.empty()) return std::nullopt;
  if (trimmed.ends_with('-')) {
    const double first=value::parseInt10(trimmed.substr(0,trimmed.size()-1));
    if (std::isnan(first) || first<1) return std::nullopt;
    return PdfPageRange{first,std::numeric_limits<double>::infinity()};
  }
  const auto dash=trimmed.find('-');
  if (dash==std::string::npos) {
    const double page=value::parseInt10(trimmed);
    if (std::isnan(page) || page<1) return std::nullopt;
    return PdfPageRange{page,page};
  }
  const double first=value::parseInt10(trimmed.substr(0,dash));
  const double last=value::parseInt10(trimmed.substr(dash+1));
  if (std::isnan(first) || std::isnan(last) || first<1 || last<1 || last<first) return std::nullopt;
  return PdfPageRange{first,last};
}

std::optional<std::string> getAlternateScreenshotPath(const std::string& path) {
  const auto file=std::filesystem::path(path).filename().string();
  static const std::regex pattern(R"(^(.+)( |\xE2\x80\xAF)(AM|PM)(\.png)$)");
  std::smatch match;
  if (!std::regex_match(file,match,pattern)) return std::nullopt;
  const std::string current=match[2].str();
  const std::string replacement=(current==" " ? "\xE2\x80\xAF" : " ")+match[3].str()+match[4].str();
  const std::string needle=current+match[3].str()+match[4].str();
  auto out=path;
  const auto pos=out.find(needle);
  if (pos!=std::string::npos) out.replace(pos,needle.size(),replacement);
  return out;
}

std::string addLineNumbers(const std::string& content, std::size_t startLine, bool compactPrefix) {
  if (content.empty()) return "";
  std::string out;
  std::size_t start=0, index=0;
  while (true) {
    const auto end=content.find('\n',start);
    auto line=content.substr(start,end==std::string::npos ? std::string::npos : end-start);
    if (!line.empty() && line.back()=='\r') line.pop_back();
    const std::string number=std::to_string(startLine+index);
    if (index) out+='\n';
    if (compactPrefix) out+=number+"\t"+line;
    else out+=std::string(number.size()<6 ? 6-number.size() : 0,' ')+number+"→"+line;
    ++index;
    if (end==std::string::npos) break;
    start=end+1;
  }
  return out;
}

} // namespace batbox::gadgets::files
