# replay_pygame.py
# 一个功能增强的、使用 Pygame 动态可视化 simulation_log.txt 的交互式回放工具 (最终稳定版)

import pygame
import pandas as pd
import sys
import argparse
try:
    from matplotlib import colormaps
except ImportError:
    from matplotlib import cm as colormaps

# --- 常量定义 ---
WHITE, GRAY, BLACK = (230, 230, 230), (120, 120, 120), (20, 20, 20)
GRID_COLOR, BACKGROUND_COLOR, PANEL_COLOR = (60, 60, 60), (30, 30, 30), (40, 40, 40)
BUTTON_COLOR, BUTTON_HOVER_COLOR = (80, 80, 80), (110, 110, 110)
SCRUBBER_COLOR, SCRUBBER_HANDLE_COLOR = (80, 80, 80), (150, 150, 250)
SCREEN_WIDTH, SCREEN_HEIGHT = 1400, 700
PADDING, LEGEND_WIDTH, BOTTOM_PANEL_HEIGHT = 60, 320, 60
BASE_SPEED_SCALER = 0.1

class Button:
    """一个简单的按钮类"""
    def __init__(self, rect, text, font, sub_rect_base=None):
        self.base_rect, self.text, self.font = pygame.Rect(rect), text, font
        self.is_hovered, self.sub_rect_base = False, sub_rect_base
    def get_screen_rect(self):
        return self.base_rect.move(self.sub_rect_base.topleft) if self.sub_rect_base else self.base_rect
    def draw(self, surface):
        color = BUTTON_HOVER_COLOR if self.is_hovered else BUTTON_COLOR
        pygame.draw.rect(surface, color, self.base_rect, border_radius=5)
        text_surface = self.font.render(self.text, True, WHITE)
        surface.blit(text_surface, text_surface.get_rect(center=self.base_rect.center))
    def handle_event(self, event, mouse_pos):
        rect_to_check = self.get_screen_rect()
        if event.type == pygame.MOUSEMOTION: self.is_hovered = rect_to_check.collidepoint(mouse_pos)
        if event.type == pygame.MOUSEBUTTONDOWN and event.button == 1 and self.is_hovered: return True
        return False

class ScrollablePanel:
    """一个可滚动的面板类"""
    def __init__(self, rect):
        self.rect, self.scroll_y, self.content_height = rect, 0, rect.height
    def handle_event(self, event, mouse_pos):
        if self.rect.collidepoint(mouse_pos) and event.type == pygame.MOUSEWHEEL:
            self.scroll_y -= event.y * 30
            max_scroll = self.content_height - self.rect.height
            self.scroll_y = max(0, min(self.scroll_y, max_scroll if max_scroll > 0 else 0))
    def get_surface(self): return pygame.Surface((self.rect.width, self.content_height), pygame.SRCALPHA)
    def draw(self, screen, content_surface):
        screen.blit(content_surface, self.rect.topleft, (0, self.scroll_y, self.rect.width, self.rect.height))
        pygame.draw.rect(screen, GRAY, self.rect, 1, 5)

def load_data(filepath):
    """加载、解析并预处理统一的日志文件"""
    print(f"Loading data from '{filepath}'...")
    try:
        df = pd.read_csv(filepath, sep=r'\s+', skiprows=[1], index_col=False)
        all_npu_ids = df['NPU_Global_ID'].unique()
        new_rows = []
        for npu_id in all_npu_ids:
            npu_df = df[df['NPU_Global_ID'] == npu_id]
            if not npu_df.empty and npu_df['Time'].min() > 0:
                first_row = npu_df.iloc[0]
                new_rows.append({
                    'Time': 0, 'NPU_Global_ID': npu_id, 'Server_ID': first_row['Server_ID'], 
                    'NPU_Local_ID': first_row['NPU_Local_ID'], 'Used_Memory': 0, 
                    'Max_Memory': first_row['Max_Memory'], 'Queue_Size': 0, 
                    'Running_Tasks_Count': 0, 'Completed_Batch_Size_NPU': 0,
                    'Cumulative_Batch_Size': 0, 'Cumulative_Users_Completed_OnTime': 0,
                    'Cumulative_Users_Timeout': 0
                })
        if new_rows:
            df = pd.concat([df, pd.DataFrame(new_rows)], ignore_index=True)
        
        df.sort_values(by=['NPU_Global_ID', 'Time'], inplace=True)
        fill_cols = ['Used_Memory', 'Max_Memory', 'Queue_Size', 'Running_Tasks_Count', 
                     'Cumulative_Batch_Size', 'Cumulative_Users_Completed_OnTime', 'Cumulative_Users_Timeout']
        df[fill_cols] = df.groupby('NPU_Global_ID', observed=False)[fill_cols].ffill()
        df.fillna(0, inplace=True)

        df['Time_Diff'] = df.groupby('NPU_Global_ID', observed=False)['Time'].diff().fillna(0)
        df['Prev_Used_Memory'] = df.groupby('NPU_Global_ID', observed=False)['Used_Memory'].shift(1).fillna(0)
        df['Mem_Time_Area'] = df['Prev_Used_Memory'] * df['Time_Diff']
        df['Cumulative_Mem_Time'] = df.groupby('NPU_Global_ID', observed=False)['Mem_Time_Area'].cumsum().fillna(0)
        print("Pre-processed data: unified log loaded and calculated.")
        
        df['NPU_Global_ID'] = df['NPU_Global_ID'].astype('category')
        npu_groups = {npu_id: group.copy() for npu_id, group in df.groupby('NPU_Global_ID', observed=False)}
        return df, npu_groups
    except Exception as e:
        print(f"Error loading or parsing file: {e}", file=sys.stderr)
        sys.exit(1)

# 【修正1】将所有绘图逻辑合并到一个函数中，彻底解决NameError
def draw_graph(screen, font, graph_r, time_window, max_val, title, data_groups, value_col, visibility_dict, color_map, line_width=1):
    """绘制一个完整的图表，包括背景、网格、坐标轴、标题和阶梯数据线"""
    # 1. 绘制背景和标题
    pygame.draw.rect(screen, BLACK, graph_r)
    title_surf = font.render(title, True, GRAY)
    screen.blit(title_surf, (graph_r.left + 5, graph_r.top + 5))
    if max_val == 0: max_val = 1

    # 2. 绘制坐标轴和网格
    # Y轴
    for i in range(6):
        y = graph_r.top + i * (graph_r.height / 5)
        pygame.draw.line(screen, GRID_COLOR, (graph_r.left, y), (graph_r.right, y))
        label_text = f"{int(max_val * (1 - i / 5))}"
        label_surface = font.render(label_text, True, WHITE)
        screen.blit(label_surface, (graph_r.left - label_surface.get_width() - 5, y - label_surface.get_height() // 2))
    # X轴
    start_time, end_time = time_window
    for i in range(11):
        x = graph_r.left + i * (graph_r.width / 10)
        pygame.draw.line(screen, GRID_COLOR, (x, graph_r.top), (x, graph_r.bottom))
        if i % 2 == 0:
            time_val = start_time + i * (end_time - start_time) / 10
            label_text = f"{int(time_val)}"
            label_surface = font.render(label_text, True, WHITE)
            screen.blit(label_surface, (x - label_surface.get_width() // 2, graph_r.bottom + 5))

    # 3. 绘制阶梯数据线
    win_width = end_time - start_time
    for npu_id, group_df in data_groups.items():
        if visibility_dict.get(npu_id):
            sub_df = group_df[group_df['Time'] <= end_time]
            last_point_before = sub_df[sub_df['Time'] < start_time].tail(1)
            points_in_window = sub_df[(sub_df['Time'] >= start_time)]
            plot_points_data, last_val = [], 0
            if not last_point_before.empty:
                last_val = last_point_before.iloc[0][value_col]
            plot_points_data.append({'Time': start_time, value_col: last_val})
            if not points_in_window.empty:
                plot_points_data.extend(points_in_window[['Time', value_col]].to_dict('records'))
            
            pixel_points, prev_p = [], None
            for p in plot_points_data:
                current_val = p[value_col]
                if prev_p:
                    px_curr = graph_r.left + ((p['Time'] - start_time) / win_width if win_width > 0 else 0) * graph_r.width
                    py_prev = graph_r.bottom - (prev_p[value_col] / max_val) * graph_r.height
                    pixel_points.append((px_curr, py_prev))
                px = graph_r.left + ((p['Time'] - start_time) / win_width if win_width > 0 else 0) * graph_r.width
                py = graph_r.bottom - (current_val / max_val) * graph_r.height
                pixel_points.append((px, py))
                prev_p = p
            if len(pixel_points) > 1:
                pixel_points.append((graph_r.right, pixel_points[-1][1]))
                pygame.draw.lines(screen, color_map.get(npu_id), False, pixel_points, line_width)

def draw_bottom_panel(screen, scrubber_rect, font, current_time, max_time, buttons):
    # ... 此函数无变化 ...
    pygame.draw.rect(screen, SCRUBBER_COLOR, scrubber_rect, border_radius=3)
    time_ratio = current_time / max_time if max_time > 0 else 0
    handle_x = scrubber_rect.left + time_ratio * scrubber_rect.width
    handle_rect = pygame.Rect(0, 0, 8, scrubber_rect.height + 4); handle_rect.centery, handle_rect.centerx = scrubber_rect.centery, handle_x
    pygame.draw.rect(screen, SCRUBBER_HANDLE_COLOR, handle_rect, border_radius=3)
    time_text = f"Time: {int(current_time)} / {int(max_time)} ms"
    text_surface = font.render(time_text, True, WHITE)
    screen.blit(text_surface, (scrubber_rect.left, scrubber_rect.bottom + 5))
    for btn in buttons: btn.draw(screen)

def main():
    parser = argparse.ArgumentParser(description="Interactively visualizes NPU logs.", formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    parser.add_argument("--file", required=True, help="Path to the comprehensive simulation_log.txt file.")
    parser.add_argument("--window", type=int, default=500, help="Initial width of the time window in ms.")
    args = parser.parse_args()
    
    current_window_size = args.window

    pygame.init()
    screen = pygame.display.set_mode((SCREEN_WIDTH, SCREEN_HEIGHT))
    pygame.display.set_caption("NPU Status Replay (Interactive)")
    clock = pygame.time.Clock()
    font, ui_font, title_font, data_font = pygame.font.SysFont("Consolas", 12), pygame.font.SysFont("Calibri", 16), pygame.font.SysFont("Calibri", 18, bold=True), pygame.font.SysFont("Consolas", 14)

    df, npu_groups = load_data(args.file)
    if df.empty: return

    max_time, max_memory, max_queue = df['Time'].max(), df['Max_Memory'].max(), df['Queue_Size'].max()
    if max_memory == 0: max_memory = 1
    if max_queue == 0: max_queue = 5
    
    npu_ids = sorted(df['NPU_Global_ID'].unique())
    cmap = colormaps.get_cmap('tab10')
    npu_color_map = {npu_id: [int(c*255) for c in cmap(i/len(npu_ids) if len(npu_ids) > 1 else 0.5)[:3]] for i, npu_id in enumerate(npu_ids)}

    main_area_height = SCREEN_HEIGHT - PADDING * 2 - BOTTOM_PANEL_HEIGHT
    mem_graph_height = main_area_height * 0.6
    queue_graph_height = main_area_height * 0.4 - 40
    mem_graph_rect = pygame.Rect(PADDING, PADDING, SCREEN_WIDTH - PADDING * 2 - LEGEND_WIDTH, mem_graph_height)
    queue_graph_rect = pygame.Rect(PADDING, mem_graph_rect.bottom + 40, mem_graph_rect.width, queue_graph_height)
    scrubber_rect = pygame.Rect(PADDING, queue_graph_rect.bottom + 40, mem_graph_rect.width, 10)
    
    sidebar_x = mem_graph_rect.right + PADDING / 2
    sidebar_height = mem_graph_rect.height + queue_graph_rect.height + 40
    upper_sidebar_rect = pygame.Rect(sidebar_x, PADDING, LEGEND_WIDTH, sidebar_height // 2 - 10)
    lower_sidebar_rect = pygame.Rect(sidebar_x, upper_sidebar_rect.bottom + 20, LEGEND_WIDTH, sidebar_height // 2 - 10)

    is_paused, is_scrubbing, speed_index = False, False, 4
    SPEED_LEVELS = [0.01, 0.05, 0.1, 0.5, 1, 2, 3, 5, 10, 20, 50]
    npu_visibility = {npu_id: True for npu_id in npu_ids}

    speed_decrease_button = Button((PADDING, 10, 40, 30), "-", ui_font)
    speed_increase_button = Button((PADDING + 135, 10, 40, 30), "+", ui_font)
    speed_display_rect = pygame.Rect(PADDING + 45, 10, 85, 30)
    
    btn_y = scrubber_rect.bottom + 5
    btn_w, btn_h, btn_gap = 60, 25, 10
    pause_button = Button((0,0,80,btn_h), "Pause", ui_font)
    nudge_minus_10, nudge_minus_1 = Button((0,0,btn_w,btn_h), "-10", ui_font), Button((0,0,btn_w,btn_h), "-1", ui_font)
    nudge_plus_1, nudge_plus_10 = Button((0,0,btn_w,btn_h), "+1", ui_font), Button((0,0,btn_w,btn_h), "+10", ui_font)
    bottom_buttons = [nudge_minus_10, nudge_minus_1, pause_button, nudge_plus_1, nudge_plus_10]
    total_bottom_width = sum(b.base_rect.width for b in bottom_buttons) + (len(bottom_buttons) - 1) * btn_gap
    current_x = scrubber_rect.centerx - total_bottom_width / 2
    for btn in bottom_buttons: btn.base_rect.x, btn.base_rect.y = current_x, btn_y; current_x += btn.base_rect.width + btn_gap

    npu_panel = ScrollablePanel(upper_sidebar_rect)
    select_all_button = Button((10, 40, 120, 25), "Select All", ui_font, upper_sidebar_rect)
    deselect_all_button = Button((140, 40, 120, 25), "Deselect All", ui_font, upper_sidebar_rect)
    legend_entries, entry_y = [], 75
    for npu_id in npu_ids:
        entry_rect = pygame.Rect(10, entry_y, upper_sidebar_rect.width - 20, 20)
        legend_entries.append({'rect': entry_rect, 'id': npu_id, 'color': npu_color_map.get(npu_id)})
        entry_y += 22
    npu_panel.content_height = entry_y
    data_panel = ScrollablePanel(lower_sidebar_rect)

    running, current_time = True, 0.0
    while running:
        mouse_pos = pygame.mouse.get_pos()
        for event in pygame.event.get():
            if event.type == pygame.QUIT or (event.type == pygame.KEYDOWN and event.key == pygame.K_ESCAPE): running = False
            
            npu_panel.handle_event(event, mouse_pos); data_panel.handle_event(event, mouse_pos)
            
            if event.type == pygame.MOUSEWHEEL and (mem_graph_rect.collidepoint(mouse_pos) or queue_graph_rect.collidepoint(mouse_pos)):
                if event.y > 0: current_window_size = max(20, current_window_size - (int(current_window_size * 0.1) or 1))
                elif event.y < 0: current_window_size = min(max_time, current_window_size + (int(current_window_size * 0.2) or 1))
            
            if speed_decrease_button.handle_event(event, mouse_pos): speed_index = max(0, speed_index - 1)
            if speed_increase_button.handle_event(event, mouse_pos): speed_index = min(len(SPEED_LEVELS) - 1, speed_index + 1)
            if select_all_button.handle_event(event, mouse_pos): npu_visibility = {npu_id: True for npu_id in npu_ids}
            if deselect_all_button.handle_event(event, mouse_pos): npu_visibility = {npu_id: False for npu_id in npu_ids}
            for btn in bottom_buttons:
                if btn.handle_event(event, mouse_pos):
                    if btn == pause_button: is_paused = not is_paused
                    else: is_paused = True; current_time += int(btn.text)
            current_time = max(0, min(max_time, current_time))

            if event.type == pygame.MOUSEBUTTONDOWN and event.button == 1:
                if upper_sidebar_rect.collidepoint(mouse_pos):
                    local_mouse_y = mouse_pos[1] - upper_sidebar_rect.top + npu_panel.scroll_y
                    for entry in legend_entries:
                        if entry['rect'].collidepoint(mouse_pos[0] - upper_sidebar_rect.left, local_mouse_y):
                            npu_visibility[entry['id']] = not npu_visibility[entry['id']]
                if scrubber_rect.collidepoint(mouse_pos):
                    is_scrubbing, is_paused = True, True
                    current_time = max(0, min(max_time, ((mouse_pos[0] - scrubber_rect.left) / scrubber_rect.width) * max_time))
            if event.type == pygame.MOUSEBUTTONUP and event.button == 1: is_scrubbing = False
            if event.type == pygame.MOUSEMOTION and is_scrubbing:
                current_time = max(0, min(max_time, ((mouse_pos[0] - scrubber_rect.left) / scrubber_rect.width) * max_time))

        speed_multiplier = SPEED_LEVELS[speed_index]
        if not is_paused:
            current_time += clock.get_time() * speed_multiplier * BASE_SPEED_SCALER
            if current_time > max_time: current_time = max_time
        start_time, end_time = int(current_time), int(current_time) + current_window_size
        
        screen.fill(BACKGROUND_COLOR)
        
        # --- 【修正】使用统一的 draw_graph 函数绘制所有图表 ---
        draw_graph(screen, font, mem_graph_rect, (start_time, end_time), max_memory, "Memory Usage (MB)", npu_groups, 'Used_Memory', npu_visibility, npu_color_map, 2)
        draw_graph(screen, font, queue_graph_rect, (start_time, end_time), max_queue, "Queue Size", npu_groups, 'Queue_Size', npu_visibility, npu_color_map, 1)
        
        speed_decrease_button.draw(screen); speed_increase_button.draw(screen)
        pygame.draw.rect(screen, BLACK, speed_display_rect)
        speed_surf = ui_font.render(f"{speed_multiplier:g}x", True, WHITE)
        screen.blit(speed_surf, speed_surf.get_rect(center=speed_display_rect.center))
        
        last_global_state = df[df['Time'] <= start_time].tail(1).iloc[0] if not df[df['Time'] <= start_time].empty else None
        
        title_surf = title_font.render("System Status", True, WHITE)
        screen.blit(title_surf, (SCREEN_WIDTH - PADDING - title_surf.get_width(), 10))
        info_text = f"Time: {start_time:7d} ms | Window: {current_window_size} ms"
        info_surf = ui_font.render(info_text, True, GRAY)
        screen.blit(info_surf, (SCREEN_WIDTH - PADDING - info_surf.get_width(), 15 + title_surf.get_height()))
        
        npu_panel_surf = npu_panel.get_surface(); npu_panel_surf.fill(PANEL_COLOR)
        npu_panel_surf.blit(title_font.render("NPU Selector", True, WHITE), (10, 5))
        select_all_button.draw(npu_panel_surf); deselect_all_button.draw(npu_panel_surf)
        for entry in legend_entries:
            text_color = WHITE if npu_visibility[entry['id']] else GRAY
            pygame.draw.rect(npu_panel_surf, entry['color'], (entry['rect'].x + 10, entry['rect'].y + 2, 15, 15))
            npu_panel_surf.blit(font.render(f"NPU {entry['id']}", True, text_color), (entry['rect'].x + 35, entry['rect'].y))
        npu_panel.draw(screen, npu_panel_surf)
        
        data_panel_surf = data_panel.get_surface(); data_panel_surf.fill(PANEL_COLOR)
        data_panel_surf.blit(title_font.render("Data @ Time Cursor", True, WHITE), (10, 5))
        rt_y = 40
        for npu_id in npu_ids:
            last_point = df[(df['Time'] <= start_time) & (df['NPU_Global_ID'] == npu_id)].tail(1)
            used_mem, queue_size, running_count, tick_batch, util_percent = 0, 0, 0, 0, 0.0
            cum_batch, cum_users_ok, cum_users_late = 0,0,0
            if not last_point.empty:
                row = last_point.iloc[0]
                max_mem = row.Max_Memory if row.Max_Memory > 0 else 1
                used_mem, queue_size, running_count = int(row.Used_Memory), int(row.Queue_Size), int(row.Running_Tasks_Count)
                tick_batch = int(row.Completed_Batch_Size_NPU) if row.Time == start_time else 0
                cumulative_used_area = row.Cumulative_Mem_Time + row.Used_Memory * (start_time - row.Time)
                cumulative_total_area = max_mem * start_time
                util_percent = (cumulative_used_area / cumulative_total_area * 100) if cumulative_total_area > 0 else 0
                if last_global_state is not None:
                    cum_batch, cum_users_ok, cum_users_late = int(last_global_state['Cumulative_Batch_Size']), int(last_global_state['Cumulative_Users_Completed_OnTime']), int(last_global_state['Cumulative_Users_Timeout'])

            text_color = WHITE if npu_visibility[npu_id] else GRAY
            y_offset = rt_y
            prefix = f"NPU {npu_id:<3}:"
            data_panel_surf.blit(data_font.render(prefix, True, text_color), (10, y_offset)); y_offset += 20
            line1 = f"  Memory: {int(used_mem):>5} MB | Running: {running_count}"
            data_panel_surf.blit(font.render(line1, True, text_color), (15, y_offset)); y_offset += 15
            line2 = f"  BSize Tick/Total: {tick_batch} / {cum_batch}"
            data_panel_surf.blit(font.render(line2, True, text_color), (15, y_offset)); y_offset += 15
            line3 = f"  Users OK/Late:  {cum_users_ok}/{cum_users_late}"
            data_panel_surf.blit(font.render(line3, True, text_color), (15, y_offset)); y_offset += 15
            line4 = f"  Queue: {queue_size:<5} | Mem Util: {util_percent:.1f}%"
            data_panel_surf.blit(font.render(line4, True, text_color), (15, y_offset)); y_offset += 15
            pygame.draw.line(data_panel_surf, GRID_COLOR, (10, y_offset), (lower_sidebar_rect.width - 10, y_offset))
            rt_y = y_offset + 8
        data_panel.content_height = rt_y
        data_panel.draw(screen, data_panel_surf)
            
        pause_button.text = "Play" if is_paused else "Pause"
        draw_bottom_panel(screen, scrubber_rect, ui_font, current_time, max_time, bottom_buttons)
        pygame.display.flip()
        clock.tick(60)

    pygame.quit()
    sys.exit()

if __name__ == '__main__':
    main()