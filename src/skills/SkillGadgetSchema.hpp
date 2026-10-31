#pragma once
#include "runtime/JsonValue.hpp"
namespace batbox::skills {
Json skillGadgetInputSchema();
Json skillGadgetOutputSchema();
Json parseSkillGadgetInput(const Json& input);
}
