#include "api_errors/Internal.hpp"
#include "formatting/Format.hpp"

namespace batbox::api_errors::internal {
namespace {
constexpr double PDF_TARGET_RAW_SIZE=20*1024*1024;
constexpr int API_PDF_MAX_PAGES=100;
}
bool isCCRMode(Providers& p){return envTruthy(p,"BATBOX_REMOTE");}
std::string getPdfTooLargeErrorMessage(Providers& p){
  const std::string limits="max "+std::to_string(API_PDF_MAX_PAGES)+" pages, "+formatting::formatFileSize(PDF_TARGET_RAW_SIZE);
  return nonInteractive(p)
    ? "PDF too large ("+limits+"). Try reading the file a different way (e.g., extract text with pdftotext)."
    : "PDF too large ("+limits+"). Double press esc to go back and try again, or use pdftotext to convert to text first.";
}
std::string getPdfPasswordProtectedErrorMessage(Providers& p){
  return nonInteractive(p)
    ? "PDF is password protected. Try using a CLI tool to extract or convert the PDF."
    : "PDF is password protected. Please double press esc to edit your message and try again.";
}
std::string getPdfInvalidErrorMessage(Providers& p){
  return nonInteractive(p)
    ? "The PDF file was not valid. Try converting it to text first (e.g., pdftotext)."
    : "The PDF file was not valid. Double press esc to go back and try again with a different file.";
}
std::string getImageTooLargeErrorMessage(Providers& p){
  return nonInteractive(p)
    ? "Image was too large. Try resizing the image or using a different approach."
    : "Image was too large. Double press esc to go back and try again with a smaller image.";
}
std::string getRequestTooLargeErrorMessage(Providers& p){
  const std::string limits="max "+formatting::formatFileSize(PDF_TARGET_RAW_SIZE);
  return nonInteractive(p)
    ? "Request too large ("+limits+"). Try with a smaller file."
    : "Request too large ("+limits+"). Double press esc to go back and try with a smaller file.";
}
std::string getTokenRevokedErrorMessage(Providers& p){
  return nonInteractive(p)
    ? "Your account does not have access to Batbox. Please login again or contact your administrator."
    : TOKEN_REVOKED_ERROR_MESSAGE;
}
std::string getOauthOrgNotAllowedErrorMessage(Providers& p){
  return nonInteractive(p)
    ? "Your organization does not have access to Batbox. Please login again or contact your administrator."
    : OAUTH_ORG_NOT_ALLOWED_ERROR_MESSAGE;
}
std::optional<std::string> get3PModelFallbackSuggestion(const std::string& model,Providers& p){
  require(p.apiProvider,"apiProvider");
  if(p.apiProvider()=="firstParty")return std::nullopt;
  const std::string m=lower(model);
  if(contains(m,"opus-4-6")||contains(m,"opus_4_6")){
    require(p.modelStrings,"modelStrings");return field(p.modelStrings(),"opus41");
  }
  if(contains(m,"sonnet-4-6")||contains(m,"sonnet_4_6")){
    require(p.modelStrings,"modelStrings");return field(p.modelStrings(),"sonnet45");
  }
  if(contains(m,"sonnet-4-5")||contains(m,"sonnet_4_5")){
    require(p.modelStrings,"modelStrings");return field(p.modelStrings(),"sonnet40");
  }
  return std::nullopt;
}
} // namespace batbox::api_errors::internal
