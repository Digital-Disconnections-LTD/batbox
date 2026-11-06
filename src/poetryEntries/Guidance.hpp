#pragma once
#include "runtime/JsonValue.hpp"
#include <functional>
#include <optional>
#include <string>

namespace batbox::poetryEntries {
struct GadgetNames;
struct GuidanceServices {
  std::string batGadgetName,askUserQuestionGadgetName,skillGadgetName,
    exploreBatType;
  int exploreBatMinQueries=0;
  std::string missionCreateGadgetName,todoWriteGadgetName,globGadgetName,
    grepGadgetName;
  std::function<bool()> nonInteractiveFlight,explorePlanBatsEnabled;
};
std::string getBatGadgetSection(const GuidanceServices& services);
std::string getUsingYourGadgetsSection(const GadgetNames& enabledGadgets,
  const GuidanceServices& services);
std::optional<std::string> getFlightSpecificGuidanceSection(
  const GadgetNames& enabledGadgets,const Json& skillGadgetCommands,
  const GuidanceServices& services);
}
