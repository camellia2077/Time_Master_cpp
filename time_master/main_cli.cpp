// file: main_cli.cpp (Refactored with corrected CLI11 flag definitions)

#include "common/pch.h"
#include <iostream>
#include <string>
#include <vector>
#include <stdexcept>
#include <print>
#include <sstream>
#include <optional>

// --- Windows-specific include for console functions ---
#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#endif

#include "common/common_utils.h"
#include "common/version.h"
#include "action_handler/ActionHandler.h"
#include "action_handler/file/FilePipelineManager.h"
#include "file_handler/FileController.h" // Needed to get config
#include "CLI/CLI.hpp" // [新增] 引入 CLI11

// --- Function Declarations ---
void print_full_usage(const char* app_name);

const std::string DATABASE_NAME = "time_data.db";

// Helper function to parse ReportFormat from string
ReportFormat parse_format_str(const std::string& format_str) {
    if (format_str == "md" || format_str == "markdown") return ReportFormat::Markdown;
    if (format_str == "tex") return ReportFormat::LaTeX;
    if (format_str == "typ") return ReportFormat::Typ;
    throw CLI::ValidationError("Unsupported format: '" + format_str + "'. Supported formats: md, tex, typ.");
}

int main(int argc, char* argv[]) {
    // --- Console Setup (Windows Only) ---
    #if defined(_WIN32) || defined(_WIN64)
    SetConsoleOutputCP(CP_UTF8);
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
        DWORD dwMode = 0;
        if (GetConsoleMode(hOut, &dwMode)) {
            dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            SetConsoleMode(hOut, dwMode);
        }
    }
    #endif

    // [重构] 为了满足保留自定义帮助信息的需求，我们在CLI11解析前手动处理
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            print_full_usage(argv[0]);
            return 0;
        }
        if (arg == "-v" || arg == "--version") {
            std::println("TimeMaster Command Version: {}", AppInfo::VERSION);
            std::println("Last Updated:  {}", AppInfo::LAST_UPDATED);
            return 0;
        }
    }

    CLI::App app{"TimeMaster: A command-line tool for time data pre-processing, import, and querying."};
    app.set_help_flag(); // 禁用CLI11的默认--help
    app.set_version_flag(); // 禁用CLI11的默认--version

    try {
        FileController file_controller(argv[0]);
        ActionHandler handler(DATABASE_NAME, file_controller.get_config(), file_controller.get_main_config_path());

        // --- 定义所有命令、子命令和选项 ---

        // 1. Full Pipeline
        std::string all_path;
        app.add_option("-a,--all", all_path, "Execute full flow: validate, convert, and import.")->check(CLI::ExistingDirectory);

        // 2. Manual Pre-processing
        bool convert_flag{false}, vs_flag{false}, vo_flag{false}, edc_flag{false};
        std::string manual_path;
        app.add_flag("-c,--convert", convert_flag, "Only convert file format.");
        // [修正] 将 -vs 改为 -s, --validate-source
        app.add_flag("-s,--validate-source", vs_flag, "Only validate the source file format.");
        // [修正] 将 -vo 改为 -o, --validate-output
        app.add_flag("-o,--validate-output", vo_flag, "Validate output file after conversion (use with -c).");
        // [修正] 将 -edc 改为 -d, --enable-day-check
        app.add_flag("-d,--enable-day-check", edc_flag, "Enable check for completeness of days (use with -vo).");
        app.add_option("manual_path", manual_path, "Path for manual pre-processing flags.")->check(CLI::ExistingPath);
        
        // 3. Manual Data Import
        std::string process_path;
        app.add_option("-p,--process", process_path, "Process a directory of formatted files and import to database.")->check(CLI::ExistingDirectory);

        // 4. Data Query Module
        auto query_cmd = app.add_subcommand("query", "Query statistics from the database.")->alias("-q");
        std::string query_format_str = "md";
        query_cmd->add_option("-f,--format", query_format_str, "Output format (md, tex, typ)");
        
        auto q_daily = query_cmd->add_subcommand("daily", "Query a specific day.")->alias("d");
        std::string daily_date;
        q_daily->add_option("date", daily_date, "Date in YYYYMMDD format.")->required();

        auto q_monthly = query_cmd->add_subcommand("monthly", "Query a specific month.")->alias("m");
        std::string monthly_date;
        q_monthly->add_option("month", monthly_date, "Month in YYYY-MM format.")->required();

        auto q_period = query_cmd->add_subcommand("period", "Query last N days.")->alias("p");
        std::string period_days_str;
        q_period->add_option("days", period_days_str, "List of days (e.g., \"7,30,90\").")->required();

        // 5. Data Export Module
        auto export_cmd = app.add_subcommand("export", "Export reports.")->alias("-e");
        std::string export_format_str = "md";
        export_cmd->add_option("-f,--format", export_format_str, "Output format (md, tex, typ)");
        
        auto exp_daily = export_cmd->add_subcommand("daily", "Export a single daily report.");
        std::string exp_daily_date;
        exp_daily->add_option("date", exp_daily_date, "Date in YYYYMMDD format.")->required();

        auto exp_monthly = export_cmd->add_subcommand("monthly", "Export a single monthly report.");
        std::string exp_monthly_date;
        exp_monthly->add_option("month", exp_monthly_date, "Month in YYYY-MM format.")->required();

        auto exp_period = export_cmd->add_subcommand("period", "Export a single period report.");
        int exp_period_days = 0;
        exp_period->add_option("days", exp_period_days, "A single number for days (e.g., 7).")->required();

        auto exp_all_daily = export_cmd->add_subcommand("all-daily", "Export all daily reports.");
        auto exp_all_monthly = export_cmd->add_subcommand("all-monthly", "Export all monthly reports.");
        
        auto exp_all_period = export_cmd->add_subcommand("all-period", "Export all period reports for a list of days.");
        std::string exp_all_period_days_str;
        exp_all_period->add_option("days", exp_all_period_days_str, "List of days (e.g., \"7,30,90\").")->required();

        // --- 解析命令行 ---
        app.parse(argc, argv);

        // --- 执行逻辑 ---
        if (!all_path.empty()) {
            handler.run_full_pipeline_and_import(all_path);
        } else if (!process_path.empty()) {
            handler.run_database_import(process_path);
        } else if (convert_flag || vs_flag || vo_flag || edc_flag) {
            FilePipelineManager pipeline(file_controller.get_config());
            // [修正] 将 vs_flag, vo_flag, edc_flag 关联到对应的逻辑中
            if (!pipeline.collectFiles(manual_path)) throw std::runtime_error("Failed to collect files.");
            if (vs_flag && !pipeline.validateSourceFiles()) throw std::runtime_error("Source validation failed.");
            if (convert_flag && !pipeline.convertFiles()) throw std::runtime_error("Conversion failed.");
            if (vo_flag && !pipeline.validateOutputFiles(edc_flag)) throw std::runtime_error("Output validation failed.");
        } else if (query_cmd->parsed()) {
            ReportFormat format = parse_format_str(query_format_str);
            if (q_daily->parsed()) {
                std::cout << handler.run_daily_query(daily_date, format) << std::endl;
            }
            if (q_monthly->parsed()) {
                std::cout << handler.run_monthly_query(monthly_date, format) << std::endl;
            }
            if (q_period->parsed()) {
                std::string token;
                std::istringstream tokenStream(period_days_str);
                bool first = true;
                while (std::getline(tokenStream, token, ',')) {
                    if (!first) std::cout << "\n" << std::string(40, '-') << "\n";
                    try {
                        std::cout << handler.run_period_query(std::stoi(token), format) << std::endl;
                    } catch (const std::exception&) {
                        std::cerr << RED_COLOR << "Error: " << RESET_COLOR << "Invalid number '" << token << "' in list. Skipping.\n";
                    }
                    first = false;
                }
            }
        } else if (export_cmd->parsed()) {
            ReportFormat format = parse_format_str(export_format_str);
            if (exp_daily->parsed()) handler.run_export_single_day_report(exp_daily_date, format);
            if (exp_monthly->parsed()) handler.run_export_single_month_report(exp_monthly_date, format);
            if (exp_period->parsed()) handler.run_export_single_period_report(exp_period_days, format);
            if (exp_all_daily->parsed()) handler.run_export_all_daily_reports_query(format);
            if (exp_all_monthly->parsed()) handler.run_export_all_monthly_reports_query(format);
            if (exp_all_period->parsed()) {
                std::vector<int> days_list;
                std::string token;
                std::istringstream tokenStream(exp_all_period_days_str);
                while (std::getline(tokenStream, token, ',')) {
                    try {
                        days_list.push_back(std::stoi(token));
                    } catch (const std::exception&) {
                        throw std::runtime_error("Invalid number provided for 'all-period' list: " + token);
                    }
                }
                if (!days_list.empty()) {
                    handler.run_export_all_period_reports_query(days_list, format);
                }
            }
        }

    } catch (const CLI::ParseError &e) {
        return app.exit(e);
    } catch (const std::exception &e) {
        std::cerr << RED_COLOR << "Error: " << RESET_COLOR << e.what() << std::endl;
        return 1;
    }

    return 0;
}

// [更新] 您的自定义帮助函数，与修正后的CLI11标志完全同步
void print_full_usage(const char* app_name) {
    std::cout << "TimeMaster: A command-line tool for time data pre-processing, import, and querying.\n\n";
    std::cout << "Usage: " << app_name << " <command|flags> [arguments...]\n\n";
    
    std::cout << GREEN_COLOR << "--- Full Pipeline ---\n" << RESET_COLOR;
    std::cout << "  --all, -a <path>\t\tExecute full flow: validate source, convert, and import into database.\n";
    std::cout << "  Example: " << app_name << " --all /path/to/source_logs\n\n";
    
    std::cout << GREEN_COLOR << "--- Manual Pre-processing Steps ---\n" << RESET_COLOR;
    std::cout << "  Usage: " << app_name << " <flag(s)> <file_or_folder_path>\n";
    std::cout << "  Action Flags:\n";
    std::cout << "    --convert, -c\t\tOnly convert file format.\n";
    std::cout << "    --validate-source, -s\tOnly validate the source file format.\n";
    std::cout << "  Optional Flags (used with action flags):\n";
    std::cout << "    --validate-output, -o\tValidate output file after conversion (use with -c).\n";
    std::cout << "    --enable-day-check, -d\tEnable check for completeness of days in a month (use with -o).\n";
    std::cout << "  Example: " << app_name << " --convert --validate-source /path/to/logs\n\n";
    
    std::cout << GREEN_COLOR << "--- Manual Data Import ---\n" << RESET_COLOR;
    std::cout << "  --process, -p <path>\t\tProcess a directory of formatted .txt files and import to database.\n";
    std::cout << "  Example: " << app_name << " --process /path/to/processed_logs/\n\n";
    
    std::cout << GREEN_COLOR << "--- Data Query Module ---\n" << RESET_COLOR;
    std::cout << "  Usage: " << app_name << " query <subcommand> [arguments...] [--format <fmt>]\n";
    std::cout << "  Subcommands:\n";
    std::cout << "    daily <YYYYMMDD>\t\tQuery statistics for a specific day.\n";
    std::cout << "    monthly <YYYYMM>\t\tQuery statistics for a specific month.\n";
    std::cout << "    period <days_list>\t\tQuery statistics for last N days (e.g., \"7,30\").\n";
    std::cout << "  Optional:\n";
    std::cout << "    --format, -f <format>\tSpecify output format (md, tex, typ). Default is md.\n";
    std::cout << "  Example: " << app_name << " query daily 20240101 --format tex\n\n";
    
    std::cout << GREEN_COLOR << "--- Data Export Module ---\n" << RESET_COLOR;
    std::cout << "  Usage: " << app_name << " export <subcommand> [arguments...] [--format <fmt>]\n";
    std::cout << "  Subcommands:\n";
    std::cout << "    daily <YYYYMMDD>\t\tExport a single daily report.\n";
    std::cout << "    monthly <YYYYMM>\t\tExport a single monthly report.\n";
    std::cout << "    period <days>\t\tExport a single period report (e.g., 7).\n";
    std::cout << "    all-daily\t\t\tExport all daily reports.\n";
    std::cout << "    all-monthly\t\t\tExport all monthly reports.\n";
    std::cout << "    all-period <days_list>\tExport period reports for a list of days (e.g., \"7,30,90\").\n";
    std::cout << "  Optional:\n";
    std::cout << "    --format, -f <format>\tSpecify output format (md, tex, typ). Default is md.\n";
    std::cout << "  Example: " << app_name << " export all-monthly --format tex\n\n";

    std::cout << GREEN_COLOR << "--- Other Options ---\n" << RESET_COLOR;
    std::cout << "  --help, -h\t\t\tShow this help message.\n";
    std::cout << "  --version, -v\t\t\tShow program version.\n";
}