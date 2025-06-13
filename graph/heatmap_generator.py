import sys
import sqlite3
import json
from datetime import datetime, timedelta
from typing import Dict, Optional
import argparse # 导入 argparse 用于更好地处理命令行参数

# 导入 Matplotlib 和 NumPy
import matplotlib.pyplot as plt
import matplotlib.colors as mcolors
import numpy as np
import matplotlib.patches as patches # 导入 patches 模块用于绘制形状

class StudyDataReader:
    """
    从SQLite数据库中读取指定年份的学习时间数据。
    """
    def __init__(self, conn: sqlite3.Connection):
        self.conn = conn

    def fetch_yearly_study_times(self, year: int) -> Dict[str, int]:
        """
        从数据库中获取指定年份的每日总学习时间。
        """
        cursor = self.conn.cursor()
        start_date_str = f"{year}0101"
        end_date_str = f"{year}1231"
        cursor.execute('''
            SELECT date, SUM(duration)
            FROM time_records
            WHERE date BETWEEN ? AND ?
            AND (project_path = 'study' OR project_path LIKE 'study_%')
            GROUP BY date
        ''', (start_date_str, end_date_str))
        return dict(cursor.fetchall())

class HeatmapGenerator:
    """
    根据时间跟踪数据为指定年份生成学习热力图 (支持SVG和Matplotlib两种格式)。
    """
    # 修改 __init__ 方法以接受可选的调色板名称
    def __init__(self, study_times: Dict[str, int], year: int, config: dict, palette_name: Optional[str] = None):
        self.year = year
        self.config = config
        self.study_times = study_times
        
        self.color_palettes = config['COLOR_PALETTES']
        self.single_colors = config.get('SINGLE_COLORS', {})
        
        # --- 代码修改部分 ---
        # 根据输入或配置文件的默认值来决定使用哪个调色板
        chosen_palette_name = palette_name
        
        # 如果提供了调色板名称，检查其是否有效
        if chosen_palette_name and chosen_palette_name not in self.color_palettes:
            print(f"警告: 在配置中未找到调色板 '{chosen_palette_name}'。")
            print(f"可用的调色板有: {', '.join(self.color_palettes.keys())}")
            chosen_palette_name = None # 重置以使用默认值

        # 如果没有选择有效的调色板，则使用配置文件中的默认值
        if not chosen_palette_name:
            chosen_palette_name = config.get('DEFAULT_COLOR_PALETTE_NAME', 'GITHUB_GREEN_LIGHT')
            print(f"正在使用默认调色板: '{chosen_palette_name}'")
        else:
            print(f"正在使用指定的调色板: '{chosen_palette_name}'")
            
        # 设置最终的调色板
        self.default_color_palette = self.color_palettes[chosen_palette_name]
        # --- 修改结束 ---
        
        over_12_hours_color_ref = config.get('OVER_12_HOURS_COLOR_REF')
        if over_12_hours_color_ref and over_12_hours_color_ref in self.single_colors:
            self.over_12_hours_color = self.single_colors[over_12_hours_color_ref]
        else:
            print(f"警告: 颜色引用 '{over_12_hours_color_ref}' 未找到。将使用默认的橙色。")
            self.over_12_hours_color = "#f97148"

        self.heatmap_data = []
        self.svg_params = {}
        self.html_content = ""
        self.container_html = ""
        self.style_css = ""
        
        # 准备数据，供两种生成方法使用
        self._prepare_heatmap_layout_data()

    @staticmethod
    def _time_format_duration(seconds: int, avg_days: int = 1) -> str:
        if seconds is None:
            seconds = 0
        total_hours = int(seconds // 3600)
        total_minutes = int((seconds % 3600) // 60)
        time_str = f"{total_hours}h{total_minutes:02d}" if total_hours > 0 else f"{total_minutes}m"
        if total_hours == 0 and total_minutes == 0:
            time_str = "0m"
        if avg_days > 1:
            avg_seconds_per_day = seconds / avg_days
            avg_hours = int(avg_seconds_per_day // 3600)
            avg_minutes = int((avg_seconds_per_day % 3600) // 60)
            avg_str = f"{avg_hours}h{avg_minutes:02d}m"
            return f"{time_str} ({avg_str}/day)"
        return time_str

    def _get_color_for_study_time(self, study_time_seconds: int) -> str:
        hours = study_time_seconds / 3600
        if hours == 0:
            return self.default_color_palette[0]
        elif hours < 4:
            return self.default_color_palette[1]
        elif hours < 8:
            return self.default_color_palette[2]
        elif hours < 10:
            return self.default_color_palette[3]
        elif hours < 12:
            return self.default_color_palette[4]
        else:
            return self.over_12_hours_color

    def _prepare_heatmap_layout_data(self):
        start_date_obj = datetime(self.year, 1, 1)
        end_date_obj = datetime(self.year, 12, 31)
        front_empty_days = start_date_obj.isoweekday() % 7
        total_days_in_year = (end_date_obj - start_date_obj).days + 1
        total_slots = front_empty_days + total_days_in_year
        back_empty_days = (7 - (total_slots % 7)) % 7
        
        self.heatmap_data = [(None, 'empty', 0)] * front_empty_days
        current_date = start_date_obj
        while current_date <= end_date_obj:
            date_str_yyyymmdd = current_date.strftime("%Y%m%d")
            study_time_seconds = self.study_times.get(date_str_yyyymmdd, 0)
            color = self._get_color_for_study_time(study_time_seconds)
            self.heatmap_data.append((current_date, color, study_time_seconds))
            current_date += timedelta(days=1)
        self.heatmap_data.extend([(None, 'empty', 0)] * back_empty_days)

    def _calculate_svg_dimensions(self):
        if not self.heatmap_data: self._prepare_heatmap_layout_data()
        self.svg_params.update({
            'cell_size': 12, 'spacing': 3, 'weeks': len(self.heatmap_data) // 7,
            'rows': 7, 'margin_top': 30, 'margin_left': 35
        })
        self.svg_params['width'] = (self.svg_params['margin_left'] + self.svg_params['weeks'] * (self.svg_params['cell_size'] + self.svg_params['spacing']) - self.svg_params['spacing'])
        self.svg_params['height'] = (self.svg_params['margin_top'] + self.svg_params['rows'] * (self.svg_params['cell_size'] + self.svg_params['spacing']) - self.svg_params['spacing'])

    def _generate_svg_header(self) -> str:
        return f'<svg xmlns="http://www.w3.org/2000/svg" width="{self.svg_params["width"]}" height="{self.svg_params["height"]}" style="font-family: Arial, sans-serif;">'

    def _generate_day_labels_svg(self) -> list:
        svg_elements, day_labels_display = [], {1: 'Mon', 3: 'Wed', 5: 'Fri'}
        for i in range(self.svg_params['rows']):
            if i in day_labels_display:
                y_pos = self.svg_params['margin_top'] + i * (self.svg_params['cell_size'] + self.svg_params['spacing']) + self.svg_params['cell_size'] - 2
                svg_elements.append(f'<text x="0" y="{y_pos}" font-size="10px" fill="#767676" alignment-baseline="middle">{day_labels_display[i]}</text>')
        return svg_elements

    def _generate_month_labels_svg(self) -> list:
        svg_elements, month_names, last_month_drawn = [], ["Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"], -1
        for week_idx in range(self.svg_params['weeks']):
            first_date_in_week = next((self.heatmap_data[week_idx * 7 + i][0] for i in range(7) if self.heatmap_data[week_idx * 7 + i][0]), None)
            if first_date_in_week:
                current_month_idx = first_date_in_week.month - 1
                if current_month_idx != last_month_drawn and (first_date_in_week.day < 8 or week_idx == 0):
                    x_pos = self.svg_params['margin_left'] + week_idx * (self.svg_params['cell_size'] + self.svg_params['spacing'])
                    svg_elements.append(f'<text x="{x_pos}" y="{self.svg_params["margin_top"] - 10}" font-size="10px" fill="#767676">{month_names[current_month_idx]}</text>')
                    last_month_drawn = current_month_idx
        return svg_elements

    def _generate_data_cells_svg(self) -> list:
        svg_elements = []
        for i, (date_obj, color, study_time_seconds) in enumerate(self.heatmap_data):
            if date_obj is not None:
                col_idx, row_idx = divmod(i, self.svg_params['rows'])
                x_pos = self.svg_params['margin_left'] + col_idx * (self.svg_params['cell_size'] + self.svg_params['spacing'])
                y_pos = self.svg_params['margin_top'] + row_idx * (self.svg_params['cell_size'] + self.svg_params['spacing'])
                duration_str = self._time_format_duration(study_time_seconds)
                title_text = f"{date_obj.strftime('%Y-%m-%d')}: {duration_str}"
                svg_elements.extend([
                    f'  <rect width="{self.svg_params["cell_size"]}" height="{self.svg_params["cell_size"]}" x="{x_pos}" y="{y_pos}" fill="{color}" rx="2" ry="2">',
                    f'    <title>{title_text}</title>',
                    f'  </rect>'
                ])
        return svg_elements

    def generate_html_output(self, output_filename: str):
        self._calculate_svg_dimensions()
        full_svg_content = '\n'.join([
            self._generate_svg_header(), *self._generate_day_labels_svg(),
            *self._generate_month_labels_svg(), *self._generate_data_cells_svg(), '</svg>'
        ])
        self.style_css = f"""
        body {{ font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Helvetica, Arial, sans-serif, "Apple Color Emoji", "Segoe UI Emoji"; }}
        .heatmap-container {{ display: inline-block; padding: 15px; border: 1px solid #d0d7de; border-radius: 6px; background-color: #ffffff; }}
        h2 {{ margin-left: {self.svg_params.get('margin_left', 35)}px; font-weight: 400; color: #24292f;}}"""
        self.container_html = f"""<div class="heatmap-container"><h2>Study Activity for {self.year}</h2>{full_svg_content}</div>"""
        self.html_content = f"""<!DOCTYPE html><html><head><meta charset="UTF-8"><title>Study Heatmap {self.year}</title><style>{self.style_css}</style></head><body>{self.container_html}</body></html>"""
        with open(output_filename, 'w', encoding='utf-8') as f:
            f.write(self.html_content)

    def generate_mpl_heatmap(self, output_filename: str):
        """
        使用 Matplotlib 生成一个 GitHub 风格的热力图并保存为 PNG 或 SVG 文件。
        """
        weeks = len(self.heatmap_data) // 7
        
        colors = self.default_color_palette + [self.over_12_hours_color]
        bounds = [0, 0.01, 4, 8, 10, 12, 24] # 0.01 用于区分 0 和 极小值
        cmap = mcolors.ListedColormap(colors)
        norm = mcolors.BoundaryNorm(bounds, cmap.N)
        
        fig, ax = plt.subplots(figsize=(weeks * 0.3, 7 * 0.3), dpi=600)
        fig.patch.set_facecolor('white')

        for week_idx in range(weeks):
            for day_idx in range(7):
                date_info_index = week_idx * 7 + day_idx
                if date_info_index < len(self.heatmap_data):
                    date_obj, _, study_seconds = self.heatmap_data[date_info_index]
                    if date_obj is not None:
                        hour_val = study_seconds / 3600.0
                        color = cmap(norm(hour_val))
                        rect = patches.FancyBboxPatch(
                            (week_idx + 0.05, 6 - day_idx + 0.05), # Y轴反转
                            0.9, 0.9,
                            boxstyle="round,pad=0,rounding_size=0.1",
                            facecolor=color,
                            edgecolor='none',
                            linewidth=0
                        )
                        ax.add_patch(rect)
        
        ax.set_xlim(0, weeks)
        ax.set_ylim(0, 7)
        ax.invert_yaxis()
        ax.set_aspect('equal')

        ax.set_yticks(np.arange(7) + 0.5)
        ax.set_yticklabels(['Sun', 'Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat'], fontsize=8)
        
        month_labels = ["Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"]
        month_ticks = []
        month_tick_labels = []
        last_month = -1
        for week_idx in range(weeks):
            first_date_in_week = next((self.heatmap_data[week_idx * 7 + i][0] for i in range(7) if self.heatmap_data[week_idx * 7 + i][0]), None)
            if first_date_in_week:
                current_month = first_date_in_week.month
                if current_month != last_month:
                    if week_idx > 0 or first_date_in_week.day == 1:
                         month_ticks.append(week_idx)
                         month_tick_labels.append(month_labels[current_month - 1])
                         last_month = current_month

        ax.set_xticks(np.array(month_ticks) + 0.5)
        ax.set_xticklabels(month_tick_labels, fontsize=8, ha='center')
        
        ax.set_title(f'Study Activity for {self.year}', loc='left', fontsize=12, pad=20)
        ax.tick_params(axis='both', which='both', length=0)
        for spine in ax.spines.values():
            spine.set_visible(False)
        
        plt.tight_layout(pad=1.5)
        plt.savefig(output_filename, bbox_inches='tight')
        plt.close()


def load_config(config_path: str = 'heatmap_colors_config.json') -> dict: # 加载颜色配置文件
    try:
        with open(config_path, 'r', encoding='utf-8') as f:
            return json.load(f)
    except FileNotFoundError:
        print(f"错误: 配置文件 '{config_path}' 未找到。")
        sys.exit(1)
    except json.JSONDecodeError:
        print(f"错误: 无法解析 '{config_path}' 中的 JSON。请检查文件格式。")
        sys.exit(1)

if __name__ == "__main__":
    # --- 代码修改部分 ---
    # 使用 argparse 来处理命令行参数
    parser = argparse.ArgumentParser(description='为指定年份生成学习热力图。')
    parser.add_argument('year', type=int, help='需要生成热力图的年份。')
    parser.add_argument('--palette', type=str, help='要使用的调色板名称(需在配置文件中定义)。')
    
    args = parser.parse_args()
    
    year_arg = args.year
    palette_arg = args.palette
    # --- 修改结束 ---
    
    config_data = load_config()
    db_path = 'time_data.db'
    db_conn = None
    try:
        db_conn = sqlite3.connect(db_path)
        
        data_reader = StudyDataReader(db_conn)
        study_data = data_reader.fetch_yearly_study_times(year_arg)
        
        # 将可选的调色板名称传递给生成器
        heatmap_generator = HeatmapGenerator(study_data, year_arg, config_data, palette_name=palette_arg)
        
        # 1. 生成 HTML/SVG 输出文件
        output_html_file = f"study_heatmap_{year_arg}.html"
        heatmap_generator.generate_html_output(output_html_file)
        print(f"HTML 热力图生成成功: {output_html_file}")
        
        # 2. 使用 Matplotlib 生成 PNG
        output_image_file = f"study_heatmap_{year_arg}_mpl.png"
        heatmap_generator.generate_mpl_heatmap(output_image_file)
        print(f"高分辨率 PNG 热力图生成成功: {output_image_file}")

        # 3. 使用 Matplotlib 生成 SVG
        output_svg_file = f"study_heatmap_{year_arg}_mpl.svg"
        heatmap_generator.generate_mpl_heatmap(output_svg_file)
        print(f"矢量 (SVG) 热力图生成成功: {output_svg_file}")

    except sqlite3.Error as e:
        print(f"数据库错误: {e}")
    except Exception as e:
        print(f"发生了意外错误: {e}")
    finally:
        if db_conn:
            db_conn.close()
