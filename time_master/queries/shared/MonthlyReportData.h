// queries/report_generators/monthly/_month_data/MonthlyReportData.h
#ifndef REPORT_DATA_H
#define REPORT_DATA_H

#include <string>
#include <vector>

// 用于月报的数据
struct MonthlyReportData {
    std::string year_month;
    long long total_duration = 0;
    int actual_days = 0;
    std::vector<std::pair<std::string, long long>> records;
};

#endif // REPORT_DATA_H