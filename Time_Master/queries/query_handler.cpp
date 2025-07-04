#include "query_handler.h"

// 引入所有具体的报告生成器类
#include "query_day.h"
#include "query_period.h"
#include "query_month.h"

QueryHandler::QueryHandler(sqlite3* db) : m_db(db) {}

std::string QueryHandler::run_daily_query(const std::string& date_str) const {
    DailyReportGenerator generator(m_db, date_str);
    return generator.generate_report();
}

std::string QueryHandler::run_period_query(int days) const {
    PeriodReportGenerator generator(m_db, days);
    return generator.generate_report();
}

std::string QueryHandler::run_monthly_query(const std::string& year_month_str) const {
    MonthlyReportGenerator generator(m_db, year_month_str);
    return generator.generate_report();
}