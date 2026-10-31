#include "runtime/JsonValue.hpp"
#include <cmath>
#include <iostream>
using namespace batbox;
int main(){Json output=Json::array();
 const std::vector<Json> values={Json(Json::value_t::discarded),nullptr,false,true,
   0,-0.0,1,-1,std::nan(""),"","0","false",Json::array(),Json::object(),
   Json::array({nullptr}),Json{{"x",false}}};
 for(const auto& value:values)output.push_back(value::truthy(value));
 std::cout<<output.dump()<<'\n';
}
