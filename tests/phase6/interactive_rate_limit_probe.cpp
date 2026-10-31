#include "interactive/cards_assistant/RateLimitCard.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>

using namespace batbox::interactive::cards_assistant;
using batbox::Json;

int main() {
  Json results = Json::array();
  for (int bits = 0; bits < 64; ++bits) {
    const auto value = getUpsellMessage({
      (bits & 1) != 0, (bits & 2) != 0, (bits & 4) != 0,
      (bits & 8) != 0, (bits & 16) != 0, (bits & 32) != 0});
    results.push_back({{"bits", bits},
      {"value", value ? Json(*value) : Json(nullptr)}});
  }
  std::cout << results.dump() << '\n';
}
