#include "gadgets/files/Grep.hpp"
#include "runtime/JsonValue.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace batbox::gadgets::files {
namespace {
std::size_t sliceIndex(double value,std::size_t length){
  if(std::isnan(value))return 0;
  if(value==std::numeric_limits<double>::infinity())return length;
  if(value==-std::numeric_limits<double>::infinity())return 0;
  value=std::trunc(value);
  if(value<0)return static_cast<std::size_t>(std::max(0.0,static_cast<double>(length)+value));
  return static_cast<std::size_t>(std::min(value,static_cast<double>(length)));
}
std::string number(double value){
  if(std::isfinite(value)&&std::trunc(value)==value&&std::abs(value)<9e15)
    return std::to_string(static_cast<long long>(value));
  return value::stringify(Json(value));
}
} // namespace

HeadLimitResult applyHeadLimit(const std::vector<std::string>& items,
  std::optional<double> limit,double offset){
  const auto start=sliceIndex(offset,items.size());
  if(limit&&*limit==0)return {std::vector<std::string>(items.begin()+start,items.end()),std::nullopt};
  const double effective=limit.value_or(250);
  const auto end=sliceIndex(offset+effective,items.size());
  std::vector<std::string> selected;
  if(end>start)selected.assign(items.begin()+start,items.begin()+end);
  const bool truncated=static_cast<double>(items.size())-offset>effective;
  return {std::move(selected),truncated?std::optional<double>(effective):std::nullopt};
}

std::string formatLimitInfo(std::optional<double> appliedLimit,std::optional<double> appliedOffset){
  std::string info;
  if(appliedLimit)info="limit: "+number(*appliedLimit);
  if(appliedOffset&&*appliedOffset!=0){
    if(!info.empty())info+=", ";
    info+="offset: "+number(*appliedOffset);
  }
  return info;
}
} // namespace batbox::gadgets::files
