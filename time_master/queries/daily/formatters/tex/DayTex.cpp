#include "common/pch.h"
// DayTex.cpp 
#include "DayTex.h"
#include <iomanip>
#include <string>

#include "common/common_utils.h"

#include "queries/shared/query_utils.h"
#include "queries/shared/breakdown/TreeFmtFactory.h"
#include "queries/shared/breakdown/ITreeFmt.h"
#include "queries/shared/DailyReportData.h"


// The 'const' keyword is added here to match the header file
std::string DayTex::format_report(const DailyReportData& data, sqlite3* db) const {
    return format_report_template(data, db);
}

// 2. 实现新的 format_content 方法，将所有核心内容生成逻辑移到这里
void DayTex::format_content(std::stringstream& ss, const DailyReportData& data, sqlite3* db) const {
    // 步骤 A: 生成报告头
    _display_header(ss, data);

    // 步骤 B: 生成报告主体
    if (data.total_duration == 0) {
        ss << "No time records for this day.\n";
    } else {
        _display_project_breakdown(ss, data, db);
    }
}

// 3. _display_preamble 方法被彻底删除

// _display_header 方法保持不变
void DayTex::_display_header(std::stringstream& ss, const DailyReportData& data) const {
    auto escape_tex_local = [](const std::string& s) {
        std::string escaped;
        for (char c : s) {
            if (c == '&' || c == '%' || c == '$' || c == '#' || c == '_' || c == '{' || c == '}') {
                escaped += '\\';
            }
            escaped += c;
        }
        return escaped;
    };

    ss << "\\section*{Daily Report for " << escape_tex_local(data.date) << "}\n\n";
    ss << "\\begin{itemize}\n";
    ss << "    \\item \\textbf{Date}: " << escape_tex_local(data.date) << "\n";
    ss << "    \\item \\textbf{Total Time Recorded}: " << escape_tex_local(time_format_duration(data.total_duration)) << "\n";
    ss << "    \\item \\textbf{Status}: " << escape_tex_local(data.metadata.status) << "\n";
    ss << "    \\item \\textbf{Getup Time}: " << escape_tex_local(data.metadata.getup_time) << "\n";
    ss << "    \\item \\textbf{Remark}: " << escape_tex_local(data.metadata.remark) << "\n";
    ss << "\\end{itemize}\n\n";
}

// _display_project_breakdown 方法保持不变
void DayTex::_display_project_breakdown(std::stringstream& ss, const DailyReportData& data, sqlite3* db) const {
    std::map<std::string, std::string> parent_map = get_parent_map(db);
    ProjectTree project_tree;
    build_project_tree_from_records(project_tree, data.records, parent_map);

    auto formatter = TreeFmtFactory::createFormatter(ReportFormat::LaTeX);
    if (formatter) {
        std::string breakdown_output = formatter->format(project_tree, data.total_duration, 1);
        ss << breakdown_output;
    }
}