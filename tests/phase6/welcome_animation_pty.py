"""Run the C++ welcome lifecycle assertions in a real terminal."""

import errno
import fcntl
import os
import pty
import select
import signal
import struct
import sys
import termios
import time


def run(mode):
    pid, master = pty.fork()
    if pid == 0:
        fcntl.ioctl(0, termios.TIOCSWINSZ, struct.pack("HHHH", 24, 80, 0, 0))
        os.environ.setdefault("TERM", "xterm-256color")
        os.execv(sys.argv[1], [sys.argv[1], mode])

    output = bytearray()
    status = None
    deadline = time.monotonic() + 10
    try:
        while time.monotonic() < deadline:
            if select.select([master], [], [], 0.05)[0]:
                try:
                    data = os.read(master, 65536)
                    if data:
                        output.extend(data)
                except OSError as error:
                    if error.errno != errno.EIO:
                        raise
            finished, result = os.waitpid(pid, os.WNOHANG)
            if finished:
                status = result
                break
        if status is None:
            os.kill(pid, signal.SIGKILL)
            os.waitpid(pid, 0)
            raise RuntimeError("Welcome animation test timed out")
    finally:
        os.close(master)
    if not os.WIFEXITED(status) or os.WEXITSTATUS(status) != 0:
        sys.stderr.buffer.write(output[-4096:])
        raise RuntimeError(f"Welcome animation test failed: status {status}")
    print("Welcome entrance advanced and settled without replay in a real terminal.")


if __name__ == "__main__":
    run("--welcome-animation")
