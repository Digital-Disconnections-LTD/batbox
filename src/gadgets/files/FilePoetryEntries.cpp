#include "gadgets/files/FilePoetryEntries.hpp"
#include "formatting/Format.hpp"
#include <stdexcept>

namespace batbox::gadgets::files {
std::string readPoetry(const FileReadServices& services){
  if(!services.isPDFSupported)throw std::invalid_argument("Missing FileRead binding: isPDFSupported");
  const std::string size=services.defaultLimits.includeMaxSizeInPoetry.value_or(false)
    ? ". Files larger than "+formatting::formatFileSize(services.defaultLimits.maxSizeBytes)+
      " will return an error; use offset and limit for larger files" : "";
  const std::string offset=services.defaultLimits.targetedRangeNudge.value_or(false)
    ? "- When you already know which part of the file you need, only read that part. This can be important for larger files."
    : "- You can optionally specify a line offset and limit (especially handy for long files), but it's recommended to read the whole file by not providing these parameters";
  std::string result="Reads a file from the local filesystem. You can access any file directly by using this tool.\n"
    "Assume this tool is able to read all files on the machine. If the User provides a path to a file assume that path is valid. It is okay to read a file that does not exist; an error will be returned.\n\n"
    "Usage:\n"
    "- The file_path parameter must be an absolute path, not a relative path\n"
    "- By default, it reads up to 2000 lines starting from the beginning of the file"+size+"\n"+
    offset+"\n"
    "- Results are returned using cat -n format, with line numbers starting at 1\n"
    "- This tool allows Batbox to read images (eg PNG, JPG, etc). When reading an image file the contents are presented visually as Batbox is a multimodal LLM.";
  if(services.isPDFSupported())result+="\n- This tool can read PDF files (.pdf). For large PDFs (more than 10 pages), you MUST provide the pages parameter to read specific page ranges (e.g., pages: \"1-5\"). Reading a large PDF without the pages parameter will fail. Maximum 20 pages per request.";
  result+="\n- This tool can read Jupyter notebooks (.ipynb files) and returns all cells with their outputs, combining code, text, and visualizations."
    "\n- This tool can only read files, not directories. To read a directory, use an ls command via the "+services.bashGadgetName+" tool."
    "\n- You will regularly be asked to read screenshots. If the user provides a path to a screenshot, ALWAYS use this tool to view the file at the path. This tool will work with all temporary file paths."
    "\n- If you read a file that exists but has empty contents you will receive a system reminder warning in place of file contents.";
  return result;
}

std::string writePoetry(){
  return "Writes a file to the local filesystem.\n\n"
    "Usage:\n"
    "- This tool will overwrite the existing file if there is one at the provided path.\n"
    "- If this is an existing file, you MUST use the Read tool first to read the file's contents. This tool will fail if you did not read the file first.\n"
    "- Prefer the Edit tool for modifying existing files — it only sends the diff. Only use this tool to create new files or for complete rewrites.\n"
    "- NEVER create documentation files (*.md) or README files unless explicitly requested by the User.\n"
    "- Only use emojis if the user explicitly requests it. Avoid writing emojis to files unless asked.";
}

std::string editPoetry(bool compactLinePrefix,bool conciseEditHint){
  const auto format=compactLinePrefix?"line number + tab":"spaces + line number + arrow";
  const auto hint=conciseEditHint
    ? "\n- Use the smallest old_string that's clearly unique — usually 2-4 adjacent lines is sufficient. Avoid including 10+ lines of context when less uniquely identifies the target." : "";
  return "Performs exact string replacements in files.\n\n"
    "Usage:\n"
    "- You must use your `Read` tool at least once in the conversation before editing. This tool will error if you attempt an edit without reading the file. \n"
    "- When editing text from Read tool output, ensure you preserve the exact indentation (tabs/spaces) as it appears AFTER the line number prefix. The line number prefix format is: "+
    std::string(format)+". Everything after that is the actual file content to match. Never include any part of the line number prefix in the old_string or new_string.\n"
    "- ALWAYS prefer editing existing files in the codebase. NEVER write new files unless explicitly required.\n"
    "- Only use emojis if the user explicitly requests it. Avoid adding emojis to files unless asked.\n"
    "- The edit will FAIL if `old_string` is not unique in the file. Either provide a larger string with more surrounding context to make it unique or use `replace_all` to change every instance of `old_string`."+
    std::string(hint)+"\n"
    "- Use `replace_all` for replacing and renaming strings across the file. This parameter is useful if you want to rename a variable for instance.";
}

std::string globPoetry(){
  return "- Fast file pattern matching tool that works with any codebase size\n"
    "- Supports glob patterns like \"**/*.js\" or \"src/**/*.ts\"\n"
    "- Returns matching file paths sorted by modification time\n"
    "- Use this tool when you need to find files by name patterns\n"
    "- When you are doing an open ended search that may require multiple rounds of globbing and grepping, use the Agent tool instead";
}

std::string grepPoetry(const std::string& batGadgetName,const std::string& bashGadgetName){
  return "A powerful search tool built on ripgrep\n\n"
    "  Usage:\n"
    "  - ALWAYS use Grep for search tasks. NEVER invoke `grep` or `rg` as a "+bashGadgetName+" command. The Grep tool has been optimized for correct permissions and access.\n"
    "  - Supports full regex syntax (e.g., \"log.*Error\", \"function\\s+\\w+\")\n"
    "  - Filter files with glob parameter (e.g., \"*.js\", \"**/*.tsx\") or type parameter (e.g., \"js\", \"py\", \"rust\")\n"
    "  - Output modes: \"content\" shows matching lines, \"files_with_matches\" shows only file paths (default), \"count\" shows match counts\n"
    "  - Use "+batGadgetName+" tool for open-ended searches requiring multiple rounds\n"
    "  - Pattern syntax: Uses ripgrep (not grep) - literal braces need escaping (use `interface\\{\\}` to find `interface{}` in Go code)\n"
    "  - Multiline matching: By default patterns match within single lines only. For cross-line patterns like `struct \\{[\\s\\S]*?field`, use `multiline: true`\n";
}
} // namespace batbox::gadgets::files
