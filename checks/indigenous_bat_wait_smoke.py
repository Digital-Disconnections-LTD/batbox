#!/usr/bin/env python3
"""A background Bat's launch must tell the model how to wait for its result.

The launch result says the result comes back and how to block on it with
TaskOutput instead of suggesting a tail of the raw transcript; TaskOutput then
returns the Bat's answer. The Agent and TaskOutput descriptions carry the rule.
"""
from pathlib import Path
import re
import sys
import tempfile

from gadget_fixture import require, run, text_of


BAT_SYSTEM_MARKER = "WAIT_FIXTURE_BAT_SYSTEM"
BAT_ANSWER = "bat-" + "answer-" + "ready"


def bat_reply(messages):
    for message in messages:
        if message.get("role") == "system" and BAT_SYSTEM_MARKER in text_of(message.get("content")):
            return BAT_ANSWER
    return None


def wait_arguments(messages):
    for message in messages:
        if message.get("role") == "tool" and message.get("tool_call_id") == "call_BATLAUNCH0001":
            found = re.search(r"agentId: (\S+)", text_of(message.get("content")))
            if found:
                return {"task_id": found.group(1), "block": True}
    raise RuntimeError("The background Bat launch returned no agentId")


STEPS = [
    ("call_BATLAUNCH0001", "Agent", {"description": "Fixture background Bat",
                                     "prompt": "Reply with the fixture answer.",
                                     "subagent_type": "wait-worker",
                                     "run_in_background": True}),
    ("call_BATWAIT000001", "TaskOutput", wait_arguments),
]


def main(binary):
    with tempfile.TemporaryDirectory(prefix="batbox-bat-wait-") as temporary:
        root = Path(temporary)
        work = root / "work"
        agents = work / ".batbox" / "agents"
        agents.mkdir(parents=True)
        (agents / "wait-worker.md").write_text(
            "---\nname: wait-worker\ndescription: Fixture worker\n---\n" + BAT_SYSTEM_MARKER + "\n",
            encoding="utf-8")
        results, tools = run(binary, root, work, "wait", STEPS, ["--yolo"], side_reply=bat_reply)

        launch = results["call_BATLAUNCH0001"]
        require("TaskOutput" in launch and "block: true" in launch and
                "Do not launch another agent" in launch,
                f"Background launch must say how to wait: {launch!r}")
        require("tail" not in launch and "output_file" not in launch,
                f"Background launch must not suggest reading the raw transcript: {launch!r}")
        waited = results["call_BATWAIT000001"]
        require("<retrieval_status>success</retrieval_status>" in waited and BAT_ANSWER in waited,
                f"Waiting on the Bat must return its result: {waited!r}")

        agent = tools.get("Agent", "")
        require("block: true" in agent and "do not launch another agent" in agent,
                f"Agent description must carry the wait rule: {agent!r}")
        task_output = tools.get("TaskOutput", "")
        require("DEPRECATED" not in task_output and "one hour" in task_output and
                "still running, not failed" in task_output,
                f"TaskOutput description must explain waiting: {task_output!r}")
    print("A background Bat's launch says how to wait, and TaskOutput returns its result")


if __name__ == "__main__":
    main(Path(sys.argv[1]).resolve(strict=True))
