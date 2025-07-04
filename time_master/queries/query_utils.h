#ifndef QUERY_UTILS_H
#define QUERY_UTILS_H

#include <sqlite3.h>
#include <string>
#include <vector>
#include <map>
#include "common_utils.h" // For ProjectNode, ProjectTree
// This header declares the shared utility functions.
// Generates formatted output lines for the project hierarchy.
std::vector<std::string> generate_sorted_output(const ProjectNode& node, int avg_days = 1, int indent = 0);

// Fetches parent-child category mappings from the database.
std::map<std::string, std::string> get_parent_map(sqlite3* db);

// Builds a hierarchical project tree from a flat list of time records.
void build_project_tree_from_records(
    ProjectTree& tree,
    const std::vector<std::pair<std::string, long long>>& records,
    const std::map<std::string, std::string>& parent_map);

// Adds or subtracts days from a date string ("YYYYMMDD").
std::string add_days_to_date_str(std::string date_str, int days);

// Gets the current system date as "YYYYMMDD".
std::string get_current_date_str();

// 将项目分类数据写入字符串流
void write_project_breakdown_to_stream(
    std::stringstream& ss,
    sqlite3* db,
    const std::vector<std::pair<std::string, long long>>& records,
    long long total_duration,
    int avg_days = 1 // 默认为1，用于日报；月报和周期报告传入实际天数
);

#endif // QUERY_UTILS_H