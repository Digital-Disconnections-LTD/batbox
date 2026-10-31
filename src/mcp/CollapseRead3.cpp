#include "mcp/CollapseData.hpp"

namespace batbox::mcp {
const std::vector<std::string_view>& collapseRead3Names(){
  static const std::vector<std::string_view> names={
    "get_advisors", "get_project_url", "get_publishable_keys", "generate_typescript_types", "list_edge_functions",
    "get_edge_function", "list_storage_buckets", "get_storage_config", "get_stripe_account_info", "retrieve_balance",
    "list_customers", "list_products", "list_prices", "list_invoices", "list_payment_intents",
    "list_subscriptions", "list_coupons", "list_disputes", "fetch_stripe_resources", "get_article_metadata",
    "get_full_text_article", "convert_article_ids", "get_copyright_status", "download_paper", "list_papers",
    "read_paper", "get_paper_fulltext", "get_pubmed_article_metadata", "download_pubmed_pdf", "pubmed_fetch",
    "pubmed_pmc_fetch", "pubmed_spell", "pubmed_cite", "pubmed_related", "bigquery_query",
    "bigquery_schema", "list_dataset_ids", "list_table_ids", "get_dataset_info", "get_table_info",
    "firecrawl_scrape", "firecrawl_map", "firecrawl_crawl", "firecrawl_check_crawl_status", "firecrawl_extract",
    "get_code_context_exa", "company_research_exa", "crawling_exa", "deep_researcher_check", "perplexity_ask",
    "perplexity_research", "perplexity_reason", "tavily_extract", "tavily_crawl", "tavily_map",
    "tavily_research", "obsidian_list_files_in_vault", "obsidian_list_files_in_dir", "obsidian_get_file_contents", "obsidian_batch_get_file_contents",
    "obsidian_get_periodic_note", "obsidian_get_recent_periodic_notes", "obsidian_get_recent_changes", "get_figma_data", "download_figma_images",
    "browser_console_messages", "browser_network_requests", "browser_take_screenshot", "browser_snapshot", "browser_get_config",
    "browser_route_list", "browser_cookie_list", "browser_cookie_get", "browser_localstorage_list", "browser_localstorage_get",
    "browser_sessionstorage_list", "browser_sessionstorage_get", "browser_storage_state", "puppeteer_screenshot", "list_databases",
    "list_collections", "collection_indexes", "collection_schema", "collection_storage_size", "db_stats",
    "explain", "mongodb_logs", "aggregate", "count", "export",
    "get_neo4j_schema", "read_neo4j_cypher", "list_instances", "get_instance_details", "get_instance_by_name",
    "list_indices", "get_mappings", "esql", "get_shards", "list_records",
    "list_bases", "get_record", "get_productivity_stats", "get_overview", "fetch_object",
    "user_info", "list_workspaces", "view_attachment", "get_available_services", "read_documentation",
    "read_sections", "recommend", "analyze_log_group", "analyze_metric", "describe_log_groups",
    "get_active_alarms", "get_alarm_history", "get_metric_data", "get_metric_metadata", "kubectl_get",
    "kubectl_describe", "kubectl_logs", "kubectl_context", "explain_resource", "list_api_resources",
    "namespaces_list", "nodes_log", "nodes_top", "pods_get", "pods_list",
    "pods_list_in_namespace", "pods_log", "pods_top", "resources_get", "resources_list",
  };
  return names;
}
} // namespace batbox::mcp
