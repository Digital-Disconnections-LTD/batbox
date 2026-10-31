#pragma once
#include "flights/FlightState.hpp"
#include "flights/Metadata.hpp"
namespace batbox::flights {
struct MetadataReappendServices {
  std::function<std::string()> getFlightId;
  std::function<std::string(const std::string&)> readTail=readFileTailSync;
  std::function<void(const std::string&,const Json&)> appendMetadata=appendEntryToFile;
  std::function<std::string()> nowIso;
};
void reAppendFlightMetadata(FlightState& state,const MetadataReappendServices& services,bool skipTitleRefresh=false);
}
