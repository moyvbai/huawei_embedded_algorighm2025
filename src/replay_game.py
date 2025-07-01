# replay_pygame.py
# 一个功能增强的、使用 Pygame 动态可视化 memory_log.txt 的交互式回放工具

import pygame
import pandas as pd
import sys
import argparse
import matplotlib.cm as cm

# --- 常量定义 ---
WHITE, GRAY, BLACK = (230, 230, 230), (120, 120, 120), (20, 20, 20)
GRID_COLOR, BACKGROUND_COLOR, PANEL_COLOR = (60, 60, 60), (30, 30, 30), (40, 40, 40)
BUTTON_COLOR, BUTTON_HOVER_COLOR = (80, 80, 80), (110, 110, 110)
SCRUBBER_COLOR, SCRUBBER_HANDLE_COLOR, BAR_COLOR = (80, 80, 80), (150, 150, 250), (70, 70, 180)
SCREEN_WIDTH, SCREEN_HEIGHT = 1280, 720
PADDING, LEGEND_WIDTH, BOTTOM_PANEL_HEIGHT = 60, 220, 60
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

def load_data(filepath):
    """加载并解析日志文件"""
    print(f"Loading data from '{filepath}'...")
    try:
        df = pd.read_csv(filepath, sep=r'\s+', skiprows=[1], index_col=False)
        all_npu_ids = df['NPU_Global_ID'].unique()
        new_rows = []
        for npu_id in all_npu_ids:
            npu_df = df[df['NPU_Global_ID'] == npu_id]
            if not npu_df.empty and npu_df['Time'].min() > 0:
                first_row = npu_df.iloc[0]
                new_rows.append({'Time': 0, 'NPU_Global_ID': npu_id, 'Server_ID': first_row['Server_ID'], 'NPU_Local_ID': first_row['NPU_Local_ID'], 'Used_Memory': 0, 'Max_Memory': first_row['Max_Memory']})
        if new_rows:
            df = pd.concat([df, pd.DataFrame(new_rows)], ignore_index=True).sort_values(by=['Time', 'NPU_Global_ID']).reset_index(drop=True)
            print("Pre-processed data: ensured all NPUs have a state at t=0.")
        print(f"Data loaded successfully. Found {df['Time'].max()}ms of records.")
        df['NPU_Global_ID'] = df['NPU_Global_ID'].astype('category')
        npu_groups = {npu_id: group.copy() for npu_id, group in df.groupby('NPU_Global_ID', observed=False)}
        return df, npu_groups
    except Exception as e:
        print(f"Error loading or parsing file: {e}", file=sys.stderr)
        sys.exit(1)

def draw_graph_area(screen, font, graph_rect, time_window, max_memory):
    """绘制主图表区域"""
    pygame.draw.rect(screen, BLACK, graph_rect)
    for i in range(11):
        y = graph_rect.top + i * (graph_rect.height / 10)
        pygame.draw.line(screen, GRID_COLOR, (graph_rect.left, y), (graph_rect.right, y))
        label_text = f"{int(max_memory * (1 - i / 10))}"
        label_surface = font.render(label_text, True, WHITE)
        screen.blit(label_surface, (graph_rect.left - label_surface.get_width() - 5, y - label_surface.get_height() // 2))
    for i in range(11):
        x = graph_rect.left + i * (graph_rect.width / 10)
        pygame.draw.line(screen, GRID_COLOR, (x, graph_rect.top), (x, graph_rect.bottom))
        time_val = time_window[0] + i * (time_window[1] - time_window[0]) / 10
        label_text = f"{int(time_val/1000)}s" if time_val % 1000 == 0 else f"{int(time_val)}"
        label_surface = font.render(label_text, True, WHITE)
        screen.blit(label_surface, (x - label_surface.get_width() // 2, graph_rect.bottom + 5))

def draw_scrubber(screen, rect, font, current_time, max_time):
    """绘制底部的时间轴拖动条"""
    pygame.draw.rect(screen, SCRUBBER_COLOR, rect, border_radius=3)
    time_ratio = current_time / max_time if max_time > 0 else 0
    handle_x = rect.left + time_ratio * rect.width
    handle_rect = pygame.Rect(0, 0, 8, rect.height + 4)
    handle_rect.centery, handle_rect.centerx = rect.centery, handle_x
    pygame.draw.rect(screen, SCRUBBER_HANDLE_COLOR, handle_rect, border_radius=3)
    time_text = f"Total Time: {int(current_time)} / {int(max_time)} ms"
    text_surface = font.render(time_text, True, WHITE)
    screen.blit(text_surface, (rect.centerx - text_surface.get_width()/2, rect.bottom + 5))

def main():
    parser = argparse.ArgumentParser(description="Interactively visualizes NPU memory usage.", formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    parser.add_argument("--file", required=True, help="Path to the memory_log.txt file.")
    parser.add_argument("--window", type=int, default=500, help="Initial width of the time window in ms.")
    args = parser.parse_args()
    
    current_window_size = args.window

    pygame.init()
    screen = pygame.display.set_mode((SCREEN_WIDTH, SCREEN_HEIGHT))
    pygame.display.set_caption("NPU Memory Usage Replay (Interactive)")
    clock = pygame.time.Clock()
    font, ui_font, title_font = pygame.font.SysFont("Consolas", 12), pygame.font.SysFont("Calibri", 16), pygame.font.SysFont("Calibri", 18, bold=True)

    df, npu_groups = load_data(args.file)
    if df.empty: return

    max_time, max_memory = df['Time'].max(), df['Max_Memory'].max()
    if max_memory == 0: max_memory = 1
    
    npu_ids = sorted(df['NPU_Global_ID'].unique())
    colors = cm.get_cmap('tab10', len(npu_ids)) if len(npu_ids) > 0 else []
    npu_color_map = {npu_id: [int(c*255) for c in colors(i)[:3]] for i, npu_id in enumerate(npu_ids)}

    graph_rect = pygame.Rect(PADDING, PADDING, SCREEN_WIDTH - PADDING * 2 - LEGEND_WIDTH, SCREEN_HEIGHT - PADDING * 2 - BOTTOM_PANEL_HEIGHT)
    scrubber_rect = pygame.Rect(PADDING, graph_rect.bottom + 40, graph_rect.width, 10)
    sidebar_x = graph_rect.right + PADDING / 2
    upper_sidebar_rect = pygame.Rect(sidebar_x, PADDING, LEGEND_WIDTH - PADDING/2, graph_rect.height // 2 - 10)
    lower_sidebar_rect = pygame.Rect(sidebar_x, upper_sidebar_rect.bottom + 20, LEGEND_WIDTH - PADDING/2, graph_rect.height // 2 - 10)

    is_paused, is_scrubbing, speed_index = False, False, 4
    SPEED_LEVELS = [0.01, 0.05, 0.1, 0.5, 1, 2, 3, 5, 10, 20, 50]
    npu_visibility = {npu_id: True for npu_id in npu_ids}

    pause_button = Button((PADDING, 10, 80, 30), "Pause", ui_font)
    speed_button = Button((PADDING + 90, 10, 120, 30), f"Speed: {SPEED_LEVELS[speed_index]:g}x", ui_font)
    
    npu_panel = ScrollablePanel(upper_sidebar_rect)
    select_all_button = Button((10, 40, 90, 25), "Select All", ui_font, upper_sidebar_rect)
    deselect_all_button = Button((110, 40, 90, 25), "Deselect All", ui_font, upper_sidebar_rect)
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
            
            npu_panel.handle_event(event, mouse_pos)
            data_panel.handle_event(event, mouse_pos)
            
            if event.type == pygame.MOUSEWHEEL:
                if graph_rect.collidepoint(mouse_pos):
                    if event.y > 0: # 向上滚轮, 缩小窗口 (Zoom In)
                        zoom_step = int(current_window_size * 0.1) or 1
                        # 【修改】将最小窗口限制从100改为20
                        current_window_size = max(20, current_window_size - zoom_step)
                    elif event.y < 0: # 向下滚轮, 放大窗口 (Zoom Out)
                        zoom_step = int(current_window_size * 0.2) or 1
                        current_window_size = min(max_time, current_window_size + zoom_step)
            
            if pause_button.handle_event(event, mouse_pos): is_paused = not is_paused
            if speed_button.handle_event(event, mouse_pos): speed_index = (speed_index + 1) % len(SPEED_LEVELS)
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
        draw_graph_area(screen, font, graph_rect, (start_time, end_time), max_memory)
        
        for npu_id, group_df in npu_groups.items():
            if npu_visibility.get(npu_id):
                last_point_before = group_df[group_df['Time'] < start_time].tail(1)
                points_in_window = group_df[(group_df['Time'] >= start_time) & (group_df['Time'] <= end_time)]
                plot_points_data, last_mem = [], 0
                if not last_point_before.empty: last_mem = last_point_before.iloc[0]['Used_Memory']
                plot_points_data.append({'Time': start_time, 'Used_Memory': last_mem})
                if not points_in_window.empty: plot_points_data.extend(points_in_window[['Time', 'Used_Memory']].to_dict('records'))
                pixel_points, prev_p = [], None
                win_width = end_time - start_time
                for p in plot_points_data:
                    current_mem = p['Used_Memory']
                    if prev_p:
                        px_curr = graph_rect.left + ((p['Time'] - start_time) / win_width if win_width > 0 else 0) * graph_rect.width
                        py_prev = graph_rect.bottom - (prev_p['Used_Memory'] / max_memory) * graph_rect.height
                        pixel_points.append((px_curr, py_prev))
                    px = graph_rect.left + ((p['Time'] - start_time) / win_width if win_width > 0 else 0) * graph_rect.width
                    py = graph_rect.bottom - (current_mem / max_memory) * graph_rect.height
                    pixel_points.append((px, py))
                    prev_p = p
                if pixel_points:
                    pixel_points.append((graph_rect.right, pixel_points[-1][1]))
                    pygame.draw.lines(screen, npu_color_map.get(npu_id), False, pixel_points, 2)
        
        pause_button.text, speed_button.text = ("Play" if is_paused else "Pause"), f"Speed: {speed_multiplier:g}x"
        pause_button.draw(screen)
        speed_button.draw(screen)

        npu_panel_surf = npu_panel.get_surface()
        npu_panel_surf.fill(PANEL_COLOR)
        title_surf = title_font.render("NPU Selector", True, WHITE)
        npu_panel_surf.blit(title_surf, (10, 5))
        select_all_button.draw(npu_panel_surf)
        deselect_all_button.draw(npu_panel_surf)
        for entry in legend_entries:
            text_color = WHITE if npu_visibility[entry['id']] else GRAY
            pygame.draw.rect(npu_panel_surf, entry['color'], (entry['rect'].x + 10, entry['rect'].y + 2, 15, 15))
            npu_panel_surf.blit(font.render(f"NPU {entry['id']}", True, text_color), (entry['rect'].x + 35, entry['rect'].y))
        npu_panel.draw(screen, npu_panel_surf)
        
        data_panel_surf = data_panel.get_surface()
        data_panel_surf.fill(PANEL_COLOR)
        title_surf = title_font.render("Real-time Usage (%)", True, WHITE)
        data_panel_surf.blit(title_surf, (10, 5))
        rt_y = 35
        for npu_id in npu_ids:
            last_point = df[(df['Time'] <= start_time) & (df['NPU_Global_ID'] == npu_id)].tail(1)
            percent = 0.0
            if not last_point.empty:
                row = last_point.iloc[0]
                percent = (row.Used_Memory / row.Max_Memory * 100) if row.Max_Memory > 0 else 0
            text_color = WHITE if npu_visibility[npu_id] else GRAY
            data_panel_surf.blit(font.render(f"NPU {npu_id}: {percent:.1f}%", True, text_color), (10, rt_y))
            bar_bg_rect = pygame.Rect(10, rt_y + 15, lower_sidebar_rect.width - 20, 5)
            pygame.draw.rect(data_panel_surf, BLACK, bar_bg_rect)
            pygame.draw.rect(data_panel_surf, BAR_COLOR, (bar_bg_rect.left, bar_bg_rect.top, bar_bg_rect.width * (percent / 100), bar_bg_rect.height))
            rt_y += 30
        data_panel.content_height = rt_y
        data_panel.draw(screen, data_panel_surf)
            
        draw_scrubber(screen, scrubber_rect, ui_font, current_time, max_time)
        title_text = f"Time: {start_time:7d} ms | Window: {current_window_size} ms"
        screen.blit(title_font.render(title_text, True, WHITE), (graph_rect.left, 10))
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
            def __init__(self, count):
                self.colors = [[random.randint(50, 255) for _ in range(count)] for _ in range(count)] if count > 0 else []
            def __call__(self, i): return self.colors[i % len(self.colors)] if self.colors else (255,255,255)
        cm = type('dummy', (object,), {'get_cmap': lambda name, count: RandomColorMap(count)})()
    
    main()