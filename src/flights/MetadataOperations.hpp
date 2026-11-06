#pragma once
#include "flights/FlightState.hpp"
namespace batbox::flights {
struct MetadataServices {
  FlightState* project=nullptr;
};
class MetadataOperations {
  MetadataServices services;
public:
  explicit MetadataOperations(MetadataServices services):services(std::move(services)){}
  void restoreFlightMetadata(const Json& metadata);
  void clearFlightMetadata();
};
}
