#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#endif
#include <iostream>
#include <string>
#include <vector>
#include <limits>
#include <sstream>
#include <algorithm>
#include <sqlite3.h>
#include <filesystem>
#include <chrono>

#include "common_utils.h"
#include "data_parser.h"
#include "database_importer.h" 
#include "database_querier.h" 

// Declare ANSI escape codes for text colors
const std::string ANSI_COLOR_GREEN = "\x1b[32m";
const std::string ANSI_COLOR_RESET = "\x1b[0m";
// Define a namespace alias for convenience
namespace fs = std::filesystem;

const std::string DATABASE_NAME = "time_data.db";

void print_menu() {
    std::cout << "\n--- Time Tracking Menu ---" << std::endl;
    std::cout << "0. Process file(s) and import data" << std::endl;
    std::cout << "1. Query daily statistics" << std::endl;
    std::cout << "2. Query last 7 days" << std::endl;
    std::cout << "3. Query last 14 days" << std::endl;
    std::cout << "4. Query last 30 days" << std::endl;
    std::cout << "5. Output raw data for a day" << std::endl;
    std::cout << "6. Generate study heatmap for a year" << std::endl;
    std::cout << "7. Query monthly statistics" << std::endl;
    std::cout << "8. Exit" << std::endl;
    std::cout << "Enter your choice: ";
}

// Helper to get validated YYYYMMDD date string
std::string get_valid_date_input() {
    std::string date_str;
    while (true) {
        std::cout << "Enter date (YYYYMMDD): ";
        std::cin >> date_str;
        if (date_str.length() == 8 && std::all_of(date_str.begin(), date_str.end(), ::isdigit)) {
            int year = std::stoi(date_str.substr(0, 4));
            int month = std::stoi(date_str.substr(4, 2));
            int day = std::stoi(date_str.substr(6, 2));
            if (year > 1900 && year < 3000 && month >= 1 && month <= 12 && day >= 1 && day <= 31) {
                 break;
            }
        }
        std::cout << "Invalid date format or value. Please use YYYYMMDD." << std::endl;
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }
    return date_str;
}

// Helper to get validated YYYYMM month string
std::string get_valid_month_input() {
    std::string month_str;
    while (true) {
        std::cout << "Enter month (YYYYMM): ";
        std::cin >> month_str;
        if (month_str.length() == 6 && std::all_of(month_str.begin(), month_str.end(), ::isdigit)) {
             int year = std::stoi(month_str.substr(0, 4));
             int month = std::stoi(month_str.substr(4, 2));
             if (year > 1900 && year < 3000 && month >= 1 && month <= 12) {
                break;
             }
        }
        std::cout << "Invalid month format or value. Please use YYYYMM." << std::endl;
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }
    return month_str;
}

void run_application_loop() {
    sqlite3* db = nullptr;
    int choice = -1;

    while (choice != 8) {
        print_menu();
        std::cin >> choice;

        if (std::cin.fail()) {
            std::cout << "Invalid input. Please enter a number." << std::endl;
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            choice = -1;
            continue;
        }
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

        if (choice != 0 && choice != 8 && db == nullptr) {
            if (sqlite3_open(DATABASE_NAME.c_str(), &db)) {
                std::cerr << "Can't open database " << DATABASE_NAME << ": " << sqlite3_errmsg(db) << std::endl;
                sqlite3_close(db);
                db = nullptr;
            }
        }

        switch (choice) {
            case 0: {
                if (db) {
                    sqlite3_close(db);
                    db = nullptr;
                }
                std::cout << "Enter file name(s) or directory path(s) to process (space-separated, then Enter): ";
                std::string line;
                std::getline(std::cin, line);
                std::cout << "\nStart processing files... " << std::endl;
                auto start = std::chrono::high_resolution_clock::now();
                std::stringstream ss(line);
                std::string token;
                std::vector<std::string> user_inputs;
                while (ss >> token) {
                    user_inputs.push_back(token);
                }

                if (user_inputs.empty()) {
                    std::cout << "No filenames or directories entered." << std::endl;
                    break;
                }
                
                // --- STAGE 1: Find all .txt files to process ---
                std::vector<std::string> actual_files_to_process;
                for (const std::string& input_path_str : user_inputs) {
                    fs::path p(input_path_str);
                    if (!fs::exists(p)) {
                        std::cerr << "Warning: Path does not exist: " << input_path_str << std::endl;
                        continue;
                    }
                    if (fs::is_regular_file(p)) {
                        if (p.extension() == ".txt") {
                            actual_files_to_process.push_back(p.string());
                        }
                    } else if (fs::is_directory(p)) {
                        try {
                            for (const auto& entry : fs::recursive_directory_iterator(p)) {
                                if (fs::is_regular_file(entry.path()) && entry.path().extension() == ".txt") {
                                    actual_files_to_process.push_back(entry.path().string());
                                }
                            }
                        } catch (const fs::filesystem_error& e) {
                            std::cerr << "Filesystem error accessing directory " << input_path_str << ": " << e.what() << std::endl;
                        }
                    }
                }

                if (actual_files_to_process.empty()) {
                    std::cout << "No .txt files found to process." << std::endl;
                    break;
                }
                
                std::sort(actual_files_to_process.begin(), actual_files_to_process.end());

                // --- STAGE 2: Parsing files into memory ---
                std::cout << "Stage 1: Parsing files into memory..." << std::endl;
                DataFileParser parser;
                int successful_files_count = 0;
                std::vector<std::string> failed_files;

                for (const std::string& fname : actual_files_to_process) {
                    if (parser.parse_file(fname)) {
                        successful_files_count++;
                    } else {
                        failed_files.push_back(fname);
                    }
                }
                parser.commit_all(); // Finalize any buffered data from the last file

                // --- STAGE 3: Importing from memory to database ---
                std::cout << "Stage 2: Importing data into the database..." << std::endl;
                DatabaseImporter importer(DATABASE_NAME);
                if (!importer.is_db_open()) {
                    std::cerr << "Importer could not open database. Aborting." << std::endl;
                } else {
                    importer.import_data(parser);
                }

                // --- Reporting ---
                std::cout << "\n--- Data processing complete. ---" << std::endl;
                if (failed_files.empty()) {
                    std::cout << ANSI_COLOR_GREEN << "All files successfully processed and imported." << ANSI_COLOR_RESET << std::endl;
                    std::cout << "Successfully processed " << successful_files_count << " files." << std::endl;
                } else {
                    std::cerr << "There were errors during the parsing stage." << std::endl;
                    if (successful_files_count > 0) {
                        std::cout << "Successfully parsed " << successful_files_count << " files." << std::endl;
                    }
                    std::cerr << "Failed to parse the following " << failed_files.size() << " files:" << std::endl;
                    for (const std::string& fname : failed_files) {
                        std::cerr << "- " << fname << std::endl;
                    }
                }
                
                auto end = std::chrono::high_resolution_clock::now();
                double elapsed = std::chrono::duration<double>(end - start).count();
                std::cout << "Total processing time: " << elapsed << " seconds." << std::endl;
                break;
            }
            case 1: { 
                if (!db) { std::cerr << "Database not open." << std::endl; break; }
                std::string date_str = get_valid_date_input();
                query_day(db, date_str);
                break;
            }
            case 2: {
                 if (!db) { std::cerr << "Database not open." << std::endl; break; }
                query_period(db, 7);
                break;
            }
            case 3: {
                 if (!db) { std::cerr << "Database not open." << std::endl; break; }
                query_period(db, 14);
                break;
            }
            case 4: { 
                 if (!db) { std::cerr << "Database not open." << std::endl; break; }
                query_period(db, 30);
                break;
            }
            case 5: { 
                 if (!db) { std::cerr << "Database not open." << std::endl; break; }
                std::string date_str = get_valid_date_input();
                query_day_raw(db, date_str);
                break;
            }
            case 6: { 
                std::cout << "\nFeature 'Generate study heatmap for a year' is not yet implemented." << std::endl;
                break;
            }
            case 7: {
                 if (!db) { std::cerr << "Database not open." << std::endl; break; }
                std::string month_str = get_valid_month_input();
                query_month_summary(db, month_str);
                break;
            }
             case 8:
                std::cout << "Exiting program." << std::endl;
                break;
            default:
                std::cout << "Invalid choice. Please try again." << std::endl;
                break;
        }
    }

    if (db) {
        sqlite3_close(db);
    }
}

int main() {
    #if defined(_WIN32) || defined(_WIN64)
    SetConsoleOutputCP(CP_UTF8);
    #endif
    run_application_loop();
    return 0;
}