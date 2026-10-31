#include "terminal_render/IndigenousIndent.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <limits>
#include <cmath>

using namespace batbox;
int main(){
  const double nan=std::numeric_limits<double>::quiet_NaN();
  const double inf=std::numeric_limits<double>::infinity();
  const std::vector<std::pair<std::string,double>> inputs{
    {"abc",2},{" a\n \nb",2},{"\r\nA",1},{"\t \nA",1},
    {"A\u2028B",1},{"\u00a0\nZ",2},{"",2},{"A",0},
    {"A",1.8},{"A",nan},{"A",-1},{"A",inf},
    {"\rA",2},{"A\r\nB",2},{"A\u2029 B",1}};
  Json rows=Json::array();
  for(const auto& [input,count]:inputs){
    const Json countJson=std::isfinite(count)?Json(count):Json(value::numberToString(count));
    Json row{{"input",input},{"count",countJson}};
    try{row["output"]=terminal_render::indigenousIndentString(input,count);}
    catch(const std::range_error& error){
      row["error"]=std::string("RangeError: ")+error.what();
    }
    rows.push_back(row);
  }
  std::cout<<rows.dump()<<'\n';
}
