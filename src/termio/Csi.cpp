#include "termio/Csi.hpp"
#include "runtime/ArrayJoin.hpp"
#include <stdexcept>
namespace batbox::termio {
bool isCSIParam(double byte){return byte>=0x30&&byte<=0x3f;}
bool isCSIIntermediate(double byte){return byte>=0x20&&byte<=0x2f;}
bool isCSIFinal(double byte){return byte>=0x40&&byte<=0x7e;}
namespace {
std::string argument(const Json& value){
  if(value.is_string())return value.get<std::string>();
  if(value.is_number())return value::numberToString(value.get<double>());
  throw std::invalid_argument("CSI argument requires a string or number");
}
}
std::string csi(const Json& args){
  if(args.empty())return CSI_PREFIX;
  if(args.size()==1)return CSI_PREFIX+argument(args.at(0));
  Json params=Json::array();
  for(std::size_t i=0;i+1<args.size();++i)params.push_back(args.at(i));
  return CSI_PREFIX+value::arrayJoin(params,";")+argument(args.back());
}
}
