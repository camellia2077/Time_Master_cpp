#include "query_month.h"
#include "query_utils.h"
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <cctype>
#include <sstream>

// --- MonthlyReportGenerator Class Implementation ---

MonthlyReportGenerator::MonthlyReportGenerator(sqlite3* db, const std::string& year_month)
    : m_db(db), m_year_month(year_month) {}

std::string MonthlyReportGenerator::generate_report() {
    std::stringstream ss;
    if (!_validate_input()) {
        ss << "Invalid year_month format. Expected YYYYMM.\n";
        return ss.str();
    }

    _fetch_data();
    _display_summary(ss);

    if (m_actual_days == 0) {
        ss << "No time records found for this month.\n";
        return ss.str();
    }

    _display_project_breakdown(ss);
    return ss.str();
}

bool MonthlyReportGenerator::_validate_input() const {
    return m_year_month.length() == 6 && std::all_of(m_year_month.begin(), m_year_month.end(), ::isdigit);
}

void MonthlyReportGenerator::_fetch_data() {
    sqlite3_stmt* stmt;

    // 获取当月所有记录并计算总时长
    std::string sql_records = "SELECT project_path, duration FROM time_records WHERE SUBSTR(date, 1, 6) = ?;";
    if (sqlite3_prepare_v2(m_db, sql_records.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, m_year_month.c_str(), -1, SQLITE_STATIC);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            m_records.push_back({
                reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)),
                sqlite3_column_int64(stmt, 1)
            });
            m_total_duration += sqlite3_column_int64(stmt, 1);
        }
    }
    sqlite3_finalize(stmt);

    // 获取有记录的实际天数
    std::string sql_actual_days = "SELECT COUNT(DISTINCT date) FROM time_records WHERE SUBSTR(date, 1, 6) = ?;";
    if (sqlite3_prepare_v2(m_db, sql_actual_days.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, m_year_month.c_str(), -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            m_actual_days = sqlite3_column_int(stmt, 0);
        }
    }
    sqlite3_finalize(stmt);
}

void MonthlyReportGenerator::_display_summary(std::stringstream& ss) {
    ss << "\n--- Monthly Summary for " << m_year_month.substr(0, 4) << "-" << m_year_month.substr(4, 2) << " ---\n";
    if (m_actual_days > 0) {
        ss << "Actual Days with Records: " << m_actual_days << "\n";
        ss << "Total Time Recorded: " << time_format_duration(m_total_duration, m_actual_days) << "\n";
    }
    ss << "-------------------------------------\n";
}

void MonthlyReportGenerator::_display_project_breakdown(std::stringstream& ss) {
    // 调用公共函数，并将平均天数设置为当月有记录的实际天数
    write_project_breakdown_to_stream(ss, m_db, m_records, m_total_duration, m_actual_days);
}