#include "cloud_check.hpp"
#include "providers/aws/BedrockClient.hpp"
#include "providers/cloud/Clock.hpp"
#include "providers/cloud/Crypto.hpp"
#include "providers/cloud/ErrorShape.hpp"
#include "providers/gcp/VertexClient.hpp"
#include "transport/Errors.hpp"
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <cstdlib>
#include <memory>

using namespace batbox;
using namespace batbox::providers;
using cloud_check::check;
using cloud_check::same;
using cloud_check::throwsContaining;

namespace {
std::string header(const cloud::HttpRequest& request, const std::string& name) {
  for (const auto& [key,value] : request.headers) if (key == name) return value;
  return {};
}
Json sampleBody() {
  return Json::parse(R"({"model":"us.anthropic.claude-opus-5-5-v1:0","max_tokens":64,"stream":true,
    "system":[{"type":"text","text":"be brief"}],"messages":[{"role":"user","content":"hi"}],
    "tools":[{"name":"read","description":"read a file","input_schema":{"type":"object"}}],"betas":["context-1m"]})");
}
transport::APIError caught(const std::function<void()>& body) {
  try { body(); }
  catch (const transport::APIError& error) { return error; }
  throw std::runtime_error("expected an APIError");
}
std::string pemOf(EVP_PKEY* key, bool priv) {
  std::unique_ptr<BIO,decltype(&BIO_free)> bio(BIO_new(BIO_s_mem()),BIO_free);
  if (priv) PEM_write_bio_PrivateKey(bio.get(),key,nullptr,nullptr,0,nullptr,nullptr);
  else PEM_write_bio_PUBKEY(bio.get(),key);
  char* data = nullptr;
  const auto length = BIO_get_mem_data(bio.get(),&data);
  return std::string(data,static_cast<std::size_t>(length));
}
std::string base64UrlDecode(std::string text) {
  for (auto& c : text) c = c == '-' ? '+' : c == '_' ? '/' : c;
  while (text.size()%4) text += '=';
  return cloud::base64Decode(text).value_or("");
}
} // namespace

int main() {
  for (const auto* name : {"AWS_ENDPOINT_URL","AWS_ENDPOINT_URL_BEDROCK","AWS_ENDPOINT_URL_BEDROCK_RUNTIME","GOOGLE_VERTEX_PROJECT",
                           "GOOGLE_CLOUD_PROJECT","GCP_PROJECT","GCLOUD_PROJECT","GOOGLE_VERTEX_LOCATION","GOOGLE_CLOUD_LOCATION",
                           "VERTEX_LOCATION","GOOGLE_APPLICATION_CREDENTIALS"})
    unsetenv(name);
  const auto when = *cloud::parseTimestamp("2026-11-01T12:00:00Z");
  // Bedrock body and request.
  {
    const auto body = aws::bedrockBody(sampleBody());
    check(!body.contains("model"),"Bedrock body drops model");
    check(!body.contains("stream"),"Bedrock body drops stream");
    check(!body.contains("betas"),"Bedrock body drops betas");
    same(body.value("anthropic_version",""),"bedrock-2023-05-31","Bedrock anthropic_version");
    check(body["anthropic_beta"] == Json::array({"context-1m"}),"betas become anthropic_beta");
    check(body["tools"][0]["name"] == "read" && body["max_tokens"] == 64,"the rest of the body is unchanged");
    auto custom = sampleBody();
    custom["anthropic_version"] = "bedrock-2099-01-01";
    same(aws::bedrockBody(custom).value("anthropic_version",""),"bedrock-2099-01-01","an explicit version is kept");
  }
  {
    aws::BedrockConfig config;
    config.headers = {{"x-amzn-bedrock-guardrailidentifier","gr-1"},{"x-team","blue"}};
    aws::BedrockAuth auth;
    auth.credentials = {"AKIDEXAMPLE","secret","session",{}};
    const auto request = aws::prepareBedrockRequest(config,auth,"us-west-2",sampleBody(),true,when);
    same(request.url,"https://bedrock-runtime.us-west-2.amazonaws.com/model/us.anthropic.claude-opus-5-5-v1%3A0/invoke-with-response-stream",
          "streaming URL with an encoded model id");
    same(header(request,"accept"),"application/vnd.amazon.eventstream","event-stream accept");
    same(header(request,"x-amz-date"),"20261101T120000Z","x-amz-date");
    same(header(request,"x-amz-content-sha256"),cloud::sha256Hex(request.body),"payload hash header");
    same(header(request,"x-amz-security-token"),"session","session token header");
    same(header(request,"x-team"),"blue","custom headers are sent");
    const auto authorization = header(request,"authorization");
    check(authorization.starts_with("AWS4-HMAC-SHA256 Credential=AKIDEXAMPLE/20261101/us-west-2/bedrock/aws4_request, "),
          "SigV4 authorization: "+authorization);
    check(authorization.find("SignedHeaders=accept;content-type;host;x-amz-content-sha256;x-amz-date;x-amz-security-token;"
                             "x-amzn-bedrock-guardrailidentifier,") != std::string::npos,"signed header list: "+authorization);
    check(request.body.find("\"model\"") == std::string::npos,"serialized body has no model");
    const auto invoke = aws::prepareBedrockRequest(config,auth,"us-west-2",sampleBody(),false,when);
    check(invoke.url.ends_with("/invoke"),"non-streaming URL");
    same(header(invoke,"accept"),"application/json","non-streaming accept");

    aws::BedrockAuth bearer;
    bearer.kind = aws::BedrockAuth::Kind::Bearer;
    bearer.bearerToken = "api-key";
    const auto keyed = aws::prepareBedrockRequest(config,bearer,"eu-west-1",sampleBody(),true,when);
    same(header(keyed,"authorization"),"Bearer api-key","Bedrock API key is a bearer token");
    check(header(keyed,"x-amz-date").empty(),"bearer requests are not signed");

    config.baseUrl = "https://vpce-1.bedrock-runtime.us-west-2.vpce.amazonaws.com/";
    check(aws::prepareBedrockRequest(config,auth,"us-west-2",sampleBody(),true,when).url.starts_with("https://vpce-1."),
          "base_url overrides the runtime host");
    config.baseUrl.clear();
    setenv("AWS_ENDPOINT_URL_BEDROCK_RUNTIME","http://127.0.0.1:9",1);
    same(aws::bedrockRuntimeUrl(config,"us-east-1"),"http://127.0.0.1:9","AWS_ENDPOINT_URL_BEDROCK_RUNTIME");
    unsetenv("AWS_ENDPOINT_URL_BEDROCK_RUNTIME");
    same(aws::bedrockControlUrl("ap-northeast-1"),"https://bedrock.ap-northeast-1.amazonaws.com","control plane host");
    throwsContaining([&] { aws::prepareBedrockRequest(config,auth,"us-east-1",Json{{"max_tokens",1}},true,when); },"model id","model is required");
  }
  // Bedrock HTTP errors keep retry-relevant Anthropic types.
  {
    auto error = caught([] { aws::throwBedrockHttpError(429,R"({"message":"Too many tokens"})",
      Json{{"x-amzn-errortype","ThrottlingException:http://internal.amazon.com/coral/com.amazon.bedrock/"},{"x-amzn-requestid","r-1"}}); });
    check(error.status == 429,"throttling status");
    same(error.body["error"]["type"],"rate_limit_error","throttling type");
    check(std::string(error.what()).find("rate_limit_error") != std::string::npos,"rendered message carries the type");
    same(error.requestId,"r-1","x-amzn-requestid becomes the request id");
    error = caught([] { aws::throwBedrockHttpError(400,R"({"message":"Malformed input request"})",Json{{"x-amzn-errortype","ValidationException"}}); });
    same(error.body["error"]["type"],"invalid_request_error","validation type");
    error = caught([] { aws::throwBedrockHttpError(403,R"({"Message":"denied"})",Json{{"x-amzn-errortype","AccessDeniedException"}}); });
    check(error.status == 403 && error.body["error"]["type"] == "permission_error","access denied");
    error = caught([] { aws::throwBedrockHttpError(429,R"({"message":"not ready"})",Json{{"x-amzn-errortype","ModelNotReadyException"}}); });
    same(error.body["error"]["type"],"overloaded_error","model not ready retries as overloaded");
    error = caught([] { aws::throwBedrockHttpError(503,R"({"__type":"com.amazon#ServiceUnavailableException","message":"busy"})",Json::object()); });
    check(error.status == 503 && error.body["error"]["type"] == "overloaded_error","__type fallback");
    error = caught([] { aws::throwBedrockHttpError(403,R"({"message":"The security token included in the request is expired"})",
                                                   Json{{"x-amzn-errortype","ExpiredTokenException"}}); });
    check(error.status == 401 && error.body["error"]["type"] == "authentication_error","expired token is an authentication error");
  }
  // Vertex body, URL and headers.
  {
    const auto body = gcp::vertexBody(sampleBody(),true);
    check(!body.contains("model"),"Vertex body drops model");
    check(body["stream"] == true,"Vertex streaming body sets stream");
    check(gcp::vertexBody(sampleBody(),false)["stream"] == false,"Vertex rawPredict body clears stream");
    same(body.value("anthropic_version",""),"vertex-2023-10-16","Vertex anthropic_version");
    check(!body.contains("betas"),"Vertex body drops betas");
    gcp::VertexConfig config;
    config.headers = {{"x-goog-user-project","billing-1"}};
    auto vertexInput = sampleBody();
    vertexInput["model"] = "claude-3-5-sonnet-v2@20241022";
    const auto request = gcp::prepareVertexRequest(config,"my-project","us-east5","ya29.token",vertexInput,true);
    same(request.url,"https://us-east5-aiplatform.googleapis.com/v1/projects/my-project/locations/us-east5/publishers/anthropic/"
                      "models/claude-3-5-sonnet-v2@20241022:streamRawPredict","regional streamRawPredict URL");
    same(header(request,"authorization"),"Bearer ya29.token","Vertex bearer token");
    same(header(request,"anthropic-beta"),"context-1m","betas become the anthropic-beta header");
    same(header(request,"x-goog-user-project"),"billing-1","custom Vertex headers");
    same(header(request,"accept"),"text/event-stream","SSE accept");
    vertexInput["model"] = "claude-opus-5-5";
    same(gcp::prepareVertexRequest(config,"p","global","t",vertexInput,false).url,
          "https://aiplatform.googleapis.com/v1/projects/p/locations/global/publishers/anthropic/models/claude-opus-5-5:rawPredict",
          "global rawPredict URL");
    throwsContaining([&] { gcp::prepareVertexRequest(config,"","global","t",vertexInput,true); },"project","project is required");
    same(gcp::vertexRegion(config),"global","default Vertex region");
    setenv("GOOGLE_CLOUD_LOCATION","europe-west1",1);
    same(gcp::vertexRegion(config),"europe-west1","GOOGLE_CLOUD_LOCATION");
    setenv("GOOGLE_CLOUD_PROJECT","env-project",1);
    same(gcp::vertexProject(config),"env-project","GOOGLE_CLOUD_PROJECT");
    config.project = "configured";
    config.region = "us-east5";
    same(gcp::vertexProject(config),"configured","endpoint project wins");
    same(gcp::vertexRegion(config),"us-east5","endpoint region wins");
    gcp::VertexClient client(config);
    check(client.listModels()["data"][0]["id"] == gcp::vertexClaudeModels().front(),"built-in Vertex model list");
    config.models = {"claude-custom"};
    gcp::VertexClient listed(config);
    check(listed.listModels() == Json::parse(R"({"data":[{"id":"claude-custom"}]})"),"endpoint models win");
  }
  // Google errors.
  {
    auto error = caught([] { cloud::throwGoogleError(429,R"({"error":{"code":429,"message":"Quota exceeded","status":"RESOURCE_EXHAUSTED"}})",Json::object()); });
    check(error.status == 429 && error.body["error"]["type"] == "rate_limit_error","RESOURCE_EXHAUSTED");
    error = caught([] { cloud::throwGoogleError(400,R"([{"error":{"code":400,"message":"bad","status":"INVALID_ARGUMENT"}}])",Json::object()); });
    same(error.body["error"]["type"],"invalid_request_error","array-wrapped Google error");
    error = caught([] { cloud::throwGoogleError(529,R"({"type":"error","error":{"type":"overloaded_error","message":"Overloaded"}})",Json::object()); });
    check(error.status == 529 && error.body["error"]["type"] == "overloaded_error","Anthropic-shaped body passes through");
  }
  // Service-account JWT: RS256 over header.claims, verifiable with the public key.
  {
    std::unique_ptr<EVP_PKEY_CTX,decltype(&EVP_PKEY_CTX_free)> context(EVP_PKEY_CTX_new_id(EVP_PKEY_RSA,nullptr),EVP_PKEY_CTX_free);
    EVP_PKEY* raw = nullptr;
    EVP_PKEY_keygen_init(context.get());
    EVP_PKEY_CTX_set_rsa_keygen_bits(context.get(),2048);
    EVP_PKEY_keygen(context.get(),&raw);
    std::unique_ptr<EVP_PKEY,decltype(&EVP_PKEY_free)> key(raw,EVP_PKEY_free);
    const Json account{{"type","service_account"},{"client_email","bot@p.iam.gserviceaccount.com"},{"private_key",pemOf(key.get(),true)},
                       {"private_key_id","kid-1"},{"token_uri","http://127.0.0.1:1/token"}};
    const auto jwt = gcp::serviceAccountAssertion(account,when);
    const auto first = jwt.find('.'), second = jwt.rfind('.');
    const auto header = Json::parse(base64UrlDecode(jwt.substr(0,first)));
    const auto claims = Json::parse(base64UrlDecode(jwt.substr(first+1,second-first-1)));
    check(header == Json::parse(R"({"alg":"RS256","typ":"JWT","kid":"kid-1"})"),"JWT header");
    same(claims["iss"],"bot@p.iam.gserviceaccount.com","JWT iss");
    same(claims["aud"],"http://127.0.0.1:1/token","JWT aud is the token URI");
    same(claims["scope"],"https://www.googleapis.com/auth/cloud-platform","JWT scope");
    check(claims["exp"].get<long long>()-claims["iat"].get<long long>() == 3600,"JWT lifetime");
    const auto signature = base64UrlDecode(jwt.substr(second+1));
    std::unique_ptr<EVP_MD_CTX,decltype(&EVP_MD_CTX_free)> verify(EVP_MD_CTX_new(),EVP_MD_CTX_free);
    EVP_DigestVerifyInit(verify.get(),nullptr,EVP_sha256(),nullptr,key.get());
    const auto input = jwt.substr(0,second);
    check(EVP_DigestVerify(verify.get(),reinterpret_cast<const unsigned char*>(signature.data()),signature.size(),
                           reinterpret_cast<const unsigned char*>(input.data()),input.size()) == 1,"JWT signature verifies");
    throwsContaining([&] { gcp::serviceAccountAssertion(Json{{"client_email","x"}},when); },"private_key","incomplete service account");
  }
  return cloud_check::finish("cloud-request-shaping");
}
