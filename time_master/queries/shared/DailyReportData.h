// queries/report_generators/daily/data/DailyReportData.h
#ifndef DAILY_REPORT_DATA_H
#define DAILY_REPORT_DATA_H

#include <string>
#include <vector>
#include <map>

// This struct is part of the data for a daily report, so it belongs here.
struct DayMetadata {
    std::string status = "N/A";
    std::string remark = "N/A";
    std::string getup_time = "N/A";
};

/**
 * @brief 日查询的结构体
 */
struct DailyReportData {
    std::string date;
    DayMetadata metadata; // The compiler now sees the definition directly above
    long long total_duration = 0;
    std::vector<std::pair<std::string, long long>> records;
};

#endif // DAILY_REPORT_DATA_H