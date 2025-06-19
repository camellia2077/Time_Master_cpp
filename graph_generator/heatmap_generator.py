# heatmap_generator.py (Refactored and Modified)

import argparse
import datetime
import os
import sys
import json
import calendar
from typing import Dict, Any, List

# Import the new data access function and color constants
import db_access
from db_access import COLOR_GREEN, COLOR_RED, COLOR_RESET

# --- Configuration ---
# DB_PATH and other db-related configs are now managed in db_access.py
COLOR_CONFIG_PATH = 'heatmap_config.json'

# The DataReader class has been removed and replaced by the db_access module.

class HeatmapGenerator:
    """
    根据提供的数据生成HTML和SVG格式的热力图。
    """
    def __init__(self, color_config: Dict[str, Any]):
        self.color_palette = color_config.get('palette')
        self.over_12h_color = color_config.get('over_12h_color')
        if not self.color_palette or not self.over_12h_color:
            print(f"{COLOR_RED}❌ 错误: 颜色配置不完整。{COLOR_RESET}", file=sys.stderr)
            sys.exit(1)
        print("  ✔️  HeatmapGenerator 初始化成功。")

    def _get_color_for_hours(self, hours: float) -> str:
        if hours > 12:
            return self.over_12h_color
        elif hours > 10:
            return self.color_palette[4]
        elif hours > 8:
            return self.color_palette[3]
        elif hours > 4:
            return self.color_palette[2]
        elif hours > 0:
            return self.color_palette[1]
        else:
            return self.color_palette[0]

    def _generate_annual_svg_content(self, year: int, project_name: str, data: Dict[datetime.date, float]) -> str:
        SQUARE_SIZE, SQUARE_GAP, SQUARE_RADIUS = 12, 3, 2
        GRID_UNIT = SQUARE_SIZE + SQUARE_GAP
        LEFT_PADDING, TOP_PADDING = 30, 30
        start_date = datetime.date(year, 1, 1)
        total_days = (datetime.date(year, 12, 31) - start_date).days + 1
        first_day_weekday = (start_date.isoweekday()) % 7
        num_weeks = (total_days + first_day_weekday + 6) // 7
        svg_width = num_weeks * GRID_UNIT + LEFT_PADDING
        svg_height = 7 * GRID_UNIT + TOP_PADDING
        rects_html, month_labels_html = [], []
        last_month = -1
        for day_index in range(total_days):
            current_date = start_date + datetime.timedelta(days=day_index)
            grid_day_index = day_index + first_day_weekday
            week_x, day_y = grid_day_index // 7, grid_day_index % 7
            x_pos, y_pos = week_x * GRID_UNIT + LEFT_PADDING, day_y * GRID_UNIT + TOP_PADDING
            hours = data.get(current_date, 0)
            color = self._get_color_for_hours(hours)
            tooltip = f"{hours:.2f} 小时 {project_name} on {current_date.strftime('%Y-%m-%d')}"
            rects_html.append(
                f'    <rect width="{SQUARE_SIZE}" height="{SQUARE_SIZE}" x="{x_pos}" y="{y_pos}" '
                f'fill="{color}" rx="{SQUARE_RADIUS}" ry="{SQUARE_RADIUS}">'
                f'<title>{tooltip}</title></rect>'
            )
            if current_date.month != last_month and current_date.weekday() < 3:
                month_labels_html.append(
                    f'  <text x="{x_pos}" y="{TOP_PADDING - 10}" class="month">{current_date.strftime("%b")}</text>'
                )
                last_month = current_date.month
        day_labels_html = [
            f'<text x="{LEFT_PADDING - 10}" y="{TOP_PADDING + GRID_UNIT * 1 + SQUARE_SIZE / 1.5}" class="day-label">M</text>',
            f'<text x="{LEFT_PADDING - 10}" y="{TOP_PADDING + GRID_UNIT * 3 + SQUARE_SIZE / 1.5}" class="day-label">W</text>',
            f'<text x="{LEFT_PADDING - 10}" y="{TOP_PADDING + GRID_UNIT * 5 + SQUARE_SIZE / 1.5}" class="day-label">F</text>'
        ]
        return f"""
        <svg width="{svg_width}" height="{svg_height}">
            {"\n".join(month_labels_html)}
            {"\n".join(day_labels_html)}
            {"\n".join(rects_html)}
        </svg>
        """
    
    def _generate_monthly_svg_content(self, year: int, month: int, project_name: str, data: Dict[datetime.date, float]) -> str:
        SQUARE_SIZE, SQUARE_GAP, SQUARE_RADIUS = 12, 3, 2
        GRID_UNIT = SQUARE_SIZE + SQUARE_GAP
        LEFT_PADDING, TOP_PADDING = 30, 20
        month_start_date = datetime.date(year, month, 1)
        num_days_in_month = calendar.monthrange(year, month)[1]
        first_day_weekday = (month_start_date.isoweekday()) % 7
        num_weeks = (num_days_in_month + first_day_weekday + 6) // 7
        svg_width = num_weeks * GRID_UNIT + LEFT_PADDING
        svg_height = 7 * GRID_UNIT + TOP_PADDING
        rects_html = []
        for day_index in range(num_days_in_month):
            current_date = month_start_date + datetime.timedelta(days=day_index)
            grid_day_index = day_index + first_day_weekday
            week_x, day_y = grid_day_index // 7, grid_day_index % 7
            x_pos = week_x * GRID_UNIT + LEFT_PADDING
            y_pos = day_y * GRID_UNIT + TOP_PADDING
            hours = data.get(current_date, 0)
            color = self._get_color_for_hours(hours)
            tooltip = f"{hours:.2f} 小时 {project_name} on {current_date.strftime('%Y-%m-%d')}"
            rects_html.append(
                f'    <rect width="{SQUARE_SIZE}" height="{SQUARE_SIZE}" x="{x_pos}" y="{y_pos}" '
                f'fill="{color}" rx="{SQUARE_RADIUS}" ry="{SQUARE_RADIUS}">'
                f'<title>{tooltip}</title></rect>'
            )
        day_labels_html = [
            f'<text x="{LEFT_PADDING - 15}" y="{TOP_PADDING + GRID_UNIT * 1 + SQUARE_SIZE / 1.5}" class="day-label">M</text>',
            f'<text x="{LEFT_PADDING - 15}" y="{TOP_PADDING + GRID_UNIT * 3 + SQUARE_SIZE / 1.5}" class="day-label">W</text>',
            f'<text x="{LEFT_PADDING - 15}" y="{TOP_PADDING + GRID_UNIT * 5 + SQUARE_SIZE / 1.5}" class="day-label">F</text>'
        ]
        return f"""
        <div class="monthly-heatmap">
            <h3>{month_start_date.strftime("%B")}</h3>
            <svg width="{svg_width}" height="{svg_height}">
                {"\n".join(day_labels_html)}
                {"\n".join(rects_html)}
            </svg>
        </div>
        """

    def generate_html(self, year: int, project_name: str, data: Dict[datetime.date, float]) -> str:
        annual_svg_content = self._generate_annual_svg_content(year, project_name, data)
        monthly_svgs_html = [self._generate_monthly_svg_content(year, m, project_name, data) for m in range(1, 13)]
        monthly_heatmaps_content = "\n".join(monthly_svgs_html)
        display_project_name = project_name.capitalize()
        html_template = f"""
<!DOCTYPE html>
<html lang="zh-CN">
<head>
    <meta charset="UTF-8"><meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>{display_project_name} 热力图 - {year}</title>
    <style>
        body {{ font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif; background-color: #f6f8fa; color: #24292e; display: flex; justify-content: center; align-items: center; flex-direction: column; margin: 2em; }}
        .heatmap-container {{ border: 1px solid #e1e4e8; border-radius: 6px; padding: 20px; background-color: #ffffff; max-width: 100%; overflow-x: auto; margin-bottom: 2em; }}
        .monthly-heatmaps-container {{ display: flex; flex-wrap: wrap; justify-content: center; gap: 20px; max-width: 1000px; }}
        .monthly-heatmap {{ border: 1px solid #e1e4e8; border-radius: 6px; padding: 15px; background-color: #ffffff; }}
        svg {{ display: block; margin: 0 auto; }}
        .month, .day-label {{ font-size: 10px; fill: #586069; }}
        .day-label {{ text-anchor: start; }}
        h1, h2, h3 {{ font-weight: 400; text-align: center; }}
        h1 {{ margin-bottom: 1em; }}
        h2 {{ margin-top: 2em; margin-bottom: 1em; width: 100%; }}
        h3 {{ margin-top: 0; margin-bottom: 10px; }}
        rect:hover {{ stroke: #24292e; stroke-width: 1px; }}
    </style>
</head>
<body>
    <h1>{display_project_name} 年度热力图 - {year}</h1>
    <div class="heatmap-container">{annual_svg_content}</div>
    <h2>每月详情</h2>
    <div class="monthly-heatmaps-container">{monthly_heatmaps_content}</div>
</body>
</html>"""
        return html_template

def load_config(config_path: str) -> Dict[str, Any]:
    print(f"🎨 [步骤 1/3] 正在从 '{config_path}' 加载配置...")
    try:
        with open(config_path, 'r', encoding='utf-8') as f:
            config = json.load(f)
    except FileNotFoundError:
        print(f"{COLOR_RED}❌ 错误: 配置文件 '{config_path}' 未找到。{COLOR_RESET}", file=sys.stderr)
        sys.exit(1)
    except json.JSONDecodeError:
        print(f"{COLOR_RED}❌ 错误: 配置文件 '{config_path}' 格式无效。{COLOR_RESET}", file=sys.stderr)
        sys.exit(1)
    
    # Load color configuration
    palette_name = config.get("DEFAULT_COLOR_PALETTE_NAME")
    color_palette = config.get("COLOR_PALETTES", {}).get(palette_name)
    over_12h_ref = config.get("OVER_12_HOURS_COLOR_REF")
    over_12h_color = config.get("SINGLE_COLORS", {}).get(over_12h_ref)
    
    if not all([palette_name, color_palette, over_12h_ref, over_12h_color]):
        print(f"{COLOR_RED}❌ 错误: 颜色配置文件中的键缺失或无效。{COLOR_RESET}", file=sys.stderr)
        sys.exit(1)
        
    # Load project names, default to ["mystudy"]
    project_names = config.get("PARENT_PROJECTS", ["mystudy"])
    if not isinstance(project_names, list) or not all(isinstance(p, str) for p in project_names):
        print(f"{COLOR_RED}❌ 错误: 'PARENT_PROJECTS' 在配置文件中必须是一个字符串列表。{COLOR_RESET}", file=sys.stderr)
        sys.exit(1)

    print(f"{COLOR_GREEN}✅ [步骤 1/3] 配置加载成功。{COLOR_RESET}")
    return {
        "colors": {"palette": color_palette, "over_12h_color": over_12h_color},
        "projects": project_names
    }

def write_html_to_file(filename: str, content: str):
    try:
        with open(filename, "w", encoding="utf-8") as f:
            f.write(content)
    except IOError as e:
        print(f"{COLOR_RED}❌ 写入文件 '{filename}' 时出错: {e}{COLOR_RESET}", file=sys.stderr)
        sys.exit(1)

def main():
    parser = argparse.ArgumentParser(
        description="从 time_data.db 数据库为在配置文件中指定的项目生成GitHub风格的热力图。",
        formatter_class=argparse.RawTextHelpFormatter
    )
    parser.add_argument("year", type=int, help="要生成热力图的年份 (例如: 2024)。")
    args = parser.parse_args()
    year = args.year

    print("🚀 启动热力图生成器...")
    try:
        # 1. 加载配置 (颜色和项目)
        config = load_config(COLOR_CONFIG_PATH)
        color_config = config["colors"]
        project_names = config["projects"]
        
        # 2. 初始化生成器
        generator = HeatmapGenerator(color_config)
        generated_files_count = 0

        # 3. 为每个项目生成热力图
        for project_name in project_names:
            project_name = project_name.lower()
            print(f"\n▶️  正在处理项目: '{project_name.capitalize()}'")
            
            # a. 获取数据
            print(f"  [1/3] 🔍 正在为项目 '{project_name}' 检索 {year} 年的数据...")
            project_data = db_access.get_data_for_heatmap(year, project_name)
            if not project_data:
                print(f"  ⚠️  未找到项目 '{project_name}' 在 {year} 年的数据。跳过此项目。")
                continue
            print("  ✅ 数据检索完成。")

            # b. 生成HTML
            print(f"  [2/3] 🎨 正在为 '{project_name}' 生成HTML内容...")
            html_content = generator.generate_html(year, project_name, project_data)
            print(f"  ✅ HTML内容已为 '{project_name}' 生成。")

            # c. 写入文件
            output_filename = f"heatmap_{project_name}_{year}.html"
            print(f"  [3/3] 📄 正在将HTML写入文件 '{output_filename}'...")
            write_html_to_file(output_filename, html_content)
            print(f"  ✅ 文件写入完成: {output_filename}")
            generated_files_count += 1
        
        print(f"\n🎉 全部完成！已成功为 {generated_files_count} 个项目生成热力图。")

    except Exception as e:
        print(f"\n{COLOR_RED}❌ 主进程中发生意外错误: {e}{COLOR_RESET}", file=sys.stderr)
        sys.exit(1)

if __name__ == "__main__":
    main()