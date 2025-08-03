#include "common/pch.h"
#include <iostream>
#include <string>
#include <vector>
#include <stdexcept>
#include <print>

// --- Windows-specific include for console functions ---
#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#endif

#include "common/common_utils.h"
#include "common/version.h"
#include "time_master_cli/CliController.h"

// --- Function Declarations ---
void print_full_usage(const char* app_name);

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

    std::vector<std::string> args(argv, argv + argc);
    if (args.size() < 2) {
        print_full_usage(args[0].c_str());
        return 1;
    }

    const std::string& command = args[1];

    // --- Handle simple, top-level commands directly ---
    if (command == "-h" || command == "--help") {
        print_full_usage(args[0].c_str());
        return 0;
    }
    if (command == "-v" || command == "--version") {
        std::println("TimeMaster Command Version: {}", AppInfo::VERSION);
        std::println("Last Updated:  {}", AppInfo::LAST_UPDATED);
        return 0;
    }

    // --- Delegate complex commands to the controller ---
    try {
        CliController controller(args);
        controller.execute();
    } catch (const std::exception& e) {
        std::cerr << RED_COLOR << "Error: " << RESET_COLOR << e.what() << std::endl;
        // Optionally show usage for command-related errors
        if (std::string(e.what()).find("command") != std::string::npos || 
            std::string(e.what()).find("argument") != std::string::npos) {
             std::cout << "\nUse '" << args[0] << " --help' for more information.\n";
        }
        return 1;
    }

    return 0;
}

void print_full_usage(const char* app_name) {
    std::cout << "TimeMaster: A command-line tool for time data pre-processing, import, and querying.\n\n";
    std::cout << "Usage: " << app_name << " <command> [arguments...] [options...]\n\n";

    std::cout << GREEN_COLOR << "--- Core Commands ---\n" << RESET_COLOR;
    std::cout << "  run-all <path>\t\t Execute full pipeline: validate source, convert, and import into database.\n";
    std::cout << "  preprocess <path>\t\t Manually run pre-processing steps on source files.\n";
    std::cout << "  import <path>\t\t\t Import pre-processed .txt files into the database.\n";
    std::cout << "  query <type> <period>\t\t Query data from the database.\n";
    std::cout << "  export <type> <period>\t Export reports from the database.\n\n";

    std::cout << GREEN_COLOR << "--- Command: preprocess ---\n" << RESET_COLOR;
    std::cout << "  Usage: " << app_name << " preprocess <file_or_folder_path> [options...]\n";
    std::cout << "  Options:\n";
    std::cout << "    --validate-source, -vs\t Validates the source file format.\n";
    std::cout << "    --convert, -c\t\t Converts the source file to the processed format.\n";
    std::cout << "    --validate-output, -vo\t Validates the output file after conversion (requires -c).\n";
    std::cout << "    --enable-day-check, -edc\t Enable check for day completeness in a month (requires -vo).\n";
    std::cout << "  Example: " << app_name << " preprocess /path/to/logs --convert --validate-output\n\n";

    std::cout << GREEN_COLOR << "--- Command: import ---\n" << RESET_COLOR;
    std::cout << "  Usage: " << app_name << " import <directory_path>\n";
    std::cout << "  Example: " << app_name << " import /path/to/processed_logs/\n\n";
    
    std::cout << GREEN_COLOR << "--- Command: query ---\n" << RESET_COLOR;
    std::cout << "  Usage: " << app_name << " query <type> <argument> [options...]\n";
    std::cout << "  Types:\n";
    std::cout << "    daily <YYYYMMDD>\t\t Query statistics for a specific day.\n";
    std::cout << "    monthly <YYYYMM>\t\t Query statistics for a specific month.\n";
    std::cout << "    period <days>\t\t Query statistics for last N days. Can be a list (e.g., 7,30).\n";
    std::cout << "  Options (for all query types):\n";
    std::cout << "    --format, -f <format>\t Specify output format (md, tex, typ). Default: md.\n";
    std::cout << "  Example: " << app_name << " query daily 20240101 --format tex\n\n";
    
    std::cout << GREEN_COLOR << "--- Command: export ---\n" << RESET_COLOR;
    std::cout << "  Usage: " << app_name << " export <type> [argument] [options...]\n";
    std::cout << "  Types:\n";
    std::cout << "    daily <YYYYMMDD>\t\t Export a single daily report.\n";
    std::cout << "    monthly <YYYYMM>\t\t Export a single monthly report.\n";
    std::cout << "    period <days>\t\t Export a single period report (e.g., 7).\n";
    std::cout << "    all-daily\t\t\t Export all daily reports.\n";
    std::cout << "    all-monthly\t\t\t Export all monthly reports.\n";
    std::cout << "    all-period <days_list>\t Export multiple period reports (e.g., 7,30,90).\n";
    std::cout << "  Options (for all export types):\n";
    std::cout << "    --format, -f <format>\t Specify output format (md, tex, typ). Default: md.\n";
    std::cout << "  Example: " << app_name << " export daily 20240115 --format tex\n";
    std::cout << "  Example: " << app_name << " export all-monthly --format tex\n\n";

    std::cout << GREEN_COLOR << "--- Other Options ---\n" << RESET_COLOR;
    std::cout << "  --help, -h\t\t\t Show this help message.\n";
    std::cout << "  --version, -v\t\t\t Show program version.\n";
}