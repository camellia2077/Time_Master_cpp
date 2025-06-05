// This file utilizes functions defined in common_utils.h, specifically time_str_to_seconds.
// Do not redefine functions that are already present in common_utils.h within this file.

#ifndef DATA_PARSER_H
#define DATA_PARSER_H

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <regex>
#include <sstream>
#include <algorithm>
#include <map>
#include <set>      // Added for parent_child buffering
#include <utility>  // Added for std::pair

#include "common_utils.h" // Contains global function definition for time_str_to_seconds

struct TimeRecordInternal {
    std::string date;
    std::string start;
    std::string end;
    std::string project_path;
    int duration_seconds;
};

class DataFileParser {
public:
    DataFileParser(const std::string& output_dir);
    ~DataFileParser();
    bool parse_file(const std::string& filename);
    void commit_all();

private:
    std::string output_dir;
    std::ofstream day_file;
    std::ofstream record_file;
    std::ofstream parent_child_file;

    std::string current_date;
    std::string current_status;
    std::string current_remark;
    std::string current_getup_time;
    std::vector<TimeRecordInternal> current_time_records_buffer;
    std::string current_file_name;
    bool current_date_processed;
    std::map<std::string, std::string> initial_top_level_parents;
    
    std::set<std::pair<std::string, std::string>> parent_child_buffer;

    void _open_output_files();
    void _close_output_files();

    void _handle_date_line(const std::string& line);
    void _handle_status_line(const std::string& line);
    void _handle_remark_line(const std::string& line);
    void _handle_getup_line(const std::string& line);
    void _handle_time_record_line(const std::string& line, int line_num);
    void _process_project_path(const std::string& project_path_orig);
    void _write_previous_date_data();
    void _flush_parent_child_buffer();
};

#endif // DATA_PARSER_H