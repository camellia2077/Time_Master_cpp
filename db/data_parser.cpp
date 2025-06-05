// This file utilizes functions defined in common_utils.h, specifically time_str_to_seconds.
// Do not redefine functions that are already present in common_utils.h within this file.

#include "data_parser.h"
#include "common_utils.h"
#include <fstream>
#include <iostream>
#include <regex>
#include <sqlite3.h>
#include <algorithm>
#include <stdexcept>

// --- FileDataParser Constructor & Destructor ---

FileDataParser::FileDataParser(const std::string& db_path) 
    : db(nullptr), current_date_processed(false),
      stmt_update_day(nullptr), stmt_insert_record(nullptr), stmt_insert_parent_child(nullptr) {
    if (sqlite3_open(db_path.c_str(), &db)) {
        std::cerr << "Error: Cannot open database: " << sqlite3_errmsg(db) << std::endl;
        db = nullptr;
    } else {
        _initialize_database();
        _prepare_statements(); // Prepare statements for reuse
    }
    initial_top_level_parents = {
        {"study", "STUDY"},
        {"code", "CODE"}
    };
}

FileDataParser::~FileDataParser() {
    if (db) {
        // Ensure any remaining data is processed before closing.
        // The transaction should already be handled by parse_file's scope.
        _store_previous_date_data();
        _commit_parent_child_buffer(); // Make sure the buffer is flushed

        _finalize_statements(); // Clean up prepared statements
        sqlite3_close(db);
    }
}

// --- Public Member Functions ---

bool FileDataParser::is_db_open() const {
    return db != nullptr;
}

void FileDataParser::commit_all() {
    _store_previous_date_data();
}

bool FileDataParser::parse_file(const std::string& filename) {
    if (!db) return false;

    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot open file " << filename << std::endl;
        return false;
    }

    current_file_name = filename;

    // --- Optimization 1: Single Transaction ---
    execute_sql_parser(db, "BEGIN TRANSACTION;", "Begin file transaction");
    bool transaction_success = true;

    try {
        std::string line;
        int line_num = 0;
        while (std::getline(file, line)) {
            line_num++;
            line.erase(0, line.find_first_not_of(" \t\n\r\f\v"));
            line.erase(line.find_last_not_of(" \t\n\r\f\v") + 1);
            if (line.empty()) continue;

            if (line.rfind("Date:", 0) == 0) {
                _store_previous_date_data(); 
                _handle_date_line(line);
            } else if (line.rfind("Status:", 0) == 0) {
                _handle_status_line(line);
            } else if (line.rfind("Remark:", 0) == 0) {
                _handle_remark_line(line);
            } else if (line.rfind("Getup:", 0) == 0) {
                _handle_getup_line(line);
            } else if (line.find('~') != std::string::npos) {
                _handle_time_record_line(line, line_num);
            }
        }
        
        _store_previous_date_data();      // Store the very last day's data from the file
        _commit_parent_child_buffer();    // Commit all unique parent-child pairs at once

    } catch (const std::exception& e) {
        std::cerr << current_file_name << ": An error occurred during parsing: " << e.what() << std::endl;
        transaction_success = false;
    }

    file.close();

    // Commit or rollback the single transaction for the entire file
    execute_sql_parser(db, transaction_success ? "COMMIT;" : "ROLLBACK;", transaction_success ? "Commit file transaction" : "Rollback file transaction");
    
    return transaction_success;
}


// --- Private Member Functions ---

void FileDataParser::_initialize_database() {
    execute_sql_parser(db, "CREATE TABLE IF NOT EXISTS days (date TEXT PRIMARY KEY, status TEXT, remark TEXT, getup_time TEXT);", "Create days table");
    execute_sql_parser(db, "CREATE TABLE IF NOT EXISTS time_records (date TEXT, start TEXT, end TEXT, project_path TEXT, duration INTEGER, PRIMARY KEY (date, start), FOREIGN KEY (date) REFERENCES days(date));", "Create time_records table");
    execute_sql_parser(db, "CREATE TABLE IF NOT EXISTS parent_child (child TEXT PRIMARY KEY, parent TEXT);", "Create parent_child table");
}

// --- Optimization 2: Prepare SQL Statements ---
void FileDataParser::_prepare_statements() {
    const char* update_day_sql = "UPDATE days SET status = ?, remark = ?, getup_time = ? WHERE date = ?;";
    if (sqlite3_prepare_v2(db, update_day_sql, -1, &stmt_update_day, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare day update statement.");
    }

    const char* insert_record_sql = "INSERT OR REPLACE INTO time_records (date, start, end, project_path, duration) VALUES (?, ?, ?, ?, ?);";
    if (sqlite3_prepare_v2(db, insert_record_sql, -1, &stmt_insert_record, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare time record insert statement.");
    }

    const char* insert_parent_child_sql = "INSERT OR IGNORE INTO parent_child (child, parent) VALUES (?, ?);";
    if (sqlite3_prepare_v2(db, insert_parent_child_sql, -1, &stmt_insert_parent_child, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare parent-child insert statement.");
    }
}

void FileDataParser::_finalize_statements() {
    if (stmt_update_day) sqlite3_finalize(stmt_update_day);
    if (stmt_insert_record) sqlite3_finalize(stmt_insert_record);
    if (stmt_insert_parent_child) sqlite3_finalize(stmt_insert_parent_child);
}

void FileDataParser::_handle_date_line(const std::string& line) {
    if (line.length() > 5) {
        current_date = line.substr(5);
        current_date.erase(0, current_date.find_first_not_of(" \t"));
        current_date.erase(current_date.find_last_not_of(" \t") + 1);

        current_status = "False";
        current_remark = "";
        current_getup_time = "00:00";
        current_time_records_buffer.clear();
        current_date_processed = false;

        sqlite3_stmt* stmt;
        const char* sql = "INSERT OR IGNORE INTO days (date, status, remark, getup_time) VALUES (?, ?, ?, ?);";
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, current_date.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 2, current_status.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 3, current_remark.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 4, current_getup_time.c_str(), -1, SQLITE_STATIC);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    }
}

// --- Optimization 3: Simplified String Handling ---
void FileDataParser::_handle_status_line(const std::string& line) {
    if (line.length() > 7) current_status = line.substr(7);
}

void FileDataParser::_handle_remark_line(const std::string& line) {
    if (line.length() > 7) current_remark = line.substr(7);
}

void FileDataParser::_handle_getup_line(const std::string& line) {
    if (line.length() > 6) current_getup_time = line.substr(6);
}

void FileDataParser::_handle_time_record_line(const std::string& line, int line_num) {
    std::regex time_record_regex(R"((\d{2}:\d{2})~(\d{2}:\d{2})\s*(.+))");
    std::smatch matches;
    if (std::regex_match(line, matches, time_record_regex) && matches.size() == 4) {
        std::string start_time_str = matches[1].str();
        std::string end_time_str = matches[2].str();
        // Optimization 3: Directly use the matched group without further trimming
        std::string project_path = matches[3].str();

        int start_seconds = time_str_to_seconds(start_time_str);
        int end_seconds = time_str_to_seconds(end_time_str);
        int duration_seconds = (end_seconds < start_seconds) ? ((end_seconds + 24 * 3600) - start_seconds) : (end_seconds - start_seconds);

        current_time_records_buffer.push_back({current_date, start_time_str, end_time_str, project_path, duration_seconds});
        _process_project_path(project_path);
    }
}

// --- Optimization 4: Buffer parent_child Inserts ---
void FileDataParser::_process_project_path(const std::string& project_path_orig) {
    std::string project_path = project_path_orig;
    std::replace(project_path.begin(), project_path.end(), ' ', '_');
    std::stringstream ss(project_path);
    std::string segment;
    std::vector<std::string> segments;
    while(std::getline(ss, segment, '_')) {
        if (!segment.empty()) segments.push_back(segment);
    }

    if (segments.empty()) return;
    
    // Add initial top-level parents to the buffer
    for (const auto& pair : initial_top_level_parents) {
        parent_child_buffer.insert({pair.first, pair.second});
    }

    std::string current_full_path = "";
    std::string parent_of_current_segment;
    for (size_t i = 0; i < segments.size(); ++i) {
        if (i == 0) {
            current_full_path = segments[i];
            auto it = initial_top_level_parents.find(current_full_path);
            if (it != initial_top_level_parents.end()) {
                parent_of_current_segment = it->second;
            } else {
                parent_of_current_segment = current_full_path;
                std::transform(parent_of_current_segment.begin(), parent_of_current_segment.end(), parent_of_current_segment.begin(), ::toupper);
            }
        } else {
            parent_of_current_segment = current_full_path;
            current_full_path += "_" + segments[i];
        }
        // Buffer the insert instead of executing it immediately
        parent_child_buffer.insert({current_full_path, parent_of_current_segment});
    }
}

void FileDataParser::_commit_parent_child_buffer() {
    if (!stmt_insert_parent_child) return;

    for (const auto& pair : parent_child_buffer) {
        sqlite3_bind_text(stmt_insert_parent_child, 1, pair.first.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt_insert_parent_child, 2, pair.second.c_str(), -1, SQLITE_STATIC);
        if (sqlite3_step(stmt_insert_parent_child) != SQLITE_DONE) {
             std::cerr << "Error inserting parent-child pair: " << sqlite3_errmsg(db) << std::endl;
        }
        sqlite3_reset(stmt_insert_parent_child);
    }
    parent_child_buffer.clear(); // Clear buffer after commit
}


void FileDataParser::_store_previous_date_data() {
    if (current_date.empty() || !db || current_date_processed) return;
    
    // This function now uses pre-compiled statements and does not manage transactions

    // Update day record
    if(stmt_update_day) {
        sqlite3_bind_text(stmt_update_day, 1, current_status.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt_update_day, 2, current_remark.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt_update_day, 3, current_getup_time.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt_update_day, 4, current_date.c_str(), -1, SQLITE_STATIC);
        if (sqlite3_step(stmt_update_day) != SQLITE_DONE) {
            std::cerr << "Error updating day record: " << sqlite3_errmsg(db) << std::endl;
        }
        sqlite3_reset(stmt_update_day);
    }
    
    // Insert all time records from buffer
    if(stmt_insert_record) {
        for (const auto& record : current_time_records_buffer) {
            sqlite3_bind_text(stmt_insert_record, 1, record.date.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt_insert_record, 2, record.start.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt_insert_record, 3, record.end.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt_insert_record, 4, record.project_path.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_int(stmt_insert_record, 5, record.duration_seconds);
            if (sqlite3_step(stmt_insert_record) != SQLITE_DONE) {
                 std::cerr << "Error inserting time record: " << sqlite3_errmsg(db) << std::endl;
            }
            sqlite3_reset(stmt_insert_record);
        }
    }

    current_time_records_buffer.clear();
    current_date_processed = true;
}

// Non-member helper function
bool execute_sql_parser(sqlite3* db, const std::string& sql, const std::string& context_msg) {
    char* err_msg = nullptr;
    if (sqlite3_exec(db, sql.c_str(), 0, 0, &err_msg) != SQLITE_OK) {
        std::cerr << "SQL Error (" << context_msg << "): " << err_msg << std::endl;
        sqlite3_free(err_msg);
        return false;
    }
    return true;
}