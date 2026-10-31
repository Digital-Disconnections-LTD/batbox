#pragma once
#include "runtime/JsonValue.hpp"
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace batbox::gadgets {
class SchemaStash {
public:
  void set(std::string name,Json schema);
  std::optional<Json> get(const std::string& name)const;
  const std::vector<std::pair<std::string,Json>>& entries()const;
  void clear();
private:
  std::vector<std::pair<std::string,Json>> entries_;
};
SchemaStash& getGadgetSchemaStash();
void clearGadgetSchemaStash();
}
