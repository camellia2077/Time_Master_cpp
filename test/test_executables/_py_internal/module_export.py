# _py_internal/module_export.py
from pathlib import Path
from .base_module import BaseTester, TestCounter

class ExportTester(BaseTester):
    """用于数据导出测试的模块。"""
    def __init__(self, counter: TestCounter, module_order: int,
                 generated_db_file_name: str, 
                 is_bulk_mode: bool,
                 specific_dates: list,
                 specific_months: list,
                 period_export_days: list, # 修改: 接收列表
                 executable_to_run: str, source_data_path: Path, converted_text_dir_name: str):
        super().__init__(counter, module_order, "export",
                         executable_to_run, source_data_path, converted_text_dir_name)
        self.db_file = Path.cwd() / generated_db_file_name
        self.is_bulk_mode = is_bulk_mode
        self.specific_dates = specific_dates
        self.specific_months = specific_months
        self.period_days_to_export = period_export_days

    def run_tests(self) -> bool:
        """运行所有与数据导出相关的测试。"""
        if not self.db_file.exists():
            print(f"警告: 跳过导出测试，因为数据库文件 '{self.db_file.name}' 不存在。")
            return True

        formats = ["md", "tex", "typ"]
        tests_to_run = []

        if self.is_bulk_mode:
            print("  模式: 全部导出")
            # ... (这部分逻辑保持不变) ...
            daily_cmd = ["--export", "all-daily"]
            monthly_cmd = ["--export", "all-monthly"]
            daily_test_name = "Bulk Export All Daily"
            monthly_test_name = "Bulk Export All Monthly"
            
            for fmt in formats:
                tests_to_run.append((f"{daily_test_name} [{fmt.upper()}]", daily_cmd + ["--format", fmt]))
                tests_to_run.append((f"{monthly_test_name} [{fmt.upper()}]", monthly_cmd + ["--format", fmt]))
        else:
            print("  模式: 指定日期/月份导出")
            # ... (这部分逻辑保持不变) ...
            for date in self.specific_dates:
                daily_cmd = ["--export", "daily", date]
                daily_test_name = f"Specific Export Daily ({date})"
                for fmt in formats:
                    tests_to_run.append((f"{daily_test_name} [{fmt.upper()}]", daily_cmd + ["--format", fmt]))

            for month in self.specific_months:
                monthly_cmd = ["--export", "monthly", month]
                monthly_test_name = f"Specific Export Monthly ({month})"
                for fmt in formats:
                    tests_to_run.append((f"{monthly_test_name} [{fmt.upper()}]", monthly_cmd + ["--format", fmt]))

        # 修改: 将整数列表转换为逗号分隔的字符串以用于命令行
        period_days_str = ",".join(map(str, self.period_days_to_export))
        
        # 根据导出模式选择正确的 period 命令
        if self.is_bulk_mode:
            period_cmd = ["--export", "all-period", period_days_str]
            period_test_name = f"Bulk Period Export ({period_days_str})"
        else:
             # 在指定导出模式下，我们循环导出单个周期报告
            for days in self.period_days_to_export:
                period_cmd = ["--export", "period", str(days)]
                period_test_name = f"Specific Period Export ({days} days)"
                for fmt in formats:
                    tests_to_run.append((f"{period_test_name} [{fmt.upper()}]", period_cmd + ["--format", fmt]))
            # 清除 period_cmd 以避免重复添加
            period_cmd = None

        # 如果是批量模式，则添加批量周期测试
        if period_cmd:
            period_test_name = f"Bulk Period Export ({period_days_str})"
            for fmt in formats:
                tests_to_run.append((f"{period_test_name} [{fmt.upper()}]", period_cmd + ["--format", fmt]))
        
        for name, args in tests_to_run:
            if not self.run_command_test(name, args):
                return False
                
        return True
