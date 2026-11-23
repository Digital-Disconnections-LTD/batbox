#pragma once
#include "runtime/ArrayJoin.hpp"
#include "runtime/JsonValue.hpp"
#include <stdexcept>
namespace batbox::termio {
inline const std::string CSI_PREFIX="\x1b[";
inline bool isCSIParam(double byte){return byte>=0x30&&byte<=0x3f;}
inline bool isCSIIntermediate(double byte){return byte>=0x20&&byte<=0x2f;}
inline bool isCSIFinal(double byte){return byte>=0x40&&byte<=0x7e;}
namespace detail {
inline std::string argument(const Json& value){
  if(value.is_string())return value.get<std::string>();
  if(value.is_number())return value::numberToString(value.get<double>());
  throw std::invalid_argument("CSI argument requires a string or number");
}
}
inline std::string csi(const Json& args=Json::array()){
  if(args.empty())return CSI_PREFIX;
  if(args.size()==1)return CSI_PREFIX+detail::argument(args.at(0));
  Json params=Json::array();
  for(std::size_t i=0;i+1<args.size();++i)params.push_back(args.at(i));
  return CSI_PREFIX+value::arrayJoin(params,";")+detail::argument(args.back());
}
inline const std::string PASTE_START=csi(Json::array({"200~"}));
inline const std::string PASTE_END=csi(Json::array({"201~"}));
inline const std::string FOCUS_IN=csi(Json::array({"I"}));
inline const std::string FOCUS_OUT=csi(Json::array({"O"}));
}
