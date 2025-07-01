# replay_pygame.py
# An enhanced, interactive replay tool for visualizing NPU memory logs using Pygame.

import pygame
import pandas as pd
import sys
import argparse
import matplotlib.cm as cm

# --- Constants ---
WHITE = (230, 230, 230)
GRAY = (120, 120, 120)
BLACK = (20, 20, 20)
GRID_COLOR = (60, 60, 60)
BACKGROUND_COLOR = (30, 30, 30)
PANEL_COLOR = (40, 40, 40)
BUTTON_COLOR = (80, 80, 80)
BUTTON_HOVER_COLOR = (110, 110, 110)
SCRUBBER_COLOR = (80, 80, 80)
SCRUBBER_HANDLE_COLOR = (150, 150, 250)
BAR_COLOR = (70, 70, 180)
SCREEN_WIDTH, SCREEN_HEIGHT = 1280, 720
PADDING = 60
LEGEND_WIDTH = 220
BOTTOM_PANEL_HEIGHT = 60
BASE_SPEED_SCALER = 0.4

class Button:
    """A simple button class for UI interaction."""
    def __init__(self, rect, text, font):
        self.rect = pygame.Rect(rect)
        self.text, self.font = text, font
        self.is_hovered = False

    def draw(self, screen):
        color = BUTTON_HOVER_COLOR if self.is_hovered else BUTTON_COLOR
        pygame.draw.rect(screen, color, self.rect, border_radius=5)
        text_surface = self.font.render(self.text, True, WHITE)
        screen.blit(text_surface, text_surface.get_rect(center=self.rect.center))

    def handle_event(self, event):
        if event.type == pygame.MOUSEMOTION:
            self.is_hovered = self.rect.collidepoint(event.pos)
        if event.type == pygame.MOUSEBUTTONDOWN and event.button == 1 and self.is_hovered:
            return True
        return False

def load_data(filepath):
    """Loads and parses the log file efficiently using pandas."""
    print(f"Loading data from '{filepath}'...")
    try:
        df = pd.read_csv(filepath, sep=r'\s+', skiprows=[1], index_col=False)
        print(f"Data loaded successfully. Found {df['Time'].max()}ms of records.")
        df['NPU_Global_ID'] = df['NPU_Global_ID'].astype('category')
        df_by_time = df.set_index('Time')
        return df, df_by_time
    except Exception as e:
        print(f"Error loading or parsing file: {e}", file=sys.stderr)
        sys.exit(1)

def draw_graph_area(screen, font, graph_rect, time_window, max_memory):
    """Draws the main graph area, including axes, grid, and labels."""
    pygame.draw.rect(screen, BLACK, graph_rect)
    # Y-axis
    for i in range(11):
        y = graph_rect.top + i * (graph_rect.height / 10)
        pygame.draw.line(screen, GRID_COLOR, (graph_rect.left, y), (graph_rect.right, y))
        label_text = f"{int(max_memory * (1 - i / 10))}"
        label_surface = font.render(label_text, True, WHITE)
        screen.blit(label_surface, (graph_rect.left - label_surface.get_width() - 5, y - label_surface.get_height() // 2))
    # X-axis
    for i in range(11):
        x = graph_rect.left + i * (graph_rect.width / 10)
        pygame.draw.line(screen, GRID_COLOR, (x, graph_rect.top), (x, graph_rect.bottom))
        time_val = time_window[0] + i * (time_window[1] - time_window[0]) / 10
        label_text = f"{int(time_val/1000)}s" if time_val % 1000 == 0 else f"{int(time_val)}"
        label_surface = font.render(label_text, True, WHITE)
        screen.blit(label_surface, (x - label_surface.get_width() // 2, graph_rect.bottom + 5))

def draw_scrubber(screen, rect, font, current_time, max_time):
    """Draws the timeline scrubber at the bottom."""
    pygame.draw.rect(screen, SCRUBBER_COLOR, rect, border_radius=3)
    time_ratio = current_time / max_time if max_time > 0 else 0
    handle_x = rect.left + time_ratio * rect.width
    handle_rect = pygame.Rect(0, 0, 8, rect.height + 4)
    handle_rect.centery, handle_rect.centerx = rect.centery, handle_x
    pygame.draw.rect(screen, SCRUBBER_HANDLE_COLOR, handle_rect, border_radius=3)
    time_text = f"Total Time: {int(current_time)} / {int(max_time)} ms"
    text_surface = font.render(time_text, True, WHITE)
    screen.blit(text_surface, (rect.centerx - text_surface.get_width() / 2, rect.bottom + 5))

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

    df, df_by_time = load_data(args.file)
    if df.empty: return

    max_time, max_memory = df['Time'].max(), df['Max_Memory'].max()
    npu_ids = sorted(df['NPU_Global_ID'].unique())
    colors = cm.get_cmap('tab10', len(npu_ids)) if len(npu_ids) > 0 else []
    npu_color_map = {npu_id: [int(c * 255) for c in colors(i)[:3]] for i, npu_id in enumerate(npu_ids)}

    graph_rect = pygame.Rect(PADDING, PADDING, SCREEN_WIDTH - PADDING * 2 - LEGEND_WIDTH, SCREEN_HEIGHT - PADDING * 2 - BOTTOM_PANEL_HEIGHT)
    scrubber_rect = pygame.Rect(PADDING, graph_rect.bottom + 40, graph_rect.width, 10)
    sidebar_x = graph_rect.right + PADDING / 2
    upper_sidebar_rect = pygame.Rect(sidebar_x, PADDING, LEGEND_WIDTH - PADDING / 2, graph_rect.height / 2 - 10)
    lower_sidebar_rect = pygame.Rect(sidebar_x, upper_sidebar_rect.bottom + 20, LEGEND_WIDTH - PADDING / 2, graph_rect.height / 2 - 10)

    is_paused, is_scrubbing, speed_index = False, False, 2
    SPEED_LEVELS = [0.1, 0.5, 1, 2, 3, 5, 10, 20]
    npu_visibility = {npu_id: True for npu_id in npu_ids}

    pause_button = Button((PADDING, 10, 80, 30), "Pause", ui_font)
    speed_button = Button((PADDING + 90, 10, 120, 30), f"Speed: {SPEED_LEVELS[speed_index]:g}x", ui_font)
    
    legend_entries = []
    entry_y = upper_sidebar_rect.top + 30
    for npu_id in npu_ids:
        entry_rect = pygame.Rect(upper_sidebar_rect.left, entry_y, upper_sidebar_rect.width, 20)
        legend_entries.append({'rect': entry_rect, 'id': npu_id, 'color': npu_color_map.get(npu_id)})
        entry_y += 22

    running, current_time = True, 0.0
    while running:
        mouse_pos = pygame.mouse.get_pos()
        for event in pygame.event.get():
            if event.type == pygame.QUIT or (event.type == pygame.KEYDOWN and event.key == pygame.K_ESCAPE): running = False
            if event.type == pygame.MOUSEWHEEL:
                zoom_step = int(current_window_size * 0.1)
                if event.y > 0: current_window_size = max(100, current_window_size - zoom_step)
                elif event.y < 0: current_window_size = min(max_time, current_window_size + zoom_step)
            
            pause_button.handle_event(event)
            speed_button.handle_event(event)
            
            if event.type == pygame.MOUSEBUTTONDOWN and event.button == 1:
                if pause_button.is_hovered: is_paused = not is_paused
                if speed_button.is_hovered: speed_index = (speed_index + 1) % len(SPEED_LEVELS)
                for entry in legend_entries:
                    if entry['rect'].collidepoint(mouse_pos): npu_visibility[entry['id']] = not npu_visibility[entry['id']]
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
        
        window_df = df[(df['Time'] >= start_time) & (df['Time'] <= end_time)]
        if not window_df.empty:
            # Fix warning by adding observed=False
            for npu_id, group in window_df.groupby('NPU_Global_ID', observed=False):
                if npu_visibility.get(npu_id):
                    points = []
                    win_width = end_time - start_time
                    for row in group.itertuples(index=False):
                        px, py = graph_rect.left + ((row.Time - start_time) / win_width if win_width > 0 else 0) * graph_rect.width, graph_rect.bottom - ((row.Used_Memory / max_memory if max_memory > 0 else 0) * graph_rect.height)
                        points.append((px, py))
                    if len(points) > 1: pygame.draw.lines(screen, npu_color_map.get(npu_id), False, points, 2)
        
        pause_button.text, speed_button.text = ("Play" if is_paused else "Pause"), f"Speed: {speed_multiplier:g}x"
        pause_button.draw(screen)
        speed_button.draw(screen)
        
        pygame.draw.rect(screen, PANEL_COLOR, upper_sidebar_rect, border_radius=5)
        screen.blit(title_font.render("NPU Selector", True, WHITE), (upper_sidebar_rect.x + 10, upper_sidebar_rect.y + 5))
        for entry in legend_entries:
            text_color = WHITE if npu_visibility[entry['id']] else GRAY
            pygame.draw.rect(screen, entry['color'], (entry['rect'].x + 10, entry['rect'].y + 2, 15, 15))
            screen.blit(font.render(f"NPU {entry['id']}", True, text_color), (entry['rect'].x + 35, entry['rect'].y))
            
        pygame.draw.rect(screen, PANEL_COLOR, lower_sidebar_rect, border_radius=5)
        screen.blit(title_font.render("Real-time Usage (%)", True, WHITE), (lower_sidebar_rect.x + 10, lower_sidebar_rect.y + 5))
        
        try:
            realtime_data_slice = df_by_time.loc[start_time]
        except KeyError:
            realtime_data_slice = None
            
        rt_y = lower_sidebar_rect.top + 35
        for npu_id in npu_ids:
            percent, npu_data_row = 0.0, None
            if realtime_data_slice is not None:
                # Fix error by handling both DataFrame (multiple rows) and Series (single row) cases
                if isinstance(realtime_data_slice, pd.DataFrame):
                    rows = realtime_data_slice[realtime_data_slice['NPU_Global_ID'] == npu_id]
                    if not rows.empty: npu_data_row = rows.iloc[0]
                elif isinstance(realtime_data_slice, pd.Series):
                    if realtime_data_slice['NPU_Global_ID'] == npu_id: npu_data_row = realtime_data_slice
            if npu_data_row is not None:
                percent = (npu_data_row['Used_Memory'] / npu_data_row['Max_Memory'] * 100) if npu_data_row['Max_Memory'] > 0 else 0

            text_color = WHITE if npu_visibility[npu_id] else GRAY
            screen.blit(font.render(f"NPU {npu_id}: {percent:.1f}%", True, text_color), (lower_sidebar_rect.left + 10, rt_y))
            bar_bg_rect = pygame.Rect(lower_sidebar_rect.left + 10, rt_y + 15, lower_sidebar_rect.width - 20, 5)
            pygame.draw.rect(screen, BLACK, bar_bg_rect)
            bar_rect = pygame.Rect(bar_bg_rect.left, bar_bg_rect.top, bar_bg_rect.width * (percent / 100), bar_bg_rect.height)
            pygame.draw.rect(screen, BAR_COLOR, bar_rect)
            rt_y += 30

        draw_scrubber(screen, scrubber_rect, ui_font, current_time, max_time)
        title_text = f"NPU Memory Usage | Time: {start_time:7d} ms | Window: {current_window_size} ms"
        screen.blit(title_font.render(title_text, True, WHITE), (graph_rect.left + 140, 10))
        pygame.display.flip()
        clock.tick(60)

    pygame.quit()
    sys.exit()

if __name__ == '__main__':
    try:
        import matplotlib.cm as cm
    except ImportError:
        print("Warning: matplotlib not found, using random colors.", file=sys.stderr)
        import random
        class RandomColorMap:
            def __init__(self, count):
                self.colors = [[random.randint(50, 255) for _ in range(3)] for _ in range(count)] if count > 0 else []
            def __call__(self, i): return self.colors[i % len(self.colors)] if self.colors else (255, 255, 255)
        cm = type('dummy', (object,), {'get_cmap': lambda name, count: RandomColorMap(count)})()
    
    main()