import sqlite3
import argparse
import datetime
import calendar
import os
import sys

# --- Configuration ---
# 数据库文件路径
DB_PATH = 'time_data.db'
# 热力图的五种颜色，从无到有
GITHUB_COLORS = ["#ebedf0", "#9be9a8", "#40c463", "#30a14e", "#216e39"]
# 数据库连接超时时间 (秒)
DB_CONNECTION_TIMEOUT = 10
# 递归查询的最大深度，防止无限循环
MAX_RECURSION_DEPTH = 15

def get_project_data_for_year(year: int, project_name: str) -> dict[datetime.date, float]:
    """
    根据年份和项目名称，查询数据库中每天的总时长。

    此函数使用递归查询来识别与指定父项目相关的所有子项目，
    然后汇总这些项目在指定年份中每天的总时长。

    Args:
        year: 要查询的四位数年份。
        project_name: 要查询的顶级父项目名称。

    Returns:
        一个字典，将日期对象映射到该日的总小时数。
    """
    print(f"🔍 [步骤 1/4] 开始为年份 {year} 检索项目 '{project_name}' 的数据...")
    
    # --- 文件存在性检查 ---
    if not os.path.exists(DB_PATH):
        print(f"❌ 错误: 在当前目录下未找到数据库文件 '{DB_PATH}'。", file=sys.stderr)
        sys.exit(1)
    
    print(f"  ✔️  数据库文件 '{DB_PATH}' 已找到。")

    # --- 带安全防护的SQL递归查询 ---
    # 查询现在是动态的，会根据传入的 project_name 进行
    sql_query = f"""
    WITH RECURSIVE target_projects(project, depth) AS (
      -- 基础情况: 将指定的项目作为递归起点，深度为1
      VALUES(?, 1) 
      UNION ALL
      -- 递归步骤: 查找已找到项目的所有子项目
      SELECT
        pc.child,
        tp.depth + 1
      FROM
        parent_child pc JOIN target_projects tp ON pc.parent = tp.project
      WHERE
        tp.depth < {MAX_RECURSION_DEPTH} -- 安全阀，防止无限递归
    )
    SELECT
      tr.date,
      SUM(tr.duration) AS total_duration_seconds
    FROM
      time_records tr
    WHERE
      tr.project_path IN (SELECT project FROM target_projects)
      AND SUBSTR(tr.date, 1, 4) = ? -- 按年份筛选
    GROUP BY
      tr.date;
    """

    project_data = {}
    try:
        print(f"  正在连接数据库 (超时时间: {DB_CONNECTION_TIMEOUT}s)...")
        with sqlite3.connect(DB_PATH, timeout=DB_CONNECTION_TIMEOUT) as conn:
            cursor = conn.cursor()
            print("  ✔️  数据库连接成功。")
            print(f"  正在执行SQL查询以查找所有 '{project_name}' 相关的记录...")
            
            # 执行查询，现在需要传递两个参数：project_name 和 year
            cursor.execute(sql_query, (project_name, str(year)))
            rows = cursor.fetchall()
            
            print(f"  ✔️  查询执行完毕。找到 {len(rows)} 天包含 '{project_name}' 的数据。")

            if not rows:
                print(f"  ⚠️  警告: 在 {year} 年未找到 '{project_name}' 的记录。生成的热力图将为空。")

            for row in rows:
                date_str, total_seconds = row
                if total_seconds is not None and total_seconds > 0:
                    current_date = datetime.datetime.strptime(date_str, '%Y%m%d').date()
                    hours = total_seconds / 3600.0
                    project_data[current_date] = hours

    except sqlite3.TimeoutError:
        print(f"❌ 错误: 在 {DB_CONNECTION_TIMEOUT} 秒内无法连接到数据库。它可能被其他程序锁定。", file=sys.stderr)
        sys.exit(1)
    except sqlite3.OperationalError as e:
        print(f"❌ SQL错误: 查询数据库时发生错误: {e}", file=sys.stderr)
        print("   请确保 'time_data.db' 是一个有效的SQLite数据库，并包含 'time_records' 和 'parent_child' 表。", file=sys.stderr)
        sys.exit(1)
    except ValueError as e:
        print(f"❌ 数据错误: 数据库中的日期格式不正确。 {e}", file=sys.stderr)
        sys.exit(1)
    
    print("✅ [步骤 1/4] 数据检索完成。")
    return project_data

def get_color_for_hours(hours: float) -> str:
    """根据小时数决定热力图的颜色。"""
    if hours <= 0:
        return GITHUB_COLORS[0]
    elif hours <= 4:
        return GITHUB_COLORS[1]
    elif hours <= 8:
        return GITHUB_COLORS[2]
    elif hours <= 10:
        return GITHUB_COLORS[3]
    else: # hours > 10
        return GITHUB_COLORS[4]

def generate_heatmap_html(year: int, project_name: str, data: dict[datetime.date, float]) -> str:
    """为热力图生成完整的HTML内容。"""
    print(f"🎨 [步骤 2/4] 正在为项目 '{project_name}' 生成SVG和HTML结构...")
    
    # --- SVG和网格配置 ---
    SQUARE_SIZE = 12
    SQUARE_GAP = 3
    SQUARE_RADIUS = 2 
    GRID_UNIT = SQUARE_SIZE + SQUARE_GAP
    LEFT_PADDING = 30
    TOP_PADDING = 30

    start_date = datetime.date(year, 1, 1)
    end_date = datetime.date(year, 12, 31)
    total_days = (end_date - start_date).days + 1
    
    first_day_weekday = (start_date.isoweekday()) % 7
    num_weeks = (total_days + first_day_weekday + 6) // 7

    svg_width = num_weeks * GRID_UNIT + LEFT_PADDING
    svg_height = 7 * GRID_UNIT + TOP_PADDING

    rects_html = []
    month_labels_html = []
    last_month = -1

    for day_index in range(total_days):
        current_date = start_date + datetime.timedelta(days=day_index)
        grid_day_index = day_index + first_day_weekday
        week_x = grid_day_index // 7
        day_y = grid_day_index % 7

        x_pos = week_x * GRID_UNIT + LEFT_PADDING
        y_pos = day_y * GRID_UNIT + TOP_PADDING
        
        hours = data.get(current_date, 0)
        color = get_color_for_hours(hours)
        
        # 更新悬浮提示框的文本
        tooltip = f"{hours:.2f} 小时的 {project_name} on {current_date.strftime('%Y-%m-%d')}"
        rects_html.append(
            f'    <rect width="{SQUARE_SIZE}" height="{SQUARE_SIZE}" x="{x_pos}" y="{y_pos}" '
            f'fill="{color}" rx="{SQUARE_RADIUS}" ry="{SQUARE_RADIUS}">'
            f'<title>{tooltip}</title></rect>'
        )

        if current_date.month != last_month and current_date.weekday() < 3:
            month_name = current_date.strftime("%b")
            month_labels_html.append(
                f'  <text x="{x_pos}" y="{TOP_PADDING - 10}" class="month">{month_name}</text>'
            )
            last_month = current_date.month

    day_labels_html = [
        f'<text x="{LEFT_PADDING - 10}" y="{TOP_PADDING + GRID_UNIT * 1 + SQUARE_SIZE / 1.5}" class="day-label">M</text>',
        f'<text x="{LEFT_PADDING - 10}" y="{TOP_PADDING + GRID_UNIT * 3 + SQUARE_SIZE / 1.5}" class="day-label">W</text>',
        f'<text x="{LEFT_PADDING - 10}" y="{TOP_PADDING + GRID_UNIT * 5 + SQUARE_SIZE / 1.5}" class="day-label">F</text>'
    ]
    
    # 将项目名称首字母大写用于显示
    display_project_name = project_name.capitalize()
    
    # 更新HTML模板中的标题
    html_template = f"""
<!DOCTYPE html>
<html lang="zh-CN">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>{display_project_name} 热力图 - {year}</title>
    <style>
        body {{
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Helvetica, Arial, sans-serif, "Apple Color Emoji", "Segoe UI Emoji";
            background-color: #f6f8fa;
            color: #24292e;
            display: flex;
            justify-content: center;
            align-items: center;
            flex-direction: column;
            margin: 2em;
        }}
        .heatmap-container {{
            border: 1px solid #e1e4e8;
            border-radius: 6px;
            padding: 20px;
            background-color: #ffffff;
            max-width: 100%;
            overflow-x: auto;
        }}
        svg {{ display: block; margin: 0 auto; }}
        .month, .day-label {{ font-size: 10px; fill: #586069; text-anchor: middle; }}
        .day-label {{ text-anchor: end; }}
        h1 {{ font-weight: 400; text-align: center; }}
        rect:hover {{ stroke: #24292e; stroke-width: 1px; }}
    </style>
</head>
<body>
    <h1>{display_project_name} 热力图 - {year}</h1>
    <div class="heatmap-container">
        <svg width="{svg_width}" height="{svg_height}">
            {"\n".join(month_labels_html)}
            {"\n".join(day_labels_html)}
            {"\n".join(rects_html)}
        </svg>
    </div>
</body>
</html>
"""
    print("✅ [步骤 2/4] HTML生成完成。")
    return html_template

def main():
    """主函数，用于解析参数并生成热力图。"""
    parser = argparse.ArgumentParser(
        description="从 time_data.db 数据库为指定项目生成一个GitHub风格的热力图。"
    )
    parser.add_argument(
        "year",
        type=int,
        help="要为其生成热力图的年份 (例如: 2024)。"
    )
    # 新增的可选参数，用于指定项目名称
    parser.add_argument(
        "-p", "--project",
        type=str,
        default="study",
        help='要生成热力图的父项目 (例如 "code")。默认为 "study"。'
    )
    args = parser.parse_args()
    year = args.year
    # 将项目名称转为小写以保持一致性
    project_name = args.project.lower()

    try:
        # 1. 从数据库获取指定项目的数据
        project_data = get_project_data_for_year(year, project_name)

        # 2. 生成HTML内容
        html_content = generate_heatmap_html(year, project_name, project_data)

        # 3. 将HTML写入文件，文件名包含项目名
        output_filename = f"heatmap_{project_name}_{year}.html"
        print(f"📄 [步骤 3/4] 正在将HTML写入文件 '{output_filename}'...")
        with open(output_filename, "w", encoding="utf-8") as f:
            f.write(html_content)
        
        print(f"✅ [步骤 3/4] 文件写入完成。")
        print(f"\n🎉 全部完成！已成功生成热力图: {output_filename}")

    except Exception as e:
        print(f"\n❌ 主进程中发生意外错误: {e}", file=sys.stderr)
        sys.exit(1)

if __name__ == "__main__":
    print("🚀 启动热力图生成器...")
    main()
