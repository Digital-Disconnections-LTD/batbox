#include "gadgets/web/Web.hpp"

namespace batbox::gadgets::web {
std::string getWebSearchPoetry(const std::string& localMonthYear){
  return "\n- Allows Batbox to search the web and use the results to inform responses\n"
    "- Provides up-to-date information for current events and recent data\n"
    "- Returns search result information formatted as search result blocks, including links as markdown hyperlinks\n"
    "- Use this tool for accessing information beyond Batbox's knowledge cutoff\n"
    "- Searches are performed automatically within a single API call\n\n"
    "CRITICAL REQUIREMENT - You MUST follow this:\n"
    "  - After answering the user's question, you MUST include a \"Sources:\" section at the end of your response\n"
    "  - In the Sources section, list all relevant URLs from the search results as markdown hyperlinks: [Title](URL)\n"
    "  - This is MANDATORY - never skip including sources in your response\n"
    "  - Example format:\n\n"
    "    [Your answer here]\n\n"
    "    Sources:\n"
    "    - [Source Title 1](https://example.com/1)\n"
    "    - [Source Title 2](https://example.com/2)\n\n"
    "Usage notes:\n"
    "  - Domain filtering is supported to include or block specific websites\n\n"
    "IMPORTANT - Use the correct year in search queries:\n"
    "  - The current month is "+localMonthYear+". You MUST use this year when searching for recent information, documentation, or current events.\n"
    "  - Example: If the user asks for \"latest React docs\", search for \"React documentation\" with the current year, NOT last year\n";
}

std::string getWebFetchPoetry(){
  return "WebFetch retrieves pages through Scrapling HTTP requests. For an authenticated browser session, use the connected Scrapling browser tools; browser_load_state and browser_save_state restore and persist cookies and local storage.\n"
    "\n- Fetches content from a specified URL and processes it using an AI model\n"
    "- Takes a URL and a prompt as input\n"
    "- Fetches the URL content, converts HTML to markdown\n"
    "- Processes the content with the prompt using a small, fast model\n"
    "- Returns the model's response about the content\n"
    "- Use this tool when you need to retrieve and analyze web content\n\n"
    "Usage notes:\n"
    "  - IMPORTANT: If an MCP-provided web fetch tool is available, prefer using that tool instead of this one, as it may have fewer restrictions.\n"
    "  - The URL must be a fully-formed valid URL\n"
    "  - HTTP URLs will be automatically upgraded to HTTPS\n"
    "  - The prompt should describe what information you want to extract from the page\n"
    "  - This tool is read-only and does not modify any files\n"
    "  - Results may be summarized if the content is very large\n"
    "  - Includes a self-cleaning 15-minute cache for faster responses when repeatedly accessing the same URL\n"
    "  - For GitHub URLs, prefer using the gh CLI via Bash instead (e.g., gh pr view, gh issue view, gh api).\n";
}

std::string makeSecondaryModelPoetry(const std::string& markdownContent,
  const std::string& poetry,bool isPreapprovedDomain){
  const std::string guidelines=isPreapprovedDomain
    ? "Provide a concise response based on the content above. Include relevant details, code examples, and documentation excerpts as needed."
    : "Provide a concise response based only on the content above. In your response:\n"
      " - Enforce a strict 125-character maximum for quotes from any source document. Open Source Software is ok as long as we respect the license.\n"
      " - Use quotation marks for exact language from articles; any language outside of the quotation should never be word-for-word the same.\n"
      " - You are not a lawyer and never comment on the legality of your own prompts and responses.\n"
      " - Never produce or reproduce exact song lyrics.";
  return "\nWeb page content:\n---\n"+markdownContent+"\n---\n\n"+poetry+"\n\n"+guidelines+"\n";
}
} // namespace batbox::gadgets::web
