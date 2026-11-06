#include "transport/Perform.hpp"
#include "transport/Errors.hpp"
#include <string>

namespace batbox::transport {
namespace {
struct Transfer {
  CURLM* multi = nullptr;
  CURL* easy = nullptr;
  bool added = false;
  ~Transfer() {
    if (added) curl_multi_remove_handle(multi,easy);
    if (multi) curl_multi_cleanup(multi);
  }
};
[[noreturn]] void fail(CURLMcode code) {
  throw APIConnectionError(std::string("HTTP transfer failed: ")+curl_multi_strerror(code));
}
}
CURLcode performStoppable(CURL* easy, std::stop_token stop) {
  if (!stop.stop_possible()) return curl_easy_perform(easy);
  Transfer transfer;
  transfer.easy = easy;
  transfer.multi = curl_multi_init();
  if (!transfer.multi) throw APIConnectionError("Cannot create HTTP transfer");
  if (const auto code = curl_multi_add_handle(transfer.multi,easy); code != CURLM_OK) fail(code);
  transfer.added = true;
  // Destroyed before transfer, so no wakeup can reach a cleaned-up handle.
  std::stop_callback wake(stop,[multi = transfer.multi] { curl_multi_wakeup(multi); });
  for (;;) {
    if (stop.stop_requested()) return CURLE_ABORTED_BY_CALLBACK;
    int running = 0;
    if (const auto code = curl_multi_perform(transfer.multi,&running); code != CURLM_OK) fail(code);
    if (!running) break;
    if (stop.stop_requested()) return CURLE_ABORTED_BY_CALLBACK;
    if (const auto code = curl_multi_poll(transfer.multi,nullptr,0,1000,nullptr); code != CURLM_OK) fail(code);
  }
  CURLcode result = CURLE_OK;
  int left = 0;
  while (const CURLMsg* message = curl_multi_info_read(transfer.multi,&left))
    if (message->msg == CURLMSG_DONE && message->easy_handle == easy) result = message->data.result;
  return result;
}
} // namespace batbox::transport
