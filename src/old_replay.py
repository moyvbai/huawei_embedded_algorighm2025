# replay_pygame.py
# 一个功能增强的、使用 Pygame 动态可视化 memory_log.txt 和 queue_log.txt 的交互式回放工具

import pygame
import pandas as pd
import sys
import argparse
import matplotlib.cm as cm

# --- 常量定义 ---
WHITE, GRAY, BLACK = (230, 230, 230), (120, 120, 120), (20, 20, 20)
GRID_COLOR, BACKGROUND_COLOR, PANEL_COLOR = (60, 60, 60), (30, 30, 30), (40, 40, 40)
BUTTON_COLOR, BUTTON_HOVER_COLOR = (80, 80, 80), (110, 110, 110)
SCRUBBER_COLOR, SCRUBBER_HANDLE_COLOR = (80, 80, 80), (150, 150, 250)
BAR_COLOR_CURRENT, BAR_COLOR_UTIL = (70, 70, 180), (70, 180, 70) 
SCREEN_WIDTH, SCREEN_HEIGHT = 1800, 900
PADDING, LEGEND_WIDTH, BOTTOM_PANEL_HEIGHT = 60, 280, 60
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
        if event.type == pygame.MOUSEMOTION: self.is_hovered = self.get_screen_rect().collidepoint(mouse_pos)
        if event.type == pygame.MOUSEBUTTONDOWN and event.button == 1 and self.is_hovered: return True
        return False

class ScrollablePanel:
    """一个可滚动的面板类"""
    def __init__(self, rect):
        self.rect, self.scroll_y, self.content_height = rect, 0, rect.height
    def handle_event(self, event, mouse_pos):
        if self.rect.collidepoint(mouse_pos) and event.type == pygame.MOUSEWHEEL:
            self.scroll_y -= event.y * 20
            max_scroll = self.content_height - self.rect.height
            self.scroll_y = max(0, min(self.scroll_y, max_scroll if max_scroll > 0 else 0))
    def get_surface(self): return pygame.Surface((self.rect.width, self.content_height), pygame.SRCALPHA)
    def draw(self, screen, content_surface):
        screen.blit(content_surface, self.rect.topleft, (0, self.scroll_y, self.rect.width, self.rect.height))
        pygame.draw.rect(screen, GRAY, self.rect, 1, 5)

def load_and_merge_data(mem_path, queue_path):
    """加载、合并并预处理内存和队列日志文件"""
    print(f"Loading memory data from '{mem_path}'...")
    try:
        mem_df = pd.read_csv(mem_path, sep=r'\s+', skiprows=[1], index_col=False)
    except Exception as e:
        print(f"Error loading memory log file: {e}", file=sys.stderr)
        sys.exit(1)

    print(f"Loading queue data from '{queue_path}'...")
    try:
        queue_df = pd.read_csv(queue_path, sep=r'\s+', skiprows=[1], index_col=False)
    except Exception as e:
        print(f"Error loading queue log file: {e}", file=sys.stderr)
        sys.exit(1)

    df = pd.merge(mem_df, queue_df, on=['Time', 'NPU_Global_ID', 'Server_ID', 'NPU_Local_ID'], how='outer')
    df.sort_values(by=['NPU_Global_ID', 'Time'], inplace=True)
    fill_cols = ['Used_Memory', 'Max_Memory', 'Queue_Size']
    df[fill_cols] = df.groupby('NPU_Global_ID', observed=False)[fill_cols].ffill()
    df.dropna(subset=fill_cols, inplace=True)

    all_npu_ids = df['NPU_Global_ID'].unique()
    new_rows = []
    for npu_id in all_npu_ids:
        npu_df = df[df['NPU_Global_ID'] == npu_id]
        if not npu_df.empty and npu_df['Time'].min() > 0:
            first_row = npu_df.iloc[0]
            new_rows.append({'Time': 0, 'NPU_Global_ID': npu_id, 'Server_ID': first_row['Server_ID'], 
                             'NPU_Local_ID': first_row['NPU_Local_ID'], 'Used_Memory': 0, 
                             'Max_Memory': first_row['Max_Memory'], 'Queue_Size': 0})
    if new_rows:
        df = pd.concat([df, pd.DataFrame(new_rows)], ignore_index=True)
    
    df.sort_values(by=['NPU_Global_ID', 'Time'], inplace=True)
    df['Time_Diff'] = df.groupby('NPU_Global_ID', observed=False)['Time'].diff().fillna(0)
    df['Prev_Used_Memory'] = df.groupby('NPU_Global_ID', observed=False)['Used_Memory'].shift(1).fillna(0)
    df['Mem_Time_Area'] = df['Prev_Used_Memory'] * df['Time_Diff']
    df['Cumulative_Mem_Time'] = df.groupby('NPU_Global_ID', observed=False)['Mem_Time_Area'].cumsum().fillna(0)
    print("Pre-processed data: merged logs and calculated cumulative memory-time product.")

    print(f"Data loaded successfully. Found {df['Time'].max()}ms of records.")
    df['NPU_Global_ID'] = df['NPU_Global_ID'].astype('category')
    npu_groups = {npu_id: group.copy() for npu_id, group in df.groupby('NPU_Global_ID', observed=False)}
    return df, npu_groups

def draw_graph_area(screen, font, graph_rect, time_window, max_val, title):
    """绘制一个通用的图表区域"""
    pygame.draw.rect(screen, BLACK, graph_rect)
    screen.blit(font.render(title, True, GRAY), (graph_rect.left + 5, graph_rect.top + 5))
    if max_val == 0: max_val = 1
    for i in range(6):
        y = graph_rect.top + i * (graph_rect.height / 5)
        pygame.draw.line(screen, GRID_COLOR, (graph_rect.left, y), (graph_rect.right, y))
        label_text = f"{int(max_val * (1 - i / 5))}"
        label_surface = font.render(label_text, True, WHITE)
        screen.blit(label_surface, (graph_rect.left - label_surface.get_width() - 5, y - label_surface.get_height() // 2))
    for i in range(11):
        x = graph_rect.left + i * (graph_rect.width / 10)
        pygame.draw.line(screen, GRID_COLOR, (x, graph_rect.top), (x, graph_rect.bottom))
        if i % 2 == 0:
            time_val = time_window[0] + i * (time_window[1] - time_window[0]) / 10
            label_text = f"{int(time_val)}"
            label_surface = font.render(label_text, True, WHITE)
            screen.blit(label_surface, (x - label_surface.get_width() // 2, graph_rect.bottom + 5))

def draw_bottom_panel(screen, scrubber_rect, font, current_time, max_time, buttons):
    """绘制底部的时间轴和所有控制按钮"""
    # 绘制 Scrubber
    pygame.draw.rect(screen, SCRUBBER_COLOR, scrubber_rect, border_radius=3)
    time_ratio = current_time / max_time if max_time > 0 else 0
    handle_x = scrubber_rect.left + time_ratio * scrubber_rect.width
    handle_rect = pygame.Rect(0, 0, 8, scrubber_rect.height + 4)
    handle_rect.centery, handle_rect.centerx = scrubber_rect.centery, handle_x
    pygame.draw.rect(screen, SCRUBBER_HANDLE_COLOR, handle_rect, border_radius=3)
    
    # 绘制时间文本
    time_text = f"Time: {int(current_time)} / {int(max_time)} ms"
    text_surface = font.render(time_text, True, WHITE)
    screen.blit(text_surface, (scrubber_rect.left, scrubber_rect.bottom + 5))

    # 绘制所有控制按钮
    for btn in buttons:
        btn.draw(screen)

def main():
    parser = argparse.ArgumentParser(description="Interactively visualizes NPU logs.", formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    parser.add_argument("--mem-file", required=True, help="Path to the memory_log.txt file.")
    parser.add_argument("--queue-file", required=True, help="Path to the queue_log.txt file.")
    parser.add_argument("--window", type=int, default=500, help="Initial width of the time window in ms.")
    args = parser.parse_args()
    
    current_window_size = args.window

    pygame.init()
    screen = pygame.display.set_mode((SCREEN_WIDTH, SCREEN_HEIGHT))
    pygame.display.set_caption("NPU Status Replay (Interactive)")
    clock = pygame.time.Clock()
    font, ui_font, title_font = pygame.font.SysFont("Consolas", 12), pygame.font.SysFont("Calibri", 16), pygame.font.SysFont("Calibri", 18, bold=True)

    df, npu_groups = load_and_merge_data(args.mem_file, args.queue_file)
    if df.empty: return

    max_time, max_memory, max_queue = df['Time'].max(), df['Max_Memory'].max(), df['Queue_Size'].max()
    if max_memory == 0: max_memory = 1
    if max_queue == 0: max_queue = 5
    
    npu_ids = sorted(df['NPU_Global_ID'].unique())
    colors = cm.get_cmap('tab10', len(npu_ids)) if len(npu_ids) > 0 else []
    npu_color_map = {npu_id: [int(c*255) for c in colors(i)[:3]] for i, npu_id in enumerate(npu_ids)}

    # --- 布局定义 ---
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

    # --- UI元素和状态变量初始化 ---
    is_paused, is_scrubbing, speed_index = False, False, 4
    SPEED_LEVELS = [0.01, 0.05, 0.1, 0.5, 1, 2, 3, 5, 10, 20, 50]
    npu_visibility = {npu_id: True for npu_id in npu_ids}

    # 左上角速度控制
    speed_decrease_button = Button((PADDING, 10, 40, 30), "-", ui_font)
    speed_increase_button = Button((PADDING + 135, 10, 40, 30), "+", ui_font)
    speed_display_rect = pygame.Rect(PADDING + 45, 10, 85, 30)
    
    # 【修改】将所有播放控制按钮定义在底部
    btn_y = scrubber_rect.bottom + 5
    btn_w, btn_h, btn_gap = 60, 25, 10
    
    pause_button = Button((0,0,80,btn_h), "Pause", ui_font)
    nudge_minus_10 = Button((0,0,btn_w,btn_h), "-10", ui_font)
    nudge_minus_1 = Button((0,0,btn_w,btn_h), "-1", ui_font)
    nudge_plus_1 = Button((0,0,btn_w,btn_h), "+1", ui_font)
    nudge_plus_10 = Button((0,0,btn_w,btn_h), "+10", ui_font)
    
    bottom_buttons = [nudge_minus_10, nudge_minus_1, pause_button, nudge_plus_1, nudge_plus_10]
    total_bottom_width = sum(b.base_rect.width for b in bottom_buttons) + (len(bottom_buttons) - 1) * btn_gap
    current_x = scrubber_rect.centerx - total_bottom_width / 2
    for btn in bottom_buttons:
        btn.base_rect.x = current_x
        btn.base_rect.y = btn_y
        current_x += btn.base_rect.width + btn_gap

    # 右侧面板
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

    # --- 主循环 ---
    running, current_time = True, 0.0
    while running:
        mouse_pos = pygame.mouse.get_pos()
        for event in pygame.event.get():
            if event.type == pygame.QUIT or (event.type == pygame.KEYDOWN and event.key == pygame.K_ESCAPE): running = False
            
            npu_panel.handle_event(event, mouse_pos)
            data_panel.handle_event(event, mouse_pos)
            
            if event.type == pygame.MOUSEWHEEL and (mem_graph_rect.collidepoint(mouse_pos) or queue_graph_rect.collidepoint(mouse_pos)):
                if event.y > 0: current_window_size = max(20, current_window_size - (int(current_window_size * 0.1) or 1))
                elif event.y < 0: current_window_size = min(max_time, current_window_size + (int(current_window_size * 0.2) or 1))
            
            if speed_decrease_button.handle_event(event, mouse_pos): speed_index = max(0, speed_index - 1)
            if speed_increase_button.handle_event(event, mouse_pos): speed_index = min(len(SPEED_LEVELS) - 1, speed_index + 1)
            
            # 【修改】集中处理底部按钮事件
            if pause_button.handle_event(event, mouse_pos): is_paused = not is_paused
            if nudge_minus_10.handle_event(event, mouse_pos): current_time -= 10; is_paused = True
            if nudge_minus_1.handle_event(event, mouse_pos): current_time -= 1; is_paused = True
            if nudge_plus_1.handle_event(event, mouse_pos): current_time += 1; is_paused = True
            if nudge_plus_10.handle_event(event, mouse_pos): current_time += 10; is_paused = True
            current_time = max(0, min(max_time, current_time))

            if select_all_button.handle_event(event, mouse_pos): npu_visibility = {npu_id: True for npu_id in npu_ids}
            if deselect_all_button.handle_event(event, mouse_pos): npu_visibility = {npu_id: False for npu_id in npu_ids}

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
        draw_graph_area(screen, font, mem_graph_rect, (start_time, end_time), max_memory, "Memory Usage (MB)")
        draw_graph_area(screen, font, queue_graph_rect, (start_time, end_time), max_queue, "Queue Size")
        
        for npu_id, group_df in npu_groups.items():
            if npu_visibility.get(npu_id):
                for value_col, graph_r, max_val in [('Used_Memory', mem_graph_rect, max_memory), ('Queue_Size', queue_graph_rect, max_queue)]:
                    sub_df = group_df[group_df['Time'] <= end_time]
                    last_point_before = sub_df[sub_df['Time'] < start_time].tail(1)
                    points_in_window = sub_df[(sub_df['Time'] >= start_time)]
                    plot_points_data, last_val = [], 0
                    if not last_point_before.empty: last_val = last_point_before.iloc[0][value_col]
                    plot_points_data.append({'Time': start_time, value_col: last_val})
                    if not points_in_window.empty: plot_points_data.extend(points_in_window[['Time', value_col]].to_dict('records'))
                    pixel_points, prev_p = [], None
                    win_width = end_time - start_time
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
                    if pixel_points:
                        pixel_points.append((graph_r.right, pixel_points[-1][1]))
                        pygame.draw.lines(screen, npu_color_map.get(npu_id), False, pixel_points, 2 if value_col == 'Used_Memory' else 1)
        
        # --- UI绘制 ---
        # 左上角
        speed_decrease_button.draw(screen); speed_increase_button.draw(screen)
        pygame.draw.rect(screen, BLACK, speed_display_rect)
        speed_surf = ui_font.render(f"{speed_multiplier:g}x", True, WHITE)
        screen.blit(speed_surf, speed_surf.get_rect(center=speed_display_rect.center))
        
        # 右上角
        title_surf = title_font.render("NPU Status Replay", True, WHITE)
        screen.blit(title_surf, (SCREEN_WIDTH - PADDING - title_surf.get_width(), 10))
        info_text = f"Time: {start_time:7d} ms | Window: {current_window_size} ms"
        info_surf = ui_font.render(info_text, True, GRAY)
        screen.blit(info_surf, (SCREEN_WIDTH - PADDING - info_surf.get_width(), 15 + title_surf.get_height()))
        
        # 右侧边栏
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
        rt_y = 35
        for npu_id in npu_ids:
            last_point = df[(df['Time'] <= start_time) & (df['NPU_Global_ID'] == npu_id)].tail(1)
            percent_curr, percent_util, queue_size = 0.0, 0.0, 0
            if not last_point.empty:
                row = last_point.iloc[0]
                max_mem = row.Max_Memory if row.Max_Memory > 0 else 1
                percent_curr = (row.Used_Memory / max_mem) * 100
                cumulative_used_area = row.Cumulative_Mem_Time + row.Used_Memory * (start_time - row.Time)
                cumulative_total_area = max_mem * start_time
                percent_util = (cumulative_used_area / cumulative_total_area * 100) if cumulative_total_area > 0 else 0
                queue_size = int(row.Queue_Size)
            text_color = WHITE if npu_visibility[npu_id] else GRAY
            text_line1 = f"NPU {npu_id:<2} | Curr: {percent_curr:5.1f}% | Util: {percent_util:5.1f}%"
            text_line2 = f"         Queue Size: {queue_size}"
            data_panel_surf.blit(font.render(text_line1, True, text_color), (10, rt_y))
            data_panel_surf.blit(font.render(text_line2, True, text_color), (10, rt_y + 15))
            rt_y += 40
        data_panel.content_height = rt_y
        data_panel.draw(screen, data_panel_surf)
            
        # 【修改】更新Pause按钮文本并传递整个按钮列表给绘制函数
        pause_button.text = "Play" if is_paused else "Pause"
        draw_bottom_panel(screen, scrubber_rect, ui_font, current_time, max_time, bottom_buttons)
        pygame.display.flip()
        clock.tick(60)

    pygame.quit()
    sys.exit()

if __name__ == '__main__':
    try: import matplotlib.cm as cm
    except ImportError:
        print("Warning: matplotlib not found...", file=sys.stderr)
        import random
        class RandomColorMap:
            def __init__(self, count): self.colors = [[random.randint(50, 255) for _ in range(count)] for _ in range(count)] if count > 0 else []
            def __call__(self, i): return self.colors[i % len(self.colors)] if self.colors else (255,255,255)
        cm = type('dummy', (object,), {'get_cmap': lambda name, count: RandomColorMap(count)})()
    
    main()