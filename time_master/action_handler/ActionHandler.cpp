// file: ActionHandler.cpp

#include "common/pch.h"
#include "ActionHandler.h"

#include "action_handler/database/DatabaseManager.h"
#include "action_handler/reporting/ReportExporter.h"
#include "action_handler/query/DirectQueryManager.h"
#include "db_inserter/DataImporter.h"
#include "common/common_utils.h"
#include "file_handler/FileUtils.h"
// [新增] 引入 FilePipelineManager 以便使用
#include "action_handler/file/FilePipelineManager.h"


#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <optional> // 为了使用 std::optional

namespace fs = std::filesystem;

// 构造函数: 移除了 processor_ 的初始化
ActionHandler::ActionHandler(const std::string& db_name, const AppConfig& config, const std::string& main_config_path)
    : app_config_(config),
      main_config_path_(main_config_path),
      db_name_(db_name),
      export_root_path_(config.export_path.value_or("exported_files"))
{
    db_manager_ = std::make_unique<DatabaseManager>(db_name);
}

ActionHandler::~ActionHandler() = default;
// --- Add this implementation ---
const AppConfig& ActionHandler::get_config() const {
    return app_config_;
}

// =========================================================================
//                      私有辅助函数 (保持不变)
// =========================================================================
DirectQueryManager* ActionHandler::get_direct_query_manager() {
    if (!db_manager_->open_database_if_needed()) {
        std::cerr << RED_COLOR << "错误: 无法打开数据库，查询操作中止。" << RESET_COLOR << std::endl;
        return nullptr;
    }
    if (!direct_query_manager_) {
        direct_query_manager_ = std::make_unique<DirectQueryManager>(db_manager_->get_db_connection());
    }
    return direct_query_manager_.get();
}

ReportExporter* ActionHandler::get_report_exporter() {
    if (!db_manager_->open_database_if_needed()) {
        std::cerr << RED_COLOR << "错误: 无法打开数据库，导出操作已中止。" << RESET_COLOR << std::endl;
        return nullptr;
    }
    if (!report_exporter_) {
        report_exporter_ = std::make_unique<ReportExporter>(db_manager_->get_db_connection(), export_root_path_);
    }
    return report_exporter_.get();
}


// =========================================================================
//                      直接查询与报告导出 (保持不变)
// =========================================================================
// ... run_daily_query, run_monthly_query, ... 等所有查询和导出函数都保持不变 ...
std::string ActionHandler::run_daily_query(const std::string& date, ReportFormat format) {
    if (auto* qm = get_direct_query_manager()) { // [重构]
        return qm->run_daily_query(date, format);
    }
    return std::string(RED_COLOR) + "错误: 查询失败。" + RESET_COLOR;
}

std::string ActionHandler::run_monthly_query(const std::string& month, ReportFormat format) {
    if (auto* qm = get_direct_query_manager()) { // [重构]
        return qm->run_monthly_query(month, format);
    }
    return std::string(RED_COLOR) + "错误: 查询失败。" + RESET_COLOR;
}

std::string ActionHandler::run_period_query(int days, ReportFormat format) {
    if (auto* qm = get_direct_query_manager()) { // [重构]
        return qm->run_period_query(days, format);
    }
    return std::string(RED_COLOR) + "错误: 查询失败。" + RESET_COLOR;
}

void ActionHandler::run_export_single_day_report(const std::string& date, ReportFormat format) {
    if (auto* exporter = get_report_exporter()) { // [重构]
        exporter->run_export_single_day_report(date, format);
    }
}

void ActionHandler::run_export_single_month_report(const std::string& month, ReportFormat format) {
    if (auto* exporter = get_report_exporter()) { // [重构]
        exporter->run_export_single_month_report(month, format);
    }
}

void ActionHandler::run_export_single_period_report(int days, ReportFormat format) {
    if (auto* exporter = get_report_exporter()) { // [重构]
        exporter->run_export_single_period_report(days, format);
    }
}

void ActionHandler::run_export_all_daily_reports_query(ReportFormat format) {
    if (auto* exporter = get_report_exporter()) { // [重构]
        exporter->run_export_all_daily_reports_query(format);
    }
}

void ActionHandler::run_export_all_monthly_reports_query(ReportFormat format) {
    if (auto* exporter = get_report_exporter()) { // [重构]
        exporter->run_export_all_monthly_reports_query(format);
    }
}

void ActionHandler::run_export_all_period_reports_query(const std::vector<int>& days_list, ReportFormat format) {
    if (auto* exporter = get_report_exporter()) { // [重构]
        exporter->run_export_all_period_reports_query(days_list, format);
    }
}


// =========================================================================
//                      文件处理与导入 (已重构)
// =========================================================================

void ActionHandler::run_database_import(const std::string& processed_path_str) {
    fs::path processed_path(processed_path_str);
    if (!fs::exists(processed_path) || !fs::is_directory(processed_path)) {
        std::cerr << RED_COLOR << "错误: " << RESET_COLOR << "路径不存在或不是目录。导入中止。" << std::endl;
        return;
    }
    db_manager_->close_database();
    std::cout << "开始导入过程..." << std::endl;
    handle_process_files(db_name_, processed_path.string(), main_config_path_);
    std::cout << "导入过程结束。" << std::endl;
}

void ActionHandler::run_full_pipeline_and_import(const std::string& source_path) {
    std::cout << "\n--- 开始完整流水线处理 ---" << std::endl; //
    db_manager_->close_database(); //

    // 1. 创建 FilePipelineManager 实例并委托其运行流水线
    FilePipelineManager pipeline(app_config_);
    std::optional<fs::path> output_path = pipeline.run(source_path);

    // 2. 检查流水线是否成功，并执行导入
    if (output_path) {
        run_database_import(output_path->string());
        std::cout << GREEN_COLOR << "\n成功: 完整流水线处理完毕并已导入数据。" << RESET_COLOR << std::endl; //
    } else {
        std::cerr << RED_COLOR << "错误: 文件处理流水线失败，操作终止。" << RESET_COLOR << std::endl; //
    }
}