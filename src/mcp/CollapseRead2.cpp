#include "mcp/CollapseData.hpp"

namespace batbox::mcp {
const std::vector<std::string_view>& collapseRead2Names(){
  static const std::vector<std::string_view> names={
    "confluence_get_page_children", "confluence_get_comments", "confluence_get_labels", "jira_get_issue", "jira_get_transitions",
    "jira_get_worklog", "jira_get_agile_boards", "jira_get_board_issues", "jira_get_sprints_from_board", "jira_get_sprint_issues",
    "jira_get_link_types", "jira_download_attachments", "jira_batch_get_changelogs", "jira_get_user_profile", "jira_get_project_issues",
    "jira_get_project_versions", "asana_get_attachment", "asana_get_attachments_for_object", "asana_get_goal", "asana_get_goals",
    "asana_get_parent_goals_for_goal", "asana_get_portfolio", "asana_get_portfolios", "asana_get_items_for_portfolio", "asana_get_project",
    "asana_get_projects", "asana_get_project_sections", "asana_get_project_status", "asana_get_project_statuses", "asana_get_project_task_counts",
    "asana_get_projects_for_team", "asana_get_projects_for_workspace", "asana_get_task", "asana_get_tasks", "asana_get_stories_for_task",
    "asana_get_teams_for_workspace", "asana_get_teams_for_user", "asana_get_team_users", "asana_get_time_period", "asana_get_time_periods",
    "asana_get_user", "asana_get_workspace_users", "asana_list_workspaces", "read_file", "read_text_file",
    "read_media_file", "read_multiple_files", "list_directory", "list_directory_with_sizes", "directory_tree",
    "get_file_info", "list_allowed_directories", "read_graph", "open_nodes", "query",
    "read_query", "list_tables", "describe_table", "git_status", "git_diff",
    "git_diff_unstaged", "git_diff_staged", "git_log", "git_show", "git_branch",
    "list_teams", "list_users_by_org", "get_dashboard_by_uid", "get_dashboard_summary", "get_dashboard_property",
    "get_dashboard_panel_queries", "run_panel_query", "list_datasources", "get_datasource", "get_query_examples",
    "query_prometheus", "query_prometheus_histogram", "list_prometheus_metric_metadata", "list_prometheus_metric_names", "list_prometheus_label_names",
    "list_prometheus_label_values", "query_loki_logs", "query_loki_stats", "query_loki_patterns", "list_loki_label_names",
    "list_loki_label_values", "list_incidents", "get_incident", "list_sift_investigations", "get_sift_investigation",
    "get_sift_analysis", "list_oncall_schedules", "get_oncall_shift", "get_current_oncall_users", "list_oncall_teams",
    "list_oncall_users", "list_alert_groups", "get_alert_group", "get_annotations", "get_annotation_tags",
    "get_panel_image", "list_incidents", "get_incident", "get_outlier_incident", "get_past_incidents",
    "get_related_incidents", "list_incident_notes", "list_incident_workflows", "get_incident_workflow", "list_services",
    "get_service", "list_team_members", "get_user_data", "list_schedules", "get_schedule",
    "list_schedule_users", "list_oncalls", "list_log_entries", "get_log_entry", "list_escalation_policies",
    "get_escalation_policy", "list_event_orchestrations", "get_event_orchestration", "list_status_pages", "get_status_page_post",
    "list_alerts_from_incident", "get_alert_from_incident", "list_change_events", "get_change_event", "list_organizations",
    "get_organization", "get_cost", "list_extensions", "list_migrations", "get_logs",
  };
  return names;
}
} // namespace batbox::mcp
