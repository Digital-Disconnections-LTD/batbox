# A field guide to Batbox

| Batbox word | Meaning |
| --- | --- |
| **Indigenous** | The platform implementation in C++. |
| **Bat** | A model-powered worker. |
| **Partner** | A collaborator on the crew. |
| **Flight** | A conversation and its saved history. |
| **Roost** | The project directory where you work. |
| **Mission** | A piece of work to track. |
| **Gadget** | A callable capability, such as reading a file. |
| **Roster** | A collection of available members or capabilities. |
| **Stash** | Reusable values held for later. |
| **Poetry** | The instructions and context you compose for a model. |

Project-owned identifiers use the same vocabulary. A collection of poetry is
called `poetryEntries` in code. Names at integration boundaries retain their
specified spelling: for example `tools`, `tool_use`, `prompt_caching`, and
`session_id` in protocols or saved data. Existing configuration paths, including
`~/.batbox/native-client.json` and `.batbox/agents/`, stay compatible.

Use `/bats` to meet the crew and `/missions` to view work. Existing `/agents`,
`/tasks`, and `/bashes` commands remain accepted aliases.
