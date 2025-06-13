#ifndef COMMON_UTILS_H
#define COMMON_UTILS_H

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <stdexcept>
#include <numeric>
#include <cctype>

// --- ANSI Color Code Declarations ---
// By declaring them here, any part of the application that includes common_utils.h
// can use these constants for colored console output.
extern const std::string ANSI_COLOR_GREEN;
extern const std::string ANSI_COLOR_RESET;


// --- Structure Definitions ---

// Represents a node in the project hierarchy for time aggregation 
struct ProjectNode {
    long long duration = 0;
    std::map<std::string, ProjectNode> children;
};

// Alias for the project tree structure 
using ProjectTree = std::map<std::string, ProjectNode>;

// --- Function Declarations ---

// Helper function to convert HH:MM or HHMM time string to seconds since midnight 
int time_str_to_seconds(const std::string& time_str_in);

// Formats total seconds into a string like "XhYYm" or "YYm", with optional averaging. 
std::string time_format_duration(long long total_seconds, int avg_days = 1);

// Splits a string by a delimiter. 
std::vector<std::string> split_string(const std::string& s, char delimiter);

#endif // COMMON_UTILS_H