#include "settings/schema/imports/Sandbox.hpp"
#include "settings/schema/Runtime.hpp"
namespace batbox::settings::schema::imports {
SchemaResult sandboxSettings(const Json& input){
  static const Json node=value::parse(
    "{\"type\":\"object\",\"fields\":[[\"enabled\",{\"type\":\"optional\",\"inner\":{\"type\":\"boolean\"}}],[\"failIfUnavailable\",{\"type\":\"optional\",\"inner\":{\"type\":\"boolean\"}}],[\"aut"
    "oAllowBashIfSandboxed\",{\"type\":\"optional\",\"inner\":{\"type\":\"boolean\"}}],[\"allowUnsandboxedCommands\",{\"type\":\"optional\",\"inner\":{\"type\":\"boolean\"}}],[\"network\",{\""
    "type\":\"optional\",\"inner\":{\"type\":\"object\",\"fields\":[[\"allowedDomains\",{\"type\":\"optional\",\"inner\":{\"type\":\"array\",\"element\":{\"type\":\"string\"}}}],[\"allowManagedDo"
    "mainsOnly\",{\"type\":\"optional\",\"inner\":{\"type\":\"boolean\"}}],[\"allowUnixSockets\",{\"type\":\"optional\",\"inner\":{\"type\":\"array\",\"element\":{\"type\":\"string\"}}}],[\"allow"
    "AllUnixSockets\",{\"type\":\"optional\",\"inner\":{\"type\":\"boolean\"}}],[\"allowLocalBinding\",{\"type\":\"optional\",\"inner\":{\"type\":\"boolean\"}}],[\"httpProxyPort\",{\"type\":\"o"
    "ptional\",\"inner\":{\"type\":\"number\"}}],[\"socksProxyPort\",{\"type\":\"optional\",\"inner\":{\"type\":\"number\"}}]],\"passthrough\":false}}],[\"filesystem\",{\"type\":\"optional\",\""
    "inner\":{\"type\":\"object\",\"fields\":[[\"allowWrite\",{\"type\":\"optional\",\"inner\":{\"type\":\"array\",\"element\":{\"type\":\"string\"}}}],[\"denyWrite\",{\"type\":\"optional\",\"inner"
    "\":{\"type\":\"array\",\"element\":{\"type\":\"string\"}}}],[\"denyRead\",{\"type\":\"optional\",\"inner\":{\"type\":\"array\",\"element\":{\"type\":\"string\"}}}],[\"allowRead\",{\"type\":\"opt"
    "ional\",\"inner\":{\"type\":\"array\",\"element\":{\"type\":\"string\"}}}],[\"allowManagedReadPathsOnly\",{\"type\":\"optional\",\"inner\":{\"type\":\"boolean\"}}]],\"passthrough\":false}"
    "}],[\"ignoreViolations\",{\"type\":\"optional\",\"inner\":{\"type\":\"record\",\"value\":{\"type\":\"array\",\"element\":{\"type\":\"string\"}}}}],[\"enableWeakerNestedSandbox\",{\"type\":"
    "\"optional\",\"inner\":{\"type\":\"boolean\"}}],[\"enableWeakerNetworkIsolation\",{\"type\":\"optional\",\"inner\":{\"type\":\"boolean\"}}],[\"excludedCommands\",{\"type\":\"optional\",\""
    "inner\":{\"type\":\"array\",\"element\":{\"type\":\"string\"}}}],[\"ripgrep\",{\"type\":\"optional\",\"inner\":{\"type\":\"object\",\"fields\":[[\"command\",{\"type\":\"string\"}],[\"args\",{\"t"
    "ype\":\"optional\",\"inner\":{\"type\":\"array\",\"element\":{\"type\":\"string\"}}}]],\"passthrough\":false}}]],\"passthrough\":true}" );
  auto parsed=detail::parseNode(node,input,true,Json::array(),{},{});
  return {parsed.valid,parsed.valid?parsed.value:Json(nullptr),parsed.issues};
}
}
