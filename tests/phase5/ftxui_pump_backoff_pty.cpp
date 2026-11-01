#include "terminal_ftxui/Driver.hpp"
#include <ftxui/dom/elements.hpp>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <iostream>
#include <poll.h>
#include <signal.h>
#ifdef __APPLE__
#include <util.h>
#else
#include <pty.h>
#endif
#include <string>
#include <sys/wait.h>
#include <unistd.h>

using batbox::event_loop::Loop;
using batbox::terminal_ftxui::Callbacks;
using batbox::terminal_ftxui::Driver;
using batbox::terminal_ftxui::Options;

static void child(int reportFd) {
  double idle = 0, woken = 0, idleAgain = 0;
  int rendered = 0, renderedAtWake = 0, renderedAfterWake = 0;
  std::string input;
  try {
    Loop owner;
    Options options;
    options.pumpIntervalMs = 8;
    options.idlePumpIntervalMs = 32;
    options.quietPumpsBeforeBackoff = 4;
    auto driver = std::make_shared<Driver>(owner, options, Callbacks{
      .render = [&] { ++rendered; return ftxui::text("Batbox pump probe"); },
      .onRawInputBytes = [&](std::string_view bytes) { input.append(bytes); },
      .onEvent = [](const ftxui::Event&) {},
      .onResize = [](ftxui::Dimensions) {},
      .renderPlain = {},
      .writePlain = {},
      .onExit = [] {},
      .onFrameCommitted = [] {},
    });
    driver->start();
    auto t1 = owner.setTimeout(600, [&] {
      idle = driver->pumpIntervalMs();
      renderedAtWake = rendered;
      driver->requestRedraw();
      woken = driver->pumpIntervalMs();
    });
    auto t2 = owner.setTimeout(700, [&] { renderedAfterWake = rendered; });
    auto t3 = owner.setTimeout(1300, [&] { idleAgain = driver->pumpIntervalMs(); });
    auto t4 = owner.setTimeout(1800, [&] { driver->stop(); });
    owner.run();
    t1.cancel(); t2.cancel(); t3.cancel(); t4.cancel();
  } catch (const std::exception& error) {
    std::string message = std::string("ERR ") + error.what() + "\n";
    (void)!write(reportFd, message.data(), message.size());
    _exit(2);
  }
  const bool ok = idle == 32 && woken == 8 && renderedAfterWake > renderedAtWake &&
      idleAgain == 32 && input == "k";
  const std::string report = std::string(ok ? "PASS " : "FAIL ") +
      "idle=" + std::to_string(idle) + " woken=" + std::to_string(woken) +
      " renderedAtWake=" + std::to_string(renderedAtWake) +
      " renderedAfterWake=" + std::to_string(renderedAfterWake) +
      " idleAgain=" + std::to_string(idleAgain) + " input=" + input + "\n";
  (void)!write(reportFd, report.data(), report.size());
  _exit(ok ? 0 : 1);
}

int main() {
  int report[2];
  if (pipe(report) != 0) { std::cerr << strerror(errno) << '\n'; return 2; }
  int master = -1;
  winsize size{};
  size.ws_row = 24;
  size.ws_col = 80;
  const pid_t pid = forkpty(&master, nullptr, nullptr, &size);
  if (pid < 0) { std::cerr << strerror(errno) << '\n'; return 2; }
  if (pid == 0) { close(report[0]); child(report[1]); }
  close(report[1]);
  // A key typed while the pump is backed off must still be delivered.
  bool sent = false;
  const auto started = std::chrono::steady_clock::now();
  std::string diagnostic;
  for (;;) {
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - started).count();
    if (!sent && elapsed >= 1450) {
      (void)!write(master, "k", 1);
      sent = true;
    }
    pollfd fds[2] = {{master, POLLIN, 0}, {report[0], POLLIN, 0}};
    const int ready = poll(fds, 2, 25);
    if (ready < 0 && errno != EINTR) { std::cerr << strerror(errno) << '\n'; break; }
    if (fds[0].revents & POLLIN) {
      char bytes[65536];
      (void)!read(master, bytes, sizeof(bytes));
    }
    if (fds[1].revents & POLLIN) {
      char bytes[512];
      const auto n = read(report[0], bytes, sizeof(bytes));
      if (n > 0) diagnostic.append(bytes, static_cast<std::size_t>(n));
    }
    if (diagnostic.find('\n') != std::string::npos) break;
    if (elapsed > 10000) { ::kill(pid, SIGKILL); break; }
  }
  int status = 0;
  if (waitpid(pid, &status, 0) < 0) { std::cerr << strerror(errno) << '\n'; return 2; }
  close(master);
  close(report[0]);
  std::cout << diagnostic;
  if (diagnostic.rfind("PASS ", 0) != 0) return 2;
  return WIFEXITED(status) ? WEXITSTATUS(status) : 2;
}
