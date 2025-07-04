#include "query_day.h"
#include "query_utils.h"
#include <iomanip>
#include <algorithm>
#include <sstream>

// --- DailyReportGenerator Class Implementation ---

DailyReportGenerator::DailyReportGenerator(sqlite3* db, const std::string& date)
    : m_db(db), m_date(date) {}

std::string DailyReportGenerator::generate_report() {
    std::stringstream ss;

    // 1. 获取所有需要的数据
    _fetch_metadata();
    _fetch_total_duration();

    // 2. 写入报告头部
    _display_header(ss);

    // 3. 如果当天没有记录，则提前结束
    if (m_total_duration == 0) {
        ss << "No time records for this day.\n";
        return ss.str();
    }

    // 4. 获取并写入项目分类详情
    _fetch_time_records();
    _display_project_breakdown(ss);

    return ss.str();
}

void DailyReportGenerator::_fetch_metadata() {
    sqlite3_stmt* stmt;
    std::string sql = "SELECT status, remark, getup_time FROM days WHERE date = ?;";
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, m_date.c_str(), -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const unsigned char* s = sqlite3_column_text(stmt, 0);
            if (s) m_metadata.status = reinterpret_cast<const char*>(s);
            const unsigned char* r = sqlite3_column_text(stmt, 1);
            if (r) m_metadata.remark = reinterpret_cast<const char*>(r);
            const unsigned char* g = sqlite3_column_text(stmt, 2);
            if (g) m_metadata.getup_time = reinterpret_cast<const char*>(g);
        }
    }
    sqlite3_finalize(stmt);
}

void DailyReportGenerator::_fetch_total_duration() {
    sqlite3_stmt* stmt;
    std::string sql = "SELECT SUM(duration) FROM time_records WHERE date = ?;";
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, m_date.c_str(), -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_ROW && sqlite3_column_type(stmt, 0) != SQLITE_NULL) {
            m_total_duration = sqlite3_column_int64(stmt, 0);
        }
    }
    sqlite3_finalize(stmt);
}

void DailyReportGenerator::_fetch_time_records() {
    sqlite3_stmt* stmt;
    std::string sql = "SELECT project_path, duration FROM time_records WHERE date = ?;";
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, m_date.c_str(), -1, SQLITE_STATIC);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            m_records.push_back({
                reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)),
                sqlite3_column_int64(stmt, 1)
            });
        }
    }
    sqlite3_finalize(stmt);
}

void DailyReportGenerator::_display_header(std::stringstream& ss) {
    ss << "\n--- Daily Report for " << m_date << " ---\n";
    ss << "Date: " << m_date << "\n";
    ss << "Total Time Recorded: " << time_format_duration(m_total_duration) << "\n";
    ss << "Status: " << m_metadata.status << "\n";
    ss << "Getup Time: " << m_metadata.getup_time << "\n";
    ss << "Remark: " << m_metadata.remark << "\n";
    ss << "-------------------------------------\n";
}

void DailyReportGenerator::_display_project_breakdown(std::stringstream& ss) {
    ProjectTree project_tree;
    std::map<std::string, std::string> parent_map = get_parent_map(m_db);
    build_project_tree_from_records(project_tree, m_records, parent_map);

    std::vector<std::pair<std::string, ProjectNode>> sorted_top_level;
    for (const auto& pair : project_tree) {
        sorted_top_level.push_back(pair);
    }
    std::sort(sorted_top_level.begin(), sorted_top_level.end(), [](const auto& a, const auto& b) {
        return a.second.duration > b.second.duration;
    });

    for (const auto& pair : sorted_top_level) {
        const std::string& category_name = pair.first;
        const ProjectNode& category_node = pair.second;
        double percentage = (static_cast<double>(category_node.duration) / m_total_duration * 100.0);
        
        ss << "\n## " << category_name << ": "
           << time_format_duration(category_node.duration)
           << " (" << std::fixed << std::setprecision(1) << percentage << "%) ##\n";

        std::vector<std::string> output_lines = generate_sorted_output(category_node, 1);
        for (const auto& line : output_lines) {
            ss << line << "\n";
        }
    }
}