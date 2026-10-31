#pragma once
#include "settings/schema/Leaves.hpp"

namespace batbox::settings::schema::detail {
std::string typeName(const Json& value);
std::string jsString(const Json& value);
void invalidType(Json& issues,const Json& path,
  const std::string& expected,const Json& input);
void invalidValue(Json& issues,const Json& path,
  const std::vector<std::string>& values);
bool stringField(const Json& input,const char* key,Json& output,
  Json& issues);
bool stringArrayField(const Json& input,const char* key,Json& output,
  Json& issues);
}
