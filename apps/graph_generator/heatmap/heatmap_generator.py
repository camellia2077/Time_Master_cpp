# heatmap_generator.py
import sqlite3
import tomllib
import datetime
import os
from typing import Dict, Any

from heatmap_renderer import HeatmapRenderer
from heatmap_strategies import NumericStrategy, BooleanStrategy

SCRIPT_DIRECTORY = os.path.dirname(os.path.abspath(__file__))

def load_config(path: str) -> Dict[str, Any]:
    """从 TOML 文件加载配置。"""
    try:
        with open(path, "rb") as f:
            return tomllib.load(f)
    except FileNotFoundError:
        print(f"错误: 配置文件 '{path}' 未找到。")
        exit(1)

def fetch_project_duration_data(db_path: str, project_name: str, year: int) -> Dict[datetime.date, float]:
    """从数据库获取数值型项目数据（如时长）。"""
    print(f"\n--- 正在查询项目 '{project_name}' 的时长数据 ---")
    data = {}
    conn = None
    try:
        conn = sqlite3.connect(db_path)
        cursor = conn.cursor()
        query = """
        SELECT date, SUM(duration) FROM time_records
        WHERE project_path LIKE ? AND SUBSTR(date, 1, 4) = ?
        GROUP BY date;
        """
        params = (f"{project_name}%", str(year))
        cursor.execute(query, params)
        for row in cursor.fetchall():
            date_str, total_seconds = row
            current_date = datetime.datetime.strptime(date_str, "%Y%m%d").date()
            data[current_date] = total_seconds / 3600.0
        print(f"成功查询到 {len(data)} 天的数据。")
        return data
    except sqlite3.Error as e:
        print(f"数据库查询出错: {e}")
        return {}
    finally:
        if conn:
            conn.close()

def fetch_boolean_data(db_path: str, column_name: str, year: int) -> Dict[datetime.date, int]:
    """从数据库获取布尔型数据 (0或1)。"""
    print(f"\n--- 正在查询布尔字段 '{column_name}' 的数据 ---")
    data = {}
    conn = None
    try:
        conn = sqlite3.connect(db_path)
        cursor = conn.cursor()
        if column_name not in ["sleep", "status", "exercise"]:
            raise ValueError(f"不安全的列名: {column_name}")
        
        query = f"""
        SELECT date, {column_name} FROM days
        WHERE SUBSTR(date, 1, 4) = ?;
        """
        cursor.execute(query, (str(year),))
        for row in cursor.fetchall():
            date_str, value = row
            current_date = datetime.datetime.strptime(date_str, "%Y%m%d").date()
            data[current_date] = value
        print(f"成功查询到 {len(data)} 天的数据。")
        return data
    except sqlite3.Error as e:
        print(f"数据库查询出错: {e}")
        return {}
    finally:
        if conn:
            conn.close()

def main():
    """主函数：加载配置、查询数据、生成所有配置的热力图。"""
    config_path = os.path.join(SCRIPT_DIRECTORY, "heatmap_config.toml")
    color_config_path = os.path.join(SCRIPT_DIRECTORY, "heatmap_colors.toml")
    config = load_config(config_path)
    color_config = load_config(color_config_path)
    
    db_path = config["database"]["path"]
    year = config["heatmap"]["year"]

    project_name = config["heatmap"]["project_name"]
    heatmap_data = fetch_project_duration_data(db_path, project_name, year)
    if heatmap_data:
        strategy_color_config = {
            'palette': color_config["COLOR_PALETTES"][config["heatmap"]["color_palette"]],
            'over_12h': color_config["SINGLE_COLORS"][config["heatmap"]["over_12_hours_color"]]
        }
        numeric_strategy = NumericStrategy(project_name, strategy_color_config)
        renderer = HeatmapRenderer(year, heatmap_data, numeric_strategy)
        
        annual_output = os.path.join(SCRIPT_DIRECTORY, config["heatmap"]["output_filename_annual"])
        monthly_output = os.path.join(SCRIPT_DIRECTORY, config["heatmap"]["output_filename_monthly"])
        
        renderer.save_annual_heatmap(annual_output)
        print(f"✅ 年度热力图已保存: {annual_output}")
        renderer.save_monthly_heatmap(monthly_output)
        print(f"✅ 月度热力图已保存: {monthly_output}")

    bool_reports = config.get("boolean_heatmaps", {}).get("enabled_reports", [])
    if bool_reports:
        bool_color_config = color_config.get("BOOLEAN_COLORS", {})
        
        for report_type in bool_reports:
            bool_data = fetch_boolean_data(db_path, report_type, year)
            if bool_data:
                # ✨ 核心修改：将查询到的数据传递给策略类
                bool_strategy = BooleanStrategy(report_type, bool_color_config, bool_data)
                renderer = HeatmapRenderer(year, bool_data, bool_strategy)
                
                output_basename = config["boolean_heatmaps"]["outputs"].get(report_type, report_type)
                annual_output = os.path.join(SCRIPT_DIRECTORY, f"{output_basename}_annual.html")
                monthly_output = os.path.join(SCRIPT_DIRECTORY, f"{output_basename}_monthly.html")
                
                renderer.save_annual_heatmap(annual_output)
                print(f"✅ 年度热力图已保存: {annual_output}")
                renderer.save_monthly_heatmap(monthly_output)
                print(f"✅ 月度热力图已保存: {monthly_output}")

if __name__ == "__main__":
    main()