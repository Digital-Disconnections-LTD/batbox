#include "messages/Messages.hpp"
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <vector>
using batbox::Json;
int main() {
  std::string line;
  while (std::getline(std::cin, line)) {
    try {
      Json input = Json::parse(line);
      std::size_t uuidIndex = 0;
      std::vector<std::string> logs;
      batbox::messages::Services services;
      services.randomUUID = [&] {
        std::ostringstream out; out << "00000000-0000-4000-8000-" << std::hex;
        out.width(12); out.fill('0'); out << ++uuidIndex; return out.str();
      };
      services.nowISO = [] { return "2026-03-31T12:34:56.000Z"; };
      services.strictGadgetResultPairing = [&] { return input.value("strict", false); };
      services.featureGate = [&] (const std::string& name) {
        if (name != "tengu_chair_sermon") throw std::runtime_error("unexpected feature gate");
        return input.value("smoosh", false);
      };
      services.logError = [&] (const std::string& error) { logs.push_back(error); };
      Json result;
      const std::string op = input.at("op").get<std::string>();
      if (op == "createUser") result = batbox::messages::createUserMessage(input.at("args"), services);
      else if (op == "createAssistant") result = batbox::messages::createAssistantMessage(input.at("args"), services);
      else if (op == "createAssistantError") result = batbox::messages::createAssistantAPIErrorMessage(input.at("args"), services);
      else if (op == "prepareUserContent") result = batbox::messages::prepareUserContent(input.at("args"));
      else if (op == "normalize") result = batbox::messages::normalizeMessages(input.at("messages"), services);
      else if (op == "pair") result = batbox::messages::ensureGadgetResultPairing(input.at("messages"), services);
      else if (op == "deriveUUID") result = batbox::messages::deriveUUID(input.at("uuid").get<std::string>(), input.at("index").get<std::size_t>());
      else throw std::runtime_error("unknown operation");
      std::cout << Json{{"result", result}, {"logs", logs}}.dump() << '\n';
    } catch (const std::exception& e) { std::cout << Json{{"error", e.what()}}.dump() << '\n'; }
  }
}
