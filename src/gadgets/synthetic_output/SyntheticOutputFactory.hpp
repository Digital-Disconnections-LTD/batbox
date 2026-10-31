#pragma once
#include "gadgets/synthetic_output/SyntheticOutput.hpp"
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <thread>

namespace batbox::gadgets::synthetic_output {
struct AjvValidationIssue {
  std::string instancePath,message;
};
struct AjvValidation {
  bool valid=false;
  std::optional<std::vector<AjvValidationIssue>> errors;
};
using AjvValidator=std::function<AjvValidation(const Json&)>;
struct AjvBackend {
  // nullopt means Ajv.validateSchema returned true; a string is the exact
  // Ajv.errorsText output after false. This is an imported SDK boundary.
  std::function<std::optional<std::string>(const Json&)> validateSchema;
  std::function<AjvValidator(const Json&)> compile;
};
class OutputSchemaError:public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};
struct CreateResult {
  std::shared_ptr<Gadget> gadget;
  std::optional<std::string> error;
};
class SyntheticOutputFactory {
public:
  SyntheticOutputFactory(Gadget base,AjvBackend backend);
  std::shared_ptr<const CreateResult> create(
    const std::shared_ptr<const Json>& jsonSchema);
  std::size_t stashedEntries()const;
private:
  std::shared_ptr<const CreateResult> build(const Json& schema);
  Gadget base_;
  AjvBackend backend_;
  std::thread::id owner_;
  std::map<std::weak_ptr<const Json>,std::shared_ptr<const CreateResult>,
    std::owner_less<std::weak_ptr<const Json>>> stash_;
};
} // namespace batbox::gadgets::synthetic_output
