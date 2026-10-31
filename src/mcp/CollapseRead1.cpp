#include "mcp/CollapseData.hpp"

namespace batbox::mcp {
const std::vector<std::string_view>& collapseRead1Names(){
  static const std::vector<std::string_view> names={
    "slack_read_channel", "slack_read_thread", "slack_read_canvas", "slack_read_user_profile", "slack_list_channels",
    "slack_get_channel_history", "slack_get_thread_replies", "slack_get_users", "slack_get_user_profile", "get_me",
    "get_team_members", "get_teams", "get_commit", "get_file_contents", "get_repository_tree",
    "list_branches", "list_commits", "list_releases", "list_tags", "get_latest_release",
    "get_release_by_tag", "get_tag", "list_issues", "issue_read", "list_issue_types",
    "get_label", "list_label", "pull_request_read", "get_gist", "list_gists",
    "list_notifications", "get_notification_details", "projects_list", "projects_get", "actions_get",
    "actions_list", "get_job_logs", "get_code_scanning_alert", "list_code_scanning_alerts", "get_dependabot_alert",
    "list_dependabot_alerts", "get_secret_scanning_alert", "list_secret_scanning_alerts", "get_global_security_advisory", "list_global_security_advisories",
    "list_org_repository_security_advisories", "list_repository_security_advisories", "get_discussion", "get_discussion_comments", "list_discussion_categories",
    "list_discussions", "list_starred_repositories", "get_issue", "get_pull_request", "list_pull_requests",
    "get_pull_request_files", "get_pull_request_status", "get_pull_request_comments", "get_pull_request_reviews", "list_comments",
    "list_cycles", "get_document", "list_documents", "list_issue_statuses", "get_issue_status",
    "list_my_issues", "list_issue_labels", "list_projects", "get_project", "list_project_labels",
    "list_teams", "get_team", "list_users", "get_user", "aggregate_logs",
    "list_spans", "aggregate_spans", "analyze_trace", "trace_critical_path", "query_metrics",
    "aggregate_rum_events", "list_rum_metrics", "get_rum_metric", "list_monitors", "get_monitor",
    "check_can_delete_monitor", "validate_monitor", "validate_existing_monitor", "list_dashboards", "get_dashboard",
    "query_dashboard_widget", "list_notebooks", "get_notebook", "query_notebook_cell", "get_profiling_metrics",
    "compare_profiling_metrics", "whoami", "get_issue_details", "get_issue_tag_values", "get_trace_details",
    "get_event_attachment", "get_doc", "get_sentry_resource", "list_events", "list_issue_events",
    "get_sentry_issue", "fetch", "get_comments", "get_users", "get_self",
    "gmail_get_profile", "gmail_read_message", "gmail_read_thread", "gmail_list_drafts", "gmail_list_labels",
    "google_drive_fetch", "google_drive_export", "gcal_list_calendars", "gcal_list_events", "gcal_get_event",
    "atlassian_user_info", "get_accessible_atlassian_resources", "get_visible_jira_projects", "get_jira_project_issue_types_metadata", "get_jira_issue",
    "get_transitions_for_jira_issue", "get_jira_issue_remote_issue_links", "get_confluence_spaces", "get_confluence_page", "get_pages_in_confluence_space",
    "get_confluence_page_ancestors", "get_confluence_page_descendants", "get_confluence_page_footer_comments", "get_confluence_page_inline_comments", "confluence_get_page",
  };
  return names;
}
} // namespace batbox::mcp
