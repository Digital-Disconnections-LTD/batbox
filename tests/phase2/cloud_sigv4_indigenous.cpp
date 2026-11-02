#include "cloud_check.hpp"
#include "providers/aws/SigV4.hpp"
#include "providers/cloud/Clock.hpp"
#include "providers/cloud/Crypto.hpp"
#include "runtime/JsonValue.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>

using namespace batbox;
using namespace batbox::providers;
using cloud_check::check;
using cloud_check::same;

namespace {
std::string slurp(const std::filesystem::path& path) {
  std::ifstream input(path,std::ios::binary);
  std::ostringstream buffer;
  buffer << input.rdbuf();
  return buffer.str();
}
aws::SigningRequest parseRequest(const std::string& text) {
  aws::SigningRequest request;
  std::size_t at = 0;
  auto line = [&] {
    auto end = text.find('\n',at);
    if (end == std::string::npos) end = text.size();
    auto out = text.substr(at,end-at);
    at = end+1;
    return out;
  };
  const auto first = line();
  const auto space = first.find(' ');
  request.method = first.substr(0,space);
  request.target = first.substr(space+1,first.rfind(" HTTP/")-space-1);
  while (at <= text.size()) {
    const auto header = line();
    if (header.empty()) break;
    const auto colon = header.find(':');
    request.headers.emplace_back(header.substr(0,colon),header.substr(colon+1));
  }
  if (at < text.size()) request.body = text.substr(at);
  return request;
}
void vector(const std::filesystem::path& directory) {
  const auto name = directory.filename().string();
  const auto context = value::parse(slurp(directory/"context.json"));
  aws::Credentials credentials;
  credentials.accessKeyId = context["credentials"]["access_key_id"];
  credentials.secretAccessKey = context["credentials"]["secret_access_key"];
  credentials.sessionToken = context["credentials"].value("token","");
  aws::SigningOptions options;
  options.region = context["region"];
  options.service = context["service"];
  options.time = *cloud::parseTimestamp(context["timestamp"].get<std::string>());
  options.normalizePath = context.value("normalize",true);
  options.addContentSha256Header = context.value("sign_body",false);
  options.signSessionToken = !context.value("omit_session_token",false);
  const auto result = aws::sign(parseRequest(slurp(directory/"request.txt")),credentials,options);
  same(result.canonicalRequest,slurp(directory/"header-canonical-request.txt"),name+" canonical request");
  same(result.stringToSign,slurp(directory/"header-string-to-sign.txt"),name+" string to sign");
  same(result.signature,slurp(directory/"header-signature.txt"),name+" signature");
}
} // namespace

int main(int argc, char** argv) {
  if (argc < 2) { std::cerr << "usage: cloud_sigv4 <fixtures>\n"; return 2; }
  int vectors = 0;
  for (const auto& entry : std::filesystem::directory_iterator(argv[1]))
    if (entry.is_directory()) { vector(entry.path()); ++vectors; }
  check(vectors >= 20,"expected the 20 embedded aws-c-auth vectors, found "+std::to_string(vectors));

  // The signing key derivation from the AWS documentation example.
  same(cloud::toHex(aws::signingKey("wJalrXUtnFEMI/K7MDENG+bPxRfiCYEXAMPLEKEY","20120215","us-east-1","iam")),
        "f4780e2d9f65fa895f9c67b32ce1baf0b0d8a43505a000a1a9e090d414db404d","documented signing key");

  // Bedrock model ids with ':' are %3A on the wire and %253A in the canonical request.
  same(aws::canonicalPath("/model/us.anthropic.claude-opus-5-5-v1%3A0/invoke",true),
        "/model/us.anthropic.claude-opus-5-5-v1%253A0/invoke","double-encoded Bedrock path");
  same(aws::canonicalQuery("typeEquals=SYSTEM_DEFINED&maxResults=1000&nextToken=a%2Fb"),
        "maxResults=1000&nextToken=a%2Fb&typeEquals=SYSTEM_DEFINED","sorted, re-encoded query");
  same(aws::amzDate(*cloud::parseTimestamp("2026-11-01T09:08:07.250+02:00")),"20261101T070807Z","x-amz-date from an offset time");

  aws::SigningRequest request;
  request.target = "/model/m/invoke";
  request.body = "{}";
  request.headers = {{"host","bedrock-runtime.us-west-2.amazonaws.com"},{"content-type","application/json"}};
  aws::SigningOptions options;
  options.region = "us-west-2";
  options.service = "bedrock";
  options.time = *cloud::parseTimestamp("2026-11-01T00:00:00Z");
  const auto signed_ = aws::sign(request,{"AKIDEXAMPLE","secret","token",{}},options);
  check(signed_.authorization.find("Credential=AKIDEXAMPLE/20261101/us-west-2/bedrock/aws4_request") != std::string::npos,"credential scope");
  check(signed_.authorization.find("SignedHeaders=content-type;host;x-amz-content-sha256;x-amz-date;x-amz-security-token") != std::string::npos,
        "Bedrock signed headers: "+signed_.authorization);
  same(signed_.payloadHash,cloud::sha256Hex("{}"),"payload hash");
  cloud_check::throwsContaining([] { aws::sign({},{},{"us-east-1","bedrock"}); },"access key","missing credentials");
  return cloud_check::finish("cloud-sigv4");
}
