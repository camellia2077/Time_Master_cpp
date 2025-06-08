#include <iostream>
#include <vector>
#include <string>
#include <iomanip>     // For std::setw, std::setfill
#include <sstream>     // For std::ostringstream, std::to_string
#include <fstream>     // For std::ofstream, std::ifstream
#include <stdexcept>   // For std::invalid_argument, std::out_of_range
#include <random>      // For random number generation
#include <cmath>       // For std::round
#include <memory>      // For std::unique_ptr
#include <chrono>      // For timing the execution
#include <optional>    // For safely returning from argument parsing and file loading

// Include the nlohmann/json library
#include "json.hpp" // Assuming json.hpp is in the same directory or accessible via include paths

// For Windows-specific console settings
#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#endif

// Use nlohmann::json
using json = nlohmann::json;

// --- Color and Console Setup ---

// Structure to hold ANSI color codes for console output.
struct ConsoleColors {
    static const std::string red;
    static const std::string green;
    static const std::string reset;
};

// Define the static members of ConsoleColors
const std::string ConsoleColors::red = "\033[1;31m";
const std::string ConsoleColors::green = "\033[1;32m";
const std::string ConsoleColors::reset = "\033[0m";

// Sets up the console for UTF-8 and enables ANSI color codes on Windows.
void setup_console() {
#if defined(_WIN32) || defined(_WIN64)
    // Set console output to UTF-8
    SetConsoleOutputCP(CP_UTF8);

    // Enable virtual terminal processing to handle ANSI escape sequences
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
        DWORD dwMode = 0;
        if (GetConsoleMode(hOut, &dwMode)) {
            dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            SetConsoleMode(hOut, dwMode);
        }
    }
#endif
}


// --- Configuration and Helper Functions ---

struct Config {
    int num_days;
    int items_per_day;
};

std::string format_two_digits(int n) {
    std::ostringstream ss;
    ss << std::setw(2) << std::setfill('0') << n;
    return ss.str();
}

void print_usage(const char* prog_name) {
    std::cerr << ConsoleColors::red << "Usage: " << prog_name << " <num_days> <items_per_day>" << ConsoleColors::reset << '\n';
    std::cerr << "Description: Generates test log data. Reads activities from 'activities_config.json'." << '\n';
    std::cerr << "  <num_days>        : Total number of days to generate (positive integer)." << '\n';
    std::cerr << "  <items_per_day>   : Number of log items per day (positive integer)." << '\n';
    std::cerr << "Example: " << prog_name << " 10 5" << '\n';
}

// --- Core Logic Functions ---

std::optional<std::vector<std::string>> load_activities_from_json(const std::string& json_filename) {
    std::ifstream f(json_filename);
    if (!f.is_open()) {
        std::cerr << ConsoleColors::red << "Error: Could not open activities configuration file '" << json_filename << "'." << ConsoleColors::reset << '\n';
        return std::nullopt;
    }

    try {
        json data = json::parse(f);
        f.close();

        if (data.contains("common_activities") && data["common_activities"].is_array()) {
            auto activities = data["common_activities"].get<std::vector<std::string>>();
            if (activities.empty()) 
            {
                std::cerr << ConsoleColors::red << "Error: "  << ConsoleColors::reset << "\"common_activities\" array in '" << json_filename << "' is empty." << '\n';
                return std::nullopt;
            }
            std::cout << ConsoleColors::green << "Successfully loaded " <<  ConsoleColors::reset << " activities from '" << json_filename << "'."  << '\n';
            return activities;
        } else {
            std::cerr << ConsoleColors::red << "Error: " << ConsoleColors::reset << "JSON file '" << json_filename   << "' must contain a non-empty 'common_activities' array." << '\n';
            return std::nullopt;
        }
    } catch (const json::parse_error& e) {
        std::cerr << ConsoleColors::red << "Error: " << ConsoleColors::reset << "Failed to parse JSON from '" << json_filename << "'. Detail: " << e.what()  << '\n';
        if (f.is_open()) f.close();
        return std::nullopt;
    } catch (const json::type_error& e) {
        std::cerr << ConsoleColors::red << "Error: "<< ConsoleColors::reset << "JSON type error in '" << json_filename << "'. Is 'common_activities' an array of strings? Detail: " << e.what() << '\n';
        if (f.is_open()) f.close();
        return std::nullopt;
    } catch (const std::exception& e) {
        std::cerr << ConsoleColors::red << "Error: "<< ConsoleColors::reset << "An unexpected error occurred while processing '" << json_filename << "'. Detail: " << e.what() << '\n';
        if (f.is_open()) f.close();
        return std::nullopt;
    }
}

std::optional<Config> parse_arguments(int argc, char* argv[]) {
    if (argc != 3) {
        print_usage(argv[0]);
        return std::nullopt;
    }

    Config config;
    try {
        config.num_days = std::stoi(argv[1]);
        config.items_per_day = std::stoi(argv[2]);
    } catch (const std::invalid_argument&) {
        std::cerr << ConsoleColors::red << "Error: Invalid argument. <num_days> and <items_per_day> must be integers." << ConsoleColors::reset << '\n';
        print_usage(argv[0]);
        return std::nullopt;
    } catch (const std::out_of_range&) {
        std::cerr << ConsoleColors::red << "Error: Argument out of range." << ConsoleColors::reset << '\n';
        print_usage(argv[0]);
        return std::nullopt;
    }

    if (config.num_days <= 0 || config.items_per_day <= 0) {
        std::cerr << ConsoleColors::red << "Error: <num_days> and <items_per_day> must be positive integers." << ConsoleColors::reset << '\n';
        print_usage(argv[0]);
        return std::nullopt;
    }
    if (config.items_per_day < 2) {
        std::cerr << ConsoleColors::red << "Error: <items_per_day> must be at least 2 to include '起床' and '睡觉长'." << ConsoleColors::reset << '\n';
        return std::nullopt;
    }

    return config;
}

std::string generate_log_data(const Config& config, const std::vector<std::string>& common_activities) {
    std::ostringstream log_content_stream;
    
    int current_month = 1;
    int current_day_of_month = 1;

    std::mt19937 gen(std::random_device{}());
    std::uniform_int_distribution<> dis_minute(0, 59);
    std::uniform_int_distribution<> dis_activity_selector(0, static_cast<int>(common_activities.size()) - 1);

    int minute_for_todays_actual_wakeup = dis_minute(gen);

    for (int d = 0; d < config.num_days; ++d) {
        log_content_stream << format_two_digits(current_month) << format_two_digits(current_day_of_month) << '\n';
        int minute_for_next_days_scheduled_wakeup = dis_minute(gen);

        for (int i = 0; i < config.items_per_day; ++i) {
            int display_hour_final;
            int event_minute_final;
            std::string event_text_to_use_final;

            if (i == 0) {
                event_text_to_use_final = "起床";
                display_hour_final = 6;
                event_minute_final = minute_for_todays_actual_wakeup;
            } else if (i == config.items_per_day - 1) {
                event_text_to_use_final = "睡觉长";
                display_hour_final = 6;
                event_minute_final = minute_for_next_days_scheduled_wakeup;
            } else {
                double progress_ratio = static_cast<double>(i) / (config.items_per_day - 1);
                int logical_event_hour = 6 + static_cast<int>(std::round(progress_ratio * 19.0));
                if (logical_event_hour > 25) logical_event_hour = 25;
                display_hour_final = (logical_event_hour >= 24) ? logical_event_hour - 24 : logical_event_hour;
                event_minute_final = dis_minute(gen);
                event_text_to_use_final = common_activities[dis_activity_selector(gen)];
            }
            log_content_stream << format_two_digits(display_hour_final) << format_two_digits(event_minute_final) << event_text_to_use_final << '\n';
        }

        minute_for_todays_actual_wakeup = minute_for_next_days_scheduled_wakeup;

        current_day_of_month++;
        int days_in_current_month = 31;
        if (current_month == 4 || current_month == 6 || current_month == 9 || current_month == 11) {
            days_in_current_month = 30;
        } else if (current_month == 2) {
            days_in_current_month = 28;
        }

        if (current_day_of_month > days_in_current_month) {
            current_day_of_month = 1;
            current_month = (current_month % 12) + 1;
        }

        if (d < config.num_days - 1) {
            log_content_stream << '\n';
        }
    }
    return log_content_stream.str();
}

bool write_output_file(const std::string& filename, const std::string& content) {
    std::ofstream outFile(filename);
    if (!outFile.is_open()) {
        std::cerr << ConsoleColors::red << "Error: Could not open file '" << filename << "' for writing." << ConsoleColors::reset << '\n';
        return false;
    }
    outFile << content;
    outFile.close();
    return true;
}

// --- Main Execution ---

int main(int argc, char* argv[]) {
    // Setup console for colors and UTF-8
    setup_console();

    // 1. Parse Arguments
    auto config_opt = parse_arguments(argc, argv);
    if (!config_opt) {
        return 1;
    }
    Config config = *config_opt;

    // 2. Load Activities
    auto activities_opt = load_activities_from_json("activities_config.json");
    if (!activities_opt) {
        std::cerr << ConsoleColors::red << "Exiting program due to configuration loading failure." << ConsoleColors::reset << std::endl;
        return 1;
    }
    const std::vector<std::string>& common_activities = *activities_opt;
    
    auto start_time = std::chrono::high_resolution_clock::now();

    // 3. Generate Log Data
    std::cout << "Generating data....." << '\n';
    std::string log_content = generate_log_data(config, common_activities);

    // 4. Write to File
    std::ostringstream filename_ss;
    filename_ss << "log_" << config.num_days << "_items_" << config.items_per_day << ".txt";
    std::string output_filename = filename_ss.str();

    if (!write_output_file(output_filename, log_content)) {
        return 1;
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    auto duration_s = std::chrono::duration<double>(end_time - start_time);

    std::cout << ConsoleColors::green << "Data generation complete. "<< ConsoleColors::reset << "Output is in '" << output_filename << "'"  << '\n';
    std::cout <<  "Total generation time: " << duration_ms.count() << " ms (" << std::fixed << std::setprecision(3) << duration_s.count() << " s)." << '\n';

    return 0;
}
