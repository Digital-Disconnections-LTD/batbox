#include "termio/Dec.hpp"
namespace batbox::termio {
std::string decset(double mode){return csi(Json::array({"?"+value::numberToString(mode)+"h"}));}
std::string decreset(double mode){return csi(Json::array({"?"+value::numberToString(mode)+"l"}));}
}
