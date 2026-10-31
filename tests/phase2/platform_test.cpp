#include "runtime/Platform.hpp"
#include <cmath>
#include <iostream>
#include <regex>
#include <set>
#include <stdexcept>
using namespace batbox::platform;
void require(bool condition) { if(!condition)throw std::runtime_error("Native platform binding contract failed"); }
int main() {
  require(isoTimestamp(0)=="1970-01-01T00:00:00.000Z");
  require(isoTimestamp(-1)=="1969-12-31T23:59:59.999Z");
  require(isoTimestamp(951782400000)=="2000-02-29T00:00:00.000Z");
  require(isoTimestamp(-62167219200000.0)=="0000-01-01T00:00:00.000Z");
  require(isoTimestamp(253402300800000.0)=="+010000-01-01T00:00:00.000Z");
  require(isoTimestamp(8640000000000000.0)=="+275760-09-13T00:00:00.000Z");
  require(isoTimestamp(-8640000000000000.0)=="-271821-04-20T00:00:00.000Z");
  bool threw=false;try{isoTimestamp(8640000000000001.0);}catch(const std::range_error&){threw=true;}require(threw);
  const std::regex pattern("[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}");
  std::set<std::string> ids;
  for(int i=0;i<64;++i){auto id=randomUUID();require(std::regex_match(id,pattern));ids.insert(id);auto n=randomUnitInterval();require(n>=0&&n<1);}
  require(ids.size()==64);
  auto start=nowMillis();require(!isoTimestamp().empty());require(nowMillis()>=start);
  std::cout<<"Native UUID, UTC date bounds and randomness bindings passed\n";
}
