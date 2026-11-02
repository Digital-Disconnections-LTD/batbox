// The embedded provider catalog parses, carries the promised presets, and
// every supported entry turns into an endpoint record the store accepts.
#include "application_config/EndpointStore.hpp"
#include "providers/Catalog.hpp"
#include "providers/Onboarding.hpp"
#include "providers/WireClients.hpp"
#include "transport/Errors.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#ifndef _WIN32
#include <sys/stat.h>
#endif

using namespace batbox;
namespace bp=batbox::providers;

namespace {
void check(bool condition,const std::string& reason){
  if(!condition)throw std::runtime_error(reason);
}
bp::Environment fakeEnvironment(std::map<std::string,std::string> values){
  return [values](const std::string& name)->std::optional<std::string>{
    const auto found=values.find(name);
    if(found==values.end())return std::nullopt;
    return found->second;
  };
}
}

int main(){
  try{
    const auto& catalog=bp::bundledCatalog();
    check(catalog.providers.size()>=235,"catalog lost providers");
    check(bp::bundledCatalogText().size()==bp::catalogBytes(),"chunk byte count");
    for(const auto* id:{"opencode","llama.cpp","ollama","lmstudio","vllm","openrouter",
        "anthropic","openai","groq","deepseek","mistral","xai","togetherai","fireworks-ai",
        "cerebras","moonshotai","zai","minimax","alibaba","github-copilot","amazon-bedrock",
        "google-vertex"})
      check(catalog.find(id)!=nullptr,std::string("missing provider ")+id);
    check(catalog.find("together")==catalog.find("togetherai"),"together alias");
    check(catalog.find("zen")->id=="opencode","zen alias");
    check(catalog.find("LLAMACPP")->id=="llama.cpp","case-insensitive alias");
    const auto& zen=*catalog.find("opencode");
    check(zen.kind=="anthropic"&&zen.baseUrl=="https://opencode.ai/inference"&&
      zen.messagesPath=="/anthropic/v1/messages","Zen preset");
    check(catalog.find("anthropic")->authHeader=="x-api-key","Anthropic uses x-api-key");
    check(catalog.find("openai")->kind=="openai-chat"&&
      catalog.find("openai")->quirks.value("max_tokens_field","")=="max_completion_tokens",
      "OpenAI chat quirks");
    check(catalog.find("amazon-bedrock")->kind=="bedrock","Bedrock kind");
    check(catalog.find("google-vertex")->kind=="vertex","Vertex kind");
    check(catalog.find("github-copilot")->comingSoon(),"Copilot waits for device sign-in");
    check(catalog.find("llama.cpp")->ports==std::vector<int>({9931,8080}),"llama.cpp ports");
    std::size_t unsupported=0;
    for(const auto& provider:catalog.providers){
      check(provider.models.size()<=40,provider.id+" has too many models");
      if(!provider.supported()){
        ++unsupported;check(!provider.reason.empty(),provider.id+" needs a reason");continue;
      }
      if(bp::needsBaseUrlEdit(provider.baseUrl)||provider.baseUrl.find("${")!=std::string::npos)
        continue;
      bp::Setup setup;
      setup.model=provider.models.empty()?"local-model":provider.models.front().id;
      if(bp::takesKey(provider)&&!provider.keyEnv.empty())setup.keyEnv=provider.keyEnv.front();
      const auto endpoint=bp::buildEndpoint(provider,setup);
      application_config::validateEndpoint(provider.id,endpoint);
      check(endpoint.provider==provider.kind&&endpoint.preset==provider.id,
        provider.id+" lost its kind or preset");
      check(endpoint.quirks.empty()==(provider.kind!="openai-chat"),
        provider.id+" quirks belong to openai-chat only");
    }
    check(unsupported>0&&unsupported<40,"unexpected unsupported count");

    const auto* claude=catalog.find("anthropic")->model("claude-sonnet-5-5");
    check(claude&&claude->toolCall&&claude->reasoning,"Claude Sonnet flags");
    check(bp::describeModel(*claude)=="1M context · 128K output · tools · reasoning · images",
      "model description: "+bp::describeModel(*claude));
    check(bp::compactTokens(131072)=="128K"&&bp::compactTokens(65536)=="64K"&&
      bp::compactTokens(1048576)=="1M"&&bp::compactTokens(200000)=="200K"&&
      bp::compactTokens(1500000)=="1.5M"&&bp::compactTokens(900)=="900","token formatting");

    // URL placeholders resolve from the environment or stay for editing.
    const auto& databricks=*catalog.find("databricks");
    check(bp::resolvedBaseUrl(databricks,fakeEnvironment({{"DATABRICKS_HOST","dbc.example.com"}}))==
      "https://dbc.example.com/ai-gateway/mlflow","placeholder substitution");
    check(bp::needsBaseUrlEdit("https://{resource}.services.ai.azure.com"),"brace placeholder");
    check(bp::presentKeyEnv(*catalog.find("openrouter"),
      fakeEnvironment({{"OPENROUTER_API_KEY","x"}}))==std::vector<std::string>{"OPENROUTER_API_KEY"},
      "key environment detection");

    // Local servers: no key means no auth header; live models and contexts win.
    bp::Setup local;local.model="qwen3-coder:30b";
    local.liveModels={"qwen3-coder:30b","gemma4:31b"};local.liveContexts["qwen3-coder:30b"]=32768;
    local.baseUrl="http://127.0.0.1:11434";
    const auto ollama=bp::buildEndpoint(*catalog.find("ollama"),local);
    check(ollama.authHeader=="none"&&ollama.contextWindow==32768&&ollama.maxOutputTokens==16384&&
      ollama.models==std::vector<std::string>({"qwen3-coder:30b","gemma4:31b"}),"Ollama endpoint");
    bp::Setup keyed;keyed.model="claude-sonnet-5-5";keyed.key="sk-test";
    const auto anthropic=bp::buildEndpoint(*catalog.find("anthropic"),keyed);
    check(anthropic.authToken=="sk-test"&&anthropic.apiKeyEnv.empty()&&anthropic.thinking&&
      anthropic.poetryStashing&&anthropic.contextWindow==1000000,"Anthropic endpoint");
    bool rejected=false;
    try{bp::buildEndpoint(*catalog.find("perplexity"),keyed);}catch(const std::invalid_argument&){rejected=true;}
    check(rejected,"unsupported providers cannot be saved");

    // Ids, saving, selection and private permissions.
    std::map<std::string,application::EndpointRecord> existing;
    existing["openrouter"]=anthropic;
    check(bp::uniqueEndpointId(existing,"openrouter")=="openrouter-2","second id");
    check(bp::uniqueEndpointId(existing,"groq")=="groq","free id");
    const auto directory=std::filesystem::temp_directory_path()/
      ("batbox-providers-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(directory);
    const auto path=directory/"native-client.json";
    std::ofstream(path)<<R"({"thinking":false,"auth_title":true})";
    const auto saved=bp::saveEndpoint(path,{},{},"ollama",ollama,"gemma4:31b");
    check(saved.profile=="ollama::gemma4:31b","saved profile id");
    std::ifstream input(path);const auto document=Json::parse(input);
    check(document.at("profile")=="ollama::gemma4:31b"&&document.at("thinking")==false,
      "selection persisted and unrelated keys kept");
    const auto& row=document.at("endpoints").at("ollama");
    check(!row.contains("provider")&&row.at("preset")=="ollama"&&
      row.at("auth_header")=="none"&&!row.contains("quirks"),"saved Ollama row");
#ifndef _WIN32
    struct stat info{};
    check(::stat(path.c_str(),&info)==0&&(info.st_mode&0777)==0600,"config must stay 0600");
#endif
    const auto reread=application_config::readEndpoints(path);
    check(reread&&reread->at("ollama").preset=="ollama","endpoint store reads the preset back");
    std::filesystem::remove_all(directory);

    // Connection test messages, without any network: the listing seam is
    // WireClient::listModels, injected here.
    application::EndpointRecord probe=ollama;
    const auto listing=[](std::string body){
      return [body](const application::EndpointRecord&){return Json::parse(body);};
    };
    const auto refusing=[](long status){
      return [status](const application::EndpointRecord&)->Json{
        throw transport::APIError(status,nullptr,Json::object(),"refused");};
    };
    auto result=bp::testConnection(probe,listing(R"({"data":[{"id":"a"},{"id":"b"},{"id":"a"}]})"));
    check(result.ok&&result.models==std::vector<std::string>({"a","b"})&&
      result.message=="Connected to 127.0.0.1:11434 · 2 models","live model list: "+result.message);
    result=bp::testConnection(probe,refusing(401));
    check(!result.ok&&result.message.find("rejected the key")!=std::string::npos,"401 message");
    result=bp::testConnection(probe,refusing(404));
    check(!result.ok&&result.message.find("HTTP 404")!=std::string::npos,"404 message");
    result=bp::testConnection(probe,[](const application::EndpointRecord&)->Json{
      throw transport::APIConnectionError("refused");});
    check(!result.reached&&result.message=="Could not connect to 127.0.0.1:11434. Is the server running?",
      "offline message: "+result.message);
    result=bp::testConnection(probe,listing(R"({"object":"list"})"));
    check(!result.ok&&result.message=="The server answered but sent no model list.","no data array");
    // The live listing gets the same auth a request sends.
    const auto keyedConfig=bp::endpointConfig(anthropic);
    check(transport::modelsUrl(keyedConfig.baseUrl,keyedConfig.modelsPath)==
      "https://api.anthropic.com/v1/models","Anthropic models URL");
    check(keyedConfig.headers.at("x-api-key")=="sk-test"&&
      keyedConfig.headers.at("anthropic-version")=="2023-06-01"&&keyedConfig.authToken.empty(),
      "Anthropic listing headers");
    bp::Setup chatKey;chatKey.model="llama-3.3-70b-versatile";chatKey.key="gsk-test";
    const auto groqConfig=bp::endpointConfig(bp::buildEndpoint(*catalog.find("groq"),chatKey));
    check(groqConfig.authToken=="gsk-test"&&!groqConfig.headers.contains("anthropic-version"),
      "openai-chat listing uses a bearer token");
    check(bp::endpointConfig(ollama).authToken.empty(),"no key for a keyless local server");
    check(bp::listingTarget(ollama)=="GET http://127.0.0.1:11434/v1/models","listing target");

    // Bedrock and Vertex endpoints carry the region from the catalog URL and
    // go through the same listing; Vertex answers without a request.
    check(bp::cloudRegion("bedrock","https://bedrock-runtime.eu-west-3.amazonaws.com")=="eu-west-3"&&
      bp::cloudRegion("vertex","https://us-east5-aiplatform.googleapis.com")=="us-east5"&&
      bp::cloudRegion("vertex","https://aiplatform.googleapis.com")=="global"&&
      bp::cloudRegion("bedrock","https://bedrock.example.com").empty()&&
      bp::cloudRegion("openai-chat","https://bedrock-runtime.us-east-1.amazonaws.com").empty(),
      "cloud regions from URLs");
    bp::Setup cloudSetup;cloudSetup.model="claude-sonnet-5-5@default";
    const auto vertex=bp::buildEndpoint(*catalog.find("google-vertex-anthropic"),cloudSetup);
    check(vertex.provider=="vertex"&&vertex.region=="us-east5","Vertex region "+vertex.region);
    result=bp::testConnection(vertex);
    check(result.ok&&!result.models.empty()&&result.models.front()=="claude-sonnet-5-5@default"&&
      result.message.find("Vertex AI has no model list call")==0,"Vertex listing: "+result.message);
    cloudSetup.model="us.anthropic.claude-sonnet-5-5-v1:0";
    const auto bedrock=bp::buildEndpoint(*catalog.find("amazon-bedrock"),cloudSetup);
    check(bedrock.provider=="bedrock"&&bedrock.region=="us-east-1"&&
      bp::listingTarget(bedrock)=="Amazon Bedrock in us-east-1 (signed with your AWS credentials)",
      "Bedrock endpoint");
    result=bp::testConnection(bedrock,refusing(403));
    check(!result.ok&&result.message=="Amazon Bedrock did not accept your credentials (HTTP 403): refused",
      "Bedrock credentials message: "+result.message);
    std::cout<<"provider catalog: "<<catalog.providers.size()<<" providers, "
      <<bp::catalogBytes()<<" bytes\n";
  }catch(const std::exception& error){
    std::cerr<<error.what()<<"\n";
    return 1;
  }
}
