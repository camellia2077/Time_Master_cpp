#ifndef QUERY_MONTH_H
#define QUERY_MONTH_H

#include <sqlite3.h>
#include <string>
#include <vector>
#include <sstream> // 新增
#include "common_utils.h" // For ProjectTree etc.

/**
 * @class MonthlyReportGenerator
 * @brief 负责生成月度总结报告的类。
 *
 * 该类封装了验证输入、获取数据、处理数据和显示报告的所有逻辑。
 */
class MonthlyReportGenerator {
public:
    // 构造函数，接收数据库连接和要查询的月份 (格式 YYYYMM)
    explicit MonthlyReportGenerator(sqlite3* db, const std::string& year_month);

    // 生成并返回完整的月度报告字符串
    std::string generate_report();

private:
    // 私有辅助函数
    bool _validate_input() const;
    void _fetch_data();
    // 修改为向 stringstream 写入数据
    void _display_summary(std::stringstream& ss);
    void _display_project_breakdown(std::stringstream& ss);

    // 成员变量
    sqlite3* m_db;
    const std::string m_year_month;
    long long m_total_duration = 0;
    int m_actual_days = 0;
    std::vector<std::pair<std::string, long long>> m_records;
};

#endif // QUERY_MONTH_H