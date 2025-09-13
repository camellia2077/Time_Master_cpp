# heatmap_generator.py
import sqlite3
import tomllib
import datetime
import os  # 导入 os 模块
from typing import Dict, Any

# 确保这些文件与此脚本在同一目录或Python路径中
from heatmap_renderer import HeatmapRenderer
from heatmap_strategies import NumericStrategy

# ✨ 核心修正：获取脚本文件自身所在的目录
SCRIPT_DIRECTORY = os.path.dirname(os.path.abspath(__file__))

def load_config(path: str) -> Dict[str, Any]:
    """从 TOML 文件加载配置。"""
    try:
        # ✨ 核心修正：使用文件的绝对路径来加载
        with open(path, "rb") as f:
            return tomllib.load(f)
    except FileNotFoundError:
        print(f"错误: 配置文件 '{path}' 未找到。")
        exit(1)

def fetch_project_duration_data(db_path: str, project_name: str, year: int) -> Dict[datetime.date, float]:
    """从数据库中获取指定项目在某年的每日总时长（小时）。"""
    data = {}
    conn = None
    try:
        conn = sqlite3.connect(db_path)
        cursor = conn.cursor()
        
        query = """
        SELECT
            date,
            SUM(duration)
        FROM
            time_records
        WHERE
            project_path LIKE ? AND SUBSTR(date, 1, 4) = ?
        GROUP BY
            date;
        """
        
        params = (f"{project_name}%", str(year))
        cursor.execute(query, params)
        
        for row in cursor.fetchall():
            date_str, total_seconds = row
            current_date = datetime.datetime.strptime(date_str, "%Y%m%d").date()
            total_hours = total_seconds / 3600.0
            data[current_date] = total_hours
            
        print(f"成功从数据库查询到 {len(data)} 天的 '{project_name}' 活动数据。")
        return data

    except sqlite3.Error as e:
        print(f"数据库查询出错: {e}")
        return {}
    finally:
        if conn:
            conn.close()

def main():
    """主函数：加载配置、查询数据、生成热力图。"""
    # 1. 加载配置
    # ✨ 核心修正：构建配置文件的绝对路径
    config_path = os.path.join(SCRIPT_DIRECTORY, "heatmap_config.toml")
    color_config_path = os.path.join(SCRIPT_DIRECTORY, "heatmap_colors.toml")

    config = load_config(config_path)
    color_config = load_config(color_config_path)
    
    db_path = config["database"]["path"]
    year = config["heatmap"]["year"]
    project_name = config["heatmap"]["project_name"]
    annual_output_file = config["heatmap"]["output_filename_annual"]
    monthly_output_file = config["heatmap"]["output_filename_monthly"]
    
    # 2. 从数据库获取数据
    heatmap_data = fetch_project_duration_data(db_path, project_name, year)
    if not heatmap_data:
        print("未获取到任何数据，程序将退出。")
        return

    # 3. 准备渲染策略
    strategy_color_config = {
        'palette': color_config["COLOR_PALETTES"][config["heatmap"]["color_palette"]],
        'over_12h': color_config["SINGLE_COLORS"][config["heatmap"]["over_12_hours_color"]]
    }
    numeric_strategy = NumericStrategy(project_name, strategy_color_config)
    
    # 4. 初始化渲染器并生成热力图
    renderer = HeatmapRenderer(year, heatmap_data, numeric_strategy)
    
    # ✨ 核心修正：构建输出文件的绝对路径，确保保存在脚本所在目录
    annual_output_path = os.path.join(SCRIPT_DIRECTORY, annual_output_file)
    monthly_output_path = os.path.join(SCRIPT_DIRECTORY, monthly_output_file)

    renderer.save_annual_heatmap(annual_output_path)
    print(f"✅ 年度热力图已成功保存到: {annual_output_path}")

    renderer.save_monthly_heatmap(monthly_output_path)
    print(f"✅ 月度热力图已成功保存到: {monthly_output_path}")

if __name__ == "__main__":
    main()