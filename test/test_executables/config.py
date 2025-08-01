# config.py
from pathlib import Path

# --- ANSI Color Codes ---
class Colors:
    """A class to hold ANSI color codes for colored console output."""
    CYAN = '\033[96m'
    GREEN = '\033[92m'
    RED = '\033[91m'
    RESET = '\033[0m'

# --- File and Directory Paths ---
SOURCE_EXECUTABLES_DIR = Path("C:/Computer/my_github/github_cpp/New_time_master/Time_Master_cpp/time_master/build")
SOURCE_DATA_PARENT_DIR = Path("C:/Computer/my_github/github_cpp/New_time_master/my_test")
SOURCE_DATA_FOLDER_NAME = "Date"
SOURCE_DATA_PATH = SOURCE_DATA_PARENT_DIR / SOURCE_DATA_FOLDER_NAME
TARGET_EXECUTABLES_DIR = Path("./") # The current directory where the script is run

# --- Executable and Database Names ---
EXECUTABLE_CLI_NAME = "time_tracker_cli.exe"
EXECUTABLE_APP_NAME = "time_tracker_app.exe"
GENERATED_DB_FILE_NAME = "time_data.db"
PROCESSED_DATA_DIR_NAME = f"Processed_{SOURCE_DATA_FOLDER_NAME}"

# ==============================================================================
#                      TEST PARAMETERS CONFIGURATION
# ==============================================================================

# --- Parameters for Query Tests ---
DAILY_QUERY_DATES = ["20250501", "20250601"]
MONTHLY_QUERY_MONTHS = ["202505", "202506"]
# 修改: 将周期天数改为整数列表
PERIOD_QUERY_DAYS = [7, 10, 15]

# --- Parameters for Export Tests ---
#  总开关: True -> 批量导出, False -> 按指定列表导出
EXPORT_MODE_IS_BULK = False

# 修改: 将周期天数改为整数列表
PERIOD_EXPORT_DAYS = [7, 10, 15] 

# 仅在 EXPORT_MODE_IS_BULK = False 时生效
SPECIFIC_EXPORT_DATES = ["20250501", "20250601"]
SPECIFIC_EXPORT_MONTHS = ["202505", "202506"]
# ==============================================================================


# --- Artifacts to be cleaned up before each test run ---
DIRECTORIES_TO_CLEAN = [
    PROCESSED_DATA_DIR_NAME,
    "Processed_Date", # Legacy name, can be removed if no longer generated
    "output",
    "exported_files"
]
