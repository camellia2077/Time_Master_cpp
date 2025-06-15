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
        This version uses the parent_child table to identify all activities
        that fall under the "study" hierarchy.
        """
        cursor = self.conn.cursor()
        start_date_str = f"{year}0101"
        end_date_str = f"{year}1231"

        # This CTE (Common Table Expression) identifies all project identifiers
        # (in their underscore-separated form as found in parent_child.child or parent_child.parent)
        # that are considered part of the "study" hierarchy.
        # It starts with 'study' itself and any direct children of 'STUDY' (the canonical parent),
        # then recursively finds all their descendants.
        sql_query = """
            WITH RECURSIVE study_project_identifiers (id) AS (
                -- Anchor members:
                -- 1. 'study' itself (this is the primary project name we assume for study).
                SELECT 'study'
                UNION
                -- 2. Direct children of 'STUDY' (the canonical top-level parent defined in C++ parser).
                --    This catches projects that might be top-level study activities but not named 'study'
                --    e.g., parent_child might have ('deep_learning', 'STUDY').
                SELECT child FROM parent_child WHERE parent = 'STUDY'

                UNION ALL

                -- Recursive members:
                -- Find children of already identified study-related project identifiers.
                -- pc.parent would be an 'id' from a previous iteration (e.g., 'study', 'deep_learning').
                SELECT pc.child
                FROM parent_child pc
                INNER JOIN study_project_identifiers spi ON pc.parent = spi.id
            )
            SELECT
                tr.date,
                SUM(tr.duration) as total_duration
            FROM
                time_records tr
            WHERE
                tr.date BETWEEN ? AND ?
                -- The project_path from time_records, when its spaces are replaced by underscores
                -- (to match the format in parent_child table), must be one of the identifiers
                -- found in our study hierarchy.
                AND REPLACE(tr.project_path, ' ', '_') IN (SELECT DISTINCT id FROM study_project_identifiers)
            GROUP BY
                tr.date;
        """
        
        cursor.execute(sql_query, (start_date_str, end_date_str))
        return dict(cursor.fetchall())

class HeatmapGenerator:
    """
    根据时间跟踪数据为指定年份生成学习热力图 (支持SVG和Matplotlib两种格式)。
    """
    def __init__(self, study_times: Dict[str, int], year: int, config: dict, palette_name: Optional[str] = None):
        self.year = year
        self.config = config
        self.study_times = study_times
        
        self.color_palettes = config['COLOR_PALETTES']
        self.single_colors = config.get('SINGLE_COLORS', {})
        
        chosen_palette_name = palette_name
        
        if chosen_palette_name and chosen_palette_name not in self.color_palettes:
            print(f"警告: 在配置中未找到调色板 '{chosen_palette_name}'。")
            print(f"可用的调色板有: {', '.join(self.color_palettes.keys())}")
            chosen_palette_name = None 

        if not chosen_palette_name:
            chosen_palette_name = config.get('DEFAULT_COLOR_PALETTE_NAME', 'GITHUB_GREEN_LIGHT')
            print(f"正在使用默认调色板: '{chosen_palette_name}'")
        else:
            print(f"正在使用指定的调色板: '{chosen_palette_name}'")
            
        self.default_color_palette = self.color_palettes[chosen_palette_name]
        
        over_12_hours_color_ref = config.get('OVER_12_HOURS_COLOR_REF')
        if over_12_hours_color_ref and over_12_hours_color_ref in self.single_colors:
            self.over_12_hours_color = self.single_colors[over_12_hours_color_ref]
        else:
            print(f"警告: 颜色引用 '{over_12_hours_color_ref}' 未找到。将使用默认的橙色。")
            self.over_12_hours_color = "#f97148" # Default orange

        self.heatmap_data = []
        self.svg_params = {}
        self.html_content = ""
        self.container_html = ""
        self.style_css = ""
        self.front_empty_days = 0 # Initialize attribute
        
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
        
        # Correct logic for `front_empty_days` if the week starts on Monday (index 0 for heatmap_data):
        self.front_empty_days = start_date_obj.weekday() # Monday is 0, Sunday is 6

        total_days_in_year = (end_date_obj - start_date_obj).days + 1
        total_slots = self.front_empty_days + total_days_in_year
        back_empty_days = (7 - (total_slots % 7)) % 7
        
        self.heatmap_data = [(None, 'empty', 0)] * self.front_empty_days
        current_date = start_date_obj
        while current_date <= end_date_obj:
            date_str_yyyymmdd = current_date.strftime("%Y%m%d")
            study_time_seconds = self.study_times.get(date_str_yyyymmdd, 0)
            color = self._get_color_for_study_time(study_time_seconds)
            self.heatmap_data.append((current_date, color, study_time_seconds))
            current_date += timedelta(days=1)
        self.heatmap_data.extend([(None, 'empty', 0)] * back_empty_days)

    def _calculate_svg_dimensions(self):
        if not self.heatmap_data: self._prepare_heatmap_layout_data() # Should already be called by __init__
        self.svg_params.update({
            'cell_size': 12, 'spacing': 3, 'weeks': len(self.heatmap_data) // 7,
            'rows': 7, 'margin_top': 30, 'margin_left': 35, 'legend_height': 20
        })
        self.svg_params['width'] = (self.svg_params['margin_left'] + self.svg_params['weeks'] * (self.svg_params['cell_size'] + self.svg_params['spacing']) - self.svg_params['spacing'])
        self.svg_params['height'] = (self.svg_params['margin_top'] + self.svg_params['rows'] * (self.svg_params['cell_size'] + self.svg_params['spacing']) - self.svg_params['spacing']) + self.svg_params['legend_height']

    def _generate_svg_header(self) -> str:
        return f'<svg xmlns="http://www.w3.org/2000/svg" width="{self.svg_params["width"]}" height="{self.svg_params["height"]}" style="font-family: Arial, sans-serif;">'

    def _generate_day_labels_svg(self) -> list:
        svg_elements, day_labels_display = [], {0: 'Mon', 2: 'Wed', 4: 'Fri'} 
        for i in range(self.svg_params['rows']): 
            if i in day_labels_display:
                y_pos = self.svg_params['margin_top'] + i * (self.svg_params['cell_size'] + self.svg_params['spacing']) + self.svg_params['cell_size'] / 2
                svg_elements.append(f'<text x="0" y="{y_pos}" font-size="10px" fill="#767676" alignment-baseline="middle">{day_labels_display[i]}</text>')
        return svg_elements

    def _generate_month_labels_svg(self) -> list:
        svg_elements, month_names, last_month_drawn_idx = [], ["Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"], -1
        
        for week_idx in range(self.svg_params['weeks']):
            first_actual_date_in_col = None
            for day_offset_in_col in range(7): # 0=Mon, ..., 6=Sun
                idx_in_heatmap = week_idx * 7 + day_offset_in_col
                if idx_in_heatmap < len(self.heatmap_data) and self.heatmap_data[idx_in_heatmap][0] is not None:
                    first_actual_date_in_col = self.heatmap_data[idx_in_heatmap][0]
                    break
            
            if not first_actual_date_in_col:
                continue 

            current_month_idx = first_actual_date_in_col.month - 1
            
            # Draw label if it's a new month and the first day of this month is in this column,
            # or if it's the first column with actual dates.
            should_draw = False
            if current_month_idx != last_month_drawn_idx:
                contains_first_of_this_month = False
                for day_offset_in_col_check in range(7):
                    idx_check = week_idx * 7 + day_offset_in_col_check
                    if idx_check < len(self.heatmap_data):
                        date_val = self.heatmap_data[idx_check][0]
                        if date_val and date_val.day == 1 and date_val.month -1 == current_month_idx:
                            contains_first_of_this_month = True
                            break
                
                # Check if it's the first column containing any dates
                is_first_populated_column = True
                if week_idx > 0:
                    is_first_populated_column = all(self.heatmap_data[i][0] is None for i in range(week_idx * 7))


                if contains_first_of_this_month or is_first_populated_column:
                    should_draw = True

            if should_draw:
                x_pos = self.svg_params['margin_left'] + week_idx * (self.svg_params['cell_size'] + self.svg_params['spacing'])
                svg_elements.append(f'<text x="{x_pos}" y="{self.svg_params["margin_top"] - 10}" font-size="10px" fill="#767676">{month_names[current_month_idx]}</text>')
                last_month_drawn_idx = current_month_idx
        return svg_elements

    def _generate_data_cells_svg(self) -> list:
        svg_elements = []
        for i, (date_obj, color_val, study_time_seconds) in enumerate(self.heatmap_data):
            col_idx = i // self.svg_params['rows'] 
            row_idx = i % self.svg_params['rows']  

            if color_val == 'empty': 
                final_color = self.config.get('EMPTY_CELL_COLOR', '#ebedf0') 
            else:
                final_color = color_val

            x_pos = self.svg_params['margin_left'] + col_idx * (self.svg_params['cell_size'] + self.svg_params['spacing'])
            y_pos = self.svg_params['margin_top'] + row_idx * (self.svg_params['cell_size'] + self.svg_params['spacing'])
            
            rect_str = f'  <rect width="{self.svg_params["cell_size"]}" height="{self.svg_params["cell_size"]}" x="{x_pos}" y="{y_pos}" fill="{final_color}" rx="2" ry="2">'
            
            if date_obj is not None: 
                duration_str = self._time_format_duration(study_time_seconds)
                title_text = f"{date_obj.strftime('%Y-%m-%d')} ({date_obj.strftime('%a')}): {duration_str}"
                svg_elements.extend([rect_str, f'    <title>{title_text}</title>', '  </rect>'])
            else: 
                svg_elements.extend([rect_str, '  </rect>'])
        return svg_elements
    
    def _generate_legend_svg(self) -> str:
        legend_items = []
        legend_x_start = self.svg_params['margin_left']
        # Position legend at the bottom, using the pre-calculated height
        legend_y_start = self.svg_params['margin_top'] + self.svg_params['rows'] * (self.svg_params['cell_size'] + self.svg_params['spacing']) + 5


        legend_items.append(f'<text x="{legend_x_start - 35}" y="{legend_y_start + self.svg_params["cell_size"] / 2}" font-size="9px" fill="#57606a" alignment-baseline="middle" text-anchor="start">Less</text>')
        
        palette_for_legend = self.default_color_palette[:5] 
        
        current_x = legend_x_start
        for i, color in enumerate(palette_for_legend):
            legend_items.append(f'<rect width="{self.svg_params["cell_size"]}" height="{self.svg_params["cell_size"]}" x="{current_x}" y="{legend_y_start}" fill="{color}" rx="2" ry="2"/>')
            current_x += self.svg_params['cell_size'] + self.svg_params['spacing']
        
        legend_items.append(f'<text x="{current_x + 5}" y="{legend_y_start + self.svg_params["cell_size"] / 2}" font-size="9px" fill="#57606a" alignment-baseline="middle" text-anchor="start">More</text>')
        
        return "\n".join(legend_items)


    def generate_html_output(self, output_filename: str):
        self._calculate_svg_dimensions() 
        full_svg_content = '\n'.join([
            self._generate_svg_header(),
            *self._generate_day_labels_svg(),
            *self._generate_month_labels_svg(),
            *self._generate_data_cells_svg(),
            self._generate_legend_svg(), 
            '</svg>'
        ])
        self.style_css = f"""
        body {{ font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Helvetica, Arial, sans-serif, "Apple Color Emoji", "Segoe UI Emoji"; margin: 20px; background-color: #f6f8fa; }}
        .heatmap-container {{ display: inline-block; padding: 20px; border: 1px solid #d0d7de; border-radius: 6px; background-color: #ffffff; box-shadow: 0 1px 3px rgba(0,0,0,0.1); }}
        h2 {{ margin-left: 0px; font-weight: 600; font-size: 16px; color: #24292f; margin-bottom:15px; text-align: left;}}"""
        self.container_html = f"""<div class="heatmap-container"><h2>Study Activity for {self.year}</h2>{full_svg_content}</div>"""
        self.html_content = f"""<!DOCTYPE html><html lang="en"><head><meta charset="UTF-8"><title>Study Heatmap {self.year}</title><style>{self.style_css}</style></head><body>{self.container_html}</body></html>"""
        with open(output_filename, 'w', encoding='utf-8') as f:
            f.write(self.html_content)

    def generate_mpl_heatmap(self, output_filename: str):
        weeks = len(self.heatmap_data) // 7
        if weeks == 0:
            print(f"No data to plot for Matplotlib heatmap for year {self.year}.")
            return

        colors_palette = self.default_color_palette + [self.over_12_hours_color]
        bounds = [0, 0.01, 4, 8, 10, 12, 100] 
        cmap = mcolors.ListedColormap(colors_palette)
        norm = mcolors.BoundaryNorm(bounds, cmap.N)
        
        fig, ax = plt.subplots(figsize=(weeks * 0.25, 7 * 0.25), dpi=300)
        fig.patch.set_facecolor(self.config.get('MPL_BACKGROUND_COLOR', 'white'))

        for week_idx in range(weeks):
            for day_idx_mon_sun in range(7): 
                data_idx = week_idx * 7 + day_idx_mon_sun
                if data_idx < len(self.heatmap_data):
                    date_obj, _, study_seconds = self.heatmap_data[data_idx]
                    if date_obj is not None:
                        hour_val = study_seconds / 3600.0
                        color_val = cmap(norm(hour_val))
                        
                        mpl_y_coord = (day_idx_mon_sun + 1) % 7 
                        
                        rect = patches.FancyBboxPatch(
                            (week_idx + 0.05, mpl_y_coord + 0.05), 
                            0.9, 0.9, 
                            boxstyle="round,pad=0,rounding_size=0.1",
                            facecolor=color_val,
                            edgecolor='none'
                        )
                        ax.add_patch(rect)
        
        ax.set_xlim(0, weeks)
        ax.set_ylim(0, 7) 
        ax.invert_yaxis() 

        ax.set_yticks(np.arange(7) + 0.5) 
        ax.set_yticklabels(['Sun', 'Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat'], fontsize=7)
        
        month_labels_mpl = ["Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"]
        month_ticks_mpl = []
        month_tick_labels_mpl = []
        last_month_idx_mpl_drawn = -1

        for week_idx in range(weeks):
            first_actual_date_in_col = None
            for day_offset_in_col in range(7):
                idx_in_heatmap = week_idx * 7 + day_offset_in_col
                if idx_in_heatmap < len(self.heatmap_data) and self.heatmap_data[idx_in_heatmap][0] is not None:
                    first_actual_date_in_col = self.heatmap_data[idx_in_heatmap][0]
                    break
            
            if not first_actual_date_in_col:
                continue

            current_month_idx = first_actual_date_in_col.month - 1
            
            should_draw_mpl_month = False
            if current_month_idx != last_month_idx_mpl_drawn:
                contains_first_of_this_month = False
                for day_offset_in_col_check in range(7):
                    idx_check = week_idx * 7 + day_offset_in_col_check
                    if idx_check < len(self.heatmap_data):
                        date_val = self.heatmap_data[idx_check][0]
                        if date_val and date_val.day == 1 and date_val.month -1 == current_month_idx:
                            contains_first_of_this_month = True
                            break
                
                is_first_populated_column_mpl = True
                if week_idx > 0:
                     is_first_populated_column_mpl = all(self.heatmap_data[i][0] is None for i in range(week_idx*7))


                if contains_first_of_this_month or is_first_populated_column_mpl:
                    should_draw_mpl_month = True

            if should_draw_mpl_month:
                month_ticks_mpl.append(week_idx)
                month_tick_labels_mpl.append(month_labels_mpl[current_month_idx])
                last_month_idx_mpl_drawn = current_month_idx
        
        ax.set_xticks(np.array(month_ticks_mpl) + 0.5) 
        ax.set_xticklabels(month_tick_labels_mpl, fontsize=7, ha='center')

        ax.set_title(f'Study Activity for {self.year}', loc='left', fontsize=10, pad=15)
        ax.tick_params(axis='both', which='both', length=0)
        for spine_pos in ['top', 'right', 'bottom', 'left']:
            ax.spines[spine_pos].set_visible(False)

        plt.tight_layout(pad=1.0)
        plt.savefig(output_filename, bbox_inches='tight')
        plt.close(fig)


def load_config(config_path: str = 'heatmap_colors_config.json') -> dict:
    default_config = {
        "COLOR_PALETTES": {
            "GITHUB_GREEN_LIGHT": ["#ebedf0", "#9be9a8", "#40c463", "#30a14e", "#216e39"],
        },
        "SINGLE_COLORS": {
            "ORANGE_OVER_12H": "#f97148"
        },
        "DEFAULT_COLOR_PALETTE_NAME": "GITHUB_GREEN_LIGHT",
        "OVER_12_HOURS_COLOR_REF": "ORANGE_OVER_12H",
        "EMPTY_CELL_COLOR": "#ebedf0",
        "MPL_BACKGROUND_COLOR": "#ffffff"
    }
    try:
        with open(config_path, 'r', encoding='utf-8') as f:
            user_config = json.load(f)
            for key, value in user_config.items():
                if key in default_config and isinstance(default_config[key], dict):
                    default_config[key].update(value)
                else:
                    default_config[key] = value
            return default_config
    except FileNotFoundError:
        print(f"配置文件 '{config_path}' 未找到。将使用内部默认配置。")
        return default_config
    except json.JSONDecodeError:
        print(f"错误: 无法解析 '{config_path}' 中的 JSON。请检查文件格式。将使用内部默认配置。")
        return default_config


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='为指定年份生成学习热力图。')
    parser.add_argument('year', type=int, help='需要生成热力图的年份。')
    parser.add_argument('--palette', type=str, help='要使用的调色板名称(需在配置文件中定义)。')
    parser.add_argument('--db_path', type=str, default='time_data.db', help='SQLite数据库文件的路径 (默认为 time_data.db)。')
    parser.add_argument('--config_path', type=str, default='heatmap_colors_config.json', help='颜色配置文件的路径 (默认为 heatmap_colors_config.json)。')
    
    args = parser.parse_args()
    
    year_arg = args.year
    palette_arg = args.palette
    db_path_arg = args.db_path
    config_path_arg = args.config_path
    
    config_data = load_config(config_path_arg)
    db_conn = None
    try:
        db_conn = sqlite3.connect(db_path_arg)
        
        data_reader = StudyDataReader(db_conn)
        study_data = data_reader.fetch_yearly_study_times(year_arg)
        
        if not study_data and not any(1 for yr_data in study_data.values() if yr_data > 0) : # Check if study_data is empty or all zero durations
            print(f"未找到 {year_arg} 年的学习数据，或所有学习时长为零。")
        
        heatmap_generator = HeatmapGenerator(study_data, year_arg, config_data, palette_name=palette_arg)
        
        output_html_file = f"study_heatmap_{year_arg}.html"
        heatmap_generator.generate_html_output(output_html_file)
        print(f"HTML 热力图已生成: {output_html_file}")
        
        output_image_file_png = f"study_heatmap_{year_arg}_mpl.png"
        heatmap_generator.generate_mpl_heatmap(output_image_file_png)
        print(f"PNG 热力图已生成: {output_image_file_png}")

        output_image_file_svg = f"study_heatmap_{year_arg}_mpl.svg"
        heatmap_generator.generate_mpl_heatmap(output_image_file_svg)
        print(f"矢量 SVG 热力图已生成: {output_image_file_svg}")

    except sqlite3.Error as e:
        print(f"数据库错误: {e} (数据库路径: {db_path_arg})")
    except Exception as e:
        import traceback
        print(f"发生了意外错误: {e}")
        traceback.print_exc()
    finally:
        if db_conn:
            db_conn.close()
