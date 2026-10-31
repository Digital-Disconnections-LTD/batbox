#include "format/TruncateExtra.hpp"
#include <stdexcept>
#include <utility>
#include <vector>

namespace batbox::format {
namespace {
void require(const TruncateServices& services){
  if(!services.stringWidth||!services.getGraphemeSegmenter)
    throw std::invalid_argument("Truncation imports required");
}
}
std::string truncatePathMiddle(const std::string& path,double maxLength,
    const TruncateServices& services){
  require(services);
  if(services.stringWidth(path)<=maxLength)return path;
  if(maxLength<=0)return "…";
  if(maxLength<5)return truncateToWidth(path,maxLength,services);
  const auto lastSlash=path.rfind('/');
  const auto filename=lastSlash!=std::string::npos?
    path.substr(lastSlash):path;
  const auto directory=lastSlash!=std::string::npos?
    path.substr(0,lastSlash):std::string();
  const auto filenameWidth=services.stringWidth(filename);
  if(filenameWidth>=maxLength-1)
    return truncateStartToWidth(path,maxLength,services);
  const auto availableForDir=maxLength-1-filenameWidth;
  if(availableForDir<=0)
    return truncateStartToWidth(filename,maxLength,services);
  const auto truncatedDir=truncateToWidthNoEllipsis(
    directory,availableForDir,services);
  return truncatedDir+"…"+filename;
}
std::string truncateStartToWidth(const std::string& text,double maxWidth,
    const TruncateServices& services){
  require(services);
  if(services.stringWidth(text)<=maxWidth)return text;
  if(maxWidth<=1)return "…";
  std::vector<std::string> segments;
  auto cursor=services.getGraphemeSegmenter()->segment(text);
  while(const auto segment=cursor.next())segments.push_back(segment->text);
  double width=0;
  std::size_t start=segments.size();
  for(std::size_t i=segments.size();i>0;--i){
    const auto segmentWidth=services.stringWidth(segments[i-1]);
    if(width+segmentWidth>maxWidth-1)break;
    width+=segmentWidth;
    start=i-1;
  }
  std::string result="…";
  for(std::size_t i=start;i<segments.size();++i)result+=segments[i];
  return result;
}
std::string truncateToWidthNoEllipsis(const std::string& text,
    double maxWidth,const TruncateServices& services){
  require(services);
  if(services.stringWidth(text)<=maxWidth)return text;
  if(maxWidth<=0)return "";
  double width=0;
  std::string result;
  auto cursor=services.getGraphemeSegmenter()->segment(text);
  while(const auto segment=cursor.next()){
    const auto segmentWidth=services.stringWidth(segment->text);
    if(width+segmentWidth>maxWidth)break;
    result+=segment->text;
    width+=segmentWidth;
  }
  return result;
}
std::vector<std::string> wrapText(const std::string& text,double width,
    const TruncateServices& services){
  require(services);
  std::vector<std::string> lines;
  std::string currentLine;
  double currentWidth=0;
  auto cursor=services.getGraphemeSegmenter()->segment(text);
  while(const auto segment=cursor.next()){
    const auto segmentWidth=services.stringWidth(segment->text);
    if(currentWidth+segmentWidth<=width){
      currentLine+=segment->text;
      currentWidth+=segmentWidth;
    }else{
      if(!currentLine.empty())lines.push_back(currentLine);
      currentLine=segment->text;
      currentWidth=segmentWidth;
    }
  }
  if(!currentLine.empty())lines.push_back(currentLine);
  return lines;
}
}
