#include "mcp/CollapseData.hpp"

namespace batbox::mcp {
const std::vector<std::string_view>& collapseSearchNames(){
  static const std::vector<std::string_view> names={
    "slack_search_public", "slack_search_public_and_private", "slack_search_channels", "slack_search_users", "search_code",
    "search_repositories", "search_issues", "search_pull_requests", "search_orgs", "search_users",
    "search_documentation", "search_logs", "search_spans", "search_rum_events", "search_audit_logs",
    "search_monitors", "search_monitor_groups", "find_slow_spans", "find_monitors_matching_pattern", "search_docs",
    "search_events", "search_issue_events", "find_organizations", "find_teams", "find_projects",
    "find_releases", "find_dsns", "search", "gmail_search_messages", "google_drive_search",
    "gcal_find_my_free_time", "gcal_find_meeting_times", "gcal_find_user_emails", "search_jira_issues_using_jql", "search_confluence_using_cql",
    "lookup_jira_account_id", "confluence_search", "jira_search", "jira_search_fields", "asana_search_tasks",
    "asana_typeahead_search", "search_files", "search_nodes", "brave_web_search", "brave_local_search",
    "search_dashboards", "search_folders", "search_docs", "search_stripe_resources", "search_stripe_documentation",
    "search_articles", "find_related_articles", "lookup_article_by_citation", "search_papers", "search_pubmed",
    "search_pubmed_key_words", "search_pubmed_advanced", "pubmed_search", "pubmed_mesh_lookup", "firecrawl_search",
    "web_search_exa", "web_search_advanced_exa", "people_search_exa", "linkedin_search_exa", "deep_search_exa",
    "perplexity_search", "perplexity_search_web", "tavily_search", "obsidian_simple_search", "obsidian_complex_search",
    "find", "search_knowledge", "search_memories", "find_memories_by_name", "search_records",
    "find_tasks", "find_tasks_by_date", "find_completed_tasks", "find_projects", "find_sections",
    "find_comments", "find_project_collaborators", "find_activity", "find_labels", "find_filters",
    "search_documentation", "search_catalog", "search_modules", "search_providers", "search_policies",
  };
  return names;
}
} // namespace batbox::mcp
