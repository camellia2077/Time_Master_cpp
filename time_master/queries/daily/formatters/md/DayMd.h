// queries/daily/formatters/DayMd.h
#ifndef DAILY_REPORT_MARKDOWN_FORMATTER_H
#define DAILY_REPORT_MARKDOWN_FORMATTER_H

#include "queries/shared/Interface/IReportFormatter.h" // 引入新的模板接口
#include "queries/shared/DailyReportData.h"   // 引入数据类型
#include <sstream>

// Forward declaration
struct DailyReportData;

/**
 * @class DayMd
 * @brief 将日报数据格式化为 Markdown 字符串的具体实现。
 */
// 继承自模板化的通用接口
class DayMd : public IReportFormatter<DailyReportData> {
public:
    DayMd() = default;

    // 函数签名与模板接口完全匹配，无需更改
    std::string format_report(const DailyReportData& data, sqlite3* db) const override;

private:
    void _display_header(std::stringstream& ss, const DailyReportData& data) const;
    void _display_project_breakdown(std::stringstream& ss, const DailyReportData& data, sqlite3* db) const;
};

#endif // DAILY_REPORT_MARKDOWN_FORMATTER_H