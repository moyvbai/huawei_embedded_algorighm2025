import math
import sys
import matplotlib.pyplot as plt
import matplotlib.colors as mcolors

# 定义错误类型常量
class ErrorTypes:
    INVALID_OUTPUT = "Invalid Output: T_i or time_j violates constraints."
    BATCHSIZE_EXCEEDS_MEMORY = "Batchsize Exceeds Memory: a*B+b > m_server."
    SAMPLES_NOT_FULLY_PROCESSED = "Samples Not Fully Processed: Sum of Batchsizes does not match cnt_i."
    INVALID_TIME_ORDER = "Invalid Time Order: time_j is not strictly increasing."
    INVALID_NPU_INDEX = "Invalid NPU Index: NPU index is out of bounds."
    INVALID_SERVER_INDEX = "Invalid Server Index: Server index is out of bounds."
    INVALID_USER_SEND_TIME = "Invalid User Send Time: User sent a request before the previous one was acknowledged."
    INVALID_START_TIME = "Invalid Start Time: First send time is earlier than the allowed start time s_i."

class Scorer:
    """
    华为嵌入式软件大赛算法组区域初赛题目评分器
    """
    def __init__(self, input_path, output_path):
        self.input_path = input_path
        self.output_path = output_path
        self.errors = []
        self._load_input()

    def _load_input(self):
        """解析输入数据文件"""
        with open(self.input_path, 'r') as f:
            lines = f.readlines()
            
            self.N = int(lines[0])
            self.servers = []
            for i in range(1, self.N + 1):
                g, k, m = map(int, lines[i].split())
                self.servers.append({'g': g, 'k': k, 'm': m, 'id': i})

            self.M = int(lines[self.N + 1])
            self.users = []
            for i in range(self.N + 2, self.N + 2 + self.M):
                s, e, cnt = map(int, lines[i].split())
                self.users.append({'s': s, 'e': e, 'cnt': cnt, 'id': len(self.users) + 1})

            self.latency = []
            for i in range(self.N + 2 + self.M, self.N + 2 + self.M + self.N):
                self.latency.append(list(map(int, lines[i].split())))

            self.a, self.b = map(int, lines[self.N + 2 + self.M + self.N].split())
            
            # 方便索引，将服务器和用户ID从1开始
            self.server_map = {s['id']: s for s in self.servers}
            self.user_map = {u['id']: u for u in self.users}

    def _load_output(self):
        """解析选手的输出文件"""
        self.schedules = {}
        try:
            with open(self.output_path, 'r') as f:
                lines = [line.strip() for line in f if line.strip()]
                line_idx = 0
                for i in range(1, self.M + 1):
                    if line_idx >= len(lines):
                        self.errors.append(f"User {i}: Missing schedule data.")
                        return False
                    
                    T_i = int(lines[line_idx])
                    if not (1 <= T_i <= 300):
                        self.errors.append(f"User {i}: {ErrorTypes.INVALID_OUTPUT} (T_i={T_i} is not in [1, 300])")
                        return False
                    
                    line_idx += 1
                    if line_idx >= len(lines):
                        self.errors.append(f"User {i}: Missing schedule details after T_i.")
                        return False

                    parts = list(map(int, lines[line_idx].split()))
                    if len(parts) != 4 * T_i:
                        self.errors.append(f"User {i}: {ErrorTypes.INVALID_OUTPUT} (Expected {4*T_i} integers, found {len(parts)})")
                        return False

                    user_schedule = []
                    for j in range(T_i):
                        time, server_id, npu_id, B = parts[j*4:(j+1)*4]
                        user_schedule.append({
                            'time': time, 'server_id': server_id,
                            'npu_id': npu_id, 'B': B
                        })
                    self.schedules[i] = user_schedule
                    line_idx += 1
            return True
        except Exception as e:
            self.errors.append(f"Failed to parse output file: {e}")
            return False

    def _validate_output(self):
        """对输出方案进行静态验证"""
        for user_id, schedule in self.schedules.items():
            user = self.user_map[user_id]
            total_samples = 0
            last_time = -1
            
            if schedule[0]['time'] < user['s']:
                self.errors.append(f"User {user_id}: {ErrorTypes.INVALID_START_TIME} (send_time {schedule[0]['time']} < s_i {user['s']})")

            for req in schedule:
                total_samples += req['B']
                
                if req['time'] <= last_time:
                    self.errors.append(f"User {user_id}: {ErrorTypes.INVALID_TIME_ORDER} (time {req['time']} <= last_time {last_time})")
                last_time = req['time']

                if not (1 <= req['server_id'] <= self.N):
                    self.errors.append(f"User {user_id}: {ErrorTypes.INVALID_SERVER_INDEX} (server_id={req['server_id']})")
                    continue # Skip further checks for this req if server is invalid

                server = self.server_map[req['server_id']]
                if not (1 <= req['npu_id'] <= server['g']):
                    self.errors.append(f"User {user_id}: {ErrorTypes.INVALID_NPU_INDEX} (npu_id={req['npu_id']} for server with {server['g']} NPUs)")

                mem_needed = self.a * req['B'] + self.b
                if mem_needed > server['m']:
                    self.errors.append(f"User {user_id}: {ErrorTypes.BATCHSIZE_EXCEEDS_MEMORY} (B={req['B']} needs {mem_needed}MB > server {server['id']} has {server['m']}MB)")

            if total_samples != user['cnt']:
                self.errors.append(f"User {user_id}: {ErrorTypes.SAMPLES_NOT_FULLY_PROCESSED} (processed {total_samples}, expected {user['cnt']})")
        
        return not self.errors

    def run(self):
        """执行模拟和评分"""
        if not self._load_output():
            self._print_errors()
            return
        if not self._validate_output():
            self._print_errors()
            return

        # 初始化模拟环境
        npu_queues = {(s['id'], n+1): [] for s in self.servers for n in range(s['g'])}
        npu_processing = {(s['id'], n+1): None for s in self.servers for n in range(s['g'])}
        server_mem_used = {s['id']: 0 for s in self.servers}
        self.npu_busy_time = {(s['id'], n + 1): 0 for s in self.servers for n in range(s['g'])}
        self.total_simulation_time = 0
        user_progress = {
            u['id']: {
                'sent': 0, 'completed': 0, 
                'next_send_time': u['s'], 'end_time': u['s'],
                'completed_requests': []  # <-- 新增
            } for u in self.users
        }
        # 创建事件队列 (arrival_time, user_id, server_id, npu_id, batch_size, send_time)
        events = []
        for user_id, schedule in self.schedules.items():
            for req in schedule:
                lat = self.latency[req['server_id']-1][user_id-1]
                arrival_time = req['time'] + lat
                events.append((arrival_time, user_id, req['server_id'], req['npu_id'], req['B'], req['time']))
        
        # 按到达时间排序事件
        events.sort()
        
        max_time = events[-1][0] + 60000 if events else 200000 # 估算一个最大模拟时间
        
        # 模拟主循环
        for t in range(max_time):
            # 1. 移除已完成的请求
            for npu_id, req in list(npu_processing.items()):
                if req and req['completion_time'] == t:
                    server_id = npu_id[0]
                    user_id = req['user_id']
                    user_progress[user_id]['completed_requests'].append(req)

                    server_mem_used[server_id] -= (self.a * req['B'] + self.b)
                    npu_processing[npu_id] = None
                    user_progress[user_id]['completed'] += req['B']
                    user_progress[user_id]['end_time'] = max(user_progress[user_id]['end_time'], t)

            # 2. 增加当前时刻到达的请求
            while events and events[0][0] == t:
                arrival_time, user_id, server_id, npu_id_req, B, send_time = events.pop(0)
                
                # 动态发送时间校验
                if send_time < user_progress[user_id]['next_send_time']:
                    self.errors.append(f"User {user_id}: {ErrorTypes.INVALID_USER_SEND_TIME} (sent at {send_time}, but earliest was {user_progress[user_id]['next_send_time']})")
                    self._print_errors()
                    return

                lat = self.latency[server_id-1][user_id-1]
                user_progress[user_id]['next_send_time'] = send_time + lat + 1
                
                npu_key = (server_id, npu_id_req)
                npu_queues[npu_key].append({
                    'arrival_time': arrival_time, 'user_id': user_id, 'B': B, 'send_time': send_time
                })

            # 3. 对所有队列排序并分配资源
            for server in self.servers:
                for npu_idx in range(1, server['g'] + 1):
                    npu_key = (server['id'], npu_idx)
                    
                    # 排序: 1. 到达时间, 2. 用户ID
                    npu_queues[npu_key].sort(key=lambda x: (x['arrival_time'], x['user_id']))
                    
                    # 如果NPU空闲，尝试从队列中分配任务
                    if not npu_processing[npu_key] and npu_queues[npu_key]:
                        # 扫描队列，找到第一个能装下的请求
                        for i, req_to_process in enumerate(npu_queues[npu_key]):
                            mem_needed = self.a * req_to_process['B'] + self.b
                            
                            # 注意：题目描述是整个服务器共享显存，但队列在NPU上。
                            # 这里按照更合理的模型，每个NPU有独立的显存 m_i
                            # 如果是服务器共享，则需要判断 server_mem_used[server['id']] + mem_needed <= server['m'] * server['g']
                            # 这里按题目 "服务器有...显存大小是mMB" 理解为每个NPU的显存
                            if mem_needed <= server['m']: # 假设m_i是单个NPU的显存
                                req = npu_queues[npu_key].pop(i)
                                
                                # 计算推理耗时
                                inference_speed = server['k'] * math.sqrt(req['B'])
                                inference_time = math.ceil(req['B'] / inference_speed)
                                
                                req['send_time'] = req_to_process.get('send_time', -1) # 从原始事件中获取发送时间
                                req['start_time'] = t
                                req['completion_time'] = t + inference_time
                                req['npu_key'] = npu_key # 记录被分配到的NPU

                                # 累加NPU的忙碌时间
                                self.npu_busy_time[npu_key] += inference_time

                                npu_processing[npu_key] = req
                                break # 分配成功，处理下一个NPU
            
            # 检查是否所有任务都完成了
            all_done = True
            for user_id in self.schedules:
                if user_progress[user_id]['completed'] < self.user_map[user_id]['cnt']:
                    all_done = False
                    break
            if all_done:
                self.total_simulation_time = t 
                break
        else: # 如果循环正常结束（未break），说明超时
            self.total_simulation_time = t 
            self.errors.append("Simulation Timeout: Not all tasks could be completed in the given time.")

        if self.errors:
            self._print_errors()
            return
            
        # 4. 计算分数
        self._calculate_score(user_progress)
        # 绘图
        # self._plot_npu_timeline_bar(user_progress)
        # self._plot_npu_gantt_charts(user_progress)
        # self._plot_user_gantt_charts(user_progress)
    
    def _plot_npu_timeline_bar(self, user_progress):
        """
        为每个NPU生成一个单行时间轴条状图，显示用户批次和空闲时间的分布。
        """
        print("\nGenerating timeline bar charts for each NPU...")

        # 1. 重组并排序数据
        npu_tasks = {key: [] for key in self.npu_busy_time.keys()}
        for user_id in range(1, self.M + 1):
            for req in user_progress[user_id].get('completed_requests', []):
                npu_tasks[req['npu_key']].append(req)

        # 2. 准备颜色映射
        cmap = plt.get_cmap('tab20', self.M)
        user_color_map = {i: cmap(i - 1) for i in range(1, self.M + 1)}
        user_color_map['Idle'] = 'lightgrey' # 为空闲时间指定一个颜色

        # 3. 为每个处理过任务的NPU生成图表
        for npu_key, tasks in npu_tasks.items():
            if not tasks:
                continue

            # 按任务开始时间排序，这是生成时间轴的关键
            tasks.sort(key=lambda x: x['start_time'])

            server_id, npu_id = npu_key
            fig, ax = plt.subplots(figsize=(20, 5)) # 图表高度可以小一些

            # 4. 构建时间轴分段列表（包括空闲时间）
            timeline_segments = []
            last_completion_time = 0
            for task in tasks:
                # 检查并添加任务开始前的空闲时间
                if task['start_time'] > last_completion_time:
                    timeline_segments.append({
                        'start': last_completion_time,
                        'duration': task['start_time'] - last_completion_time,
                        'label': 'Idle',
                        'user_id': 'Idle'
                    })
                
                # 添加任务本身
                timeline_segments.append({
                    'start': task['start_time'],
                    'duration': task['completion_time'] - task['start_time'],
                    'label': f"User {task['user_id']} (B={task['B']})",
                    'user_id': task['user_id']
                })
                last_completion_time = task['completion_time']
            
            # 添加最后一个任务完成后的空闲时间（可选，可以画到总模拟时间）
            if last_completion_time < self.total_simulation_time:
                timeline_segments.append({
                        'start': last_completion_time,
                        'duration': self.total_simulation_time - last_completion_time,
                        'label': 'Idle',
                        'user_id': 'Idle'
                    })

            # 5. 绘制所有分段
            for segment in timeline_segments:
                ax.barh(y=0, width=segment['duration'], left=segment['start'], height=0.5,
                        color=user_color_map[segment['user_id']],
                        edgecolor='white')
                
                # 在分段中添加标签（如果分段足够宽）
                if segment['duration'] > self.total_simulation_time / 50: # 避免在太窄的格子上写字
                    text = f"U{segment['user_id']}" if segment['user_id'] != 'Idle' else 'Idle'
                    text_color = 'black'
                    if segment['user_id'] != 'Idle':
                        text_color = 'white' if sum(mcolors.to_rgb(user_color_map[segment['user_id']])[:3]) < 1.5 else 'black'
                    
                    ax.text(segment['start'] + segment['duration'] / 2, 0, text,
                            ha='center', va='center', color=text_color, fontsize=9)

            # 6. 格式化图表
            ax.set_xlim(0, self.total_simulation_time)
            ax.set_xlabel("Time (ms)", fontsize=12)
            ax.set_title(f"NPU Timeline Bar - Server {server_id} / NPU {npu_id}", fontsize=16, fontweight='bold')
            
            # 隐藏Y轴，因为它没有意义
            ax.set_yticks([])
            ax.set_frame_on(False) # 移除边框

            # 创建自定义图例
            legend_labels = sorted(list(set(seg['user_id'] for seg in timeline_segments if isinstance(seg['user_id'], int))))
            legend_labels.append('Idle')
            handles = [plt.Rectangle((0,0),1,1, color=user_color_map[label]) for label in legend_labels]
            ax.legend(handles, [f'User {l}' if l != 'Idle' else 'Idle' for l in legend_labels], 
                    title="Legend", bbox_to_anchor=(1.01, 1), loc='upper left')

            fig.tight_layout()
            plt.subplots_adjust(right=0.85) # 为图例留出空间

            # 保存图表
            filename = f'./pics/timeline_bar_S{server_id}_N{npu_id}.png'
            plt.savefig(filename)
            plt.close(fig)
            print(f"Saved timeline bar for NPU (S{server_id}, N{npu_id}) to {filename}")
    
    def _plot_npu_gantt_charts(self, user_progress):
        """
        为每个NPU生成并保存一个甘特图，展示其处理的所有用户批次任务的时间线。
        """
        print("\nGenerating Gantt charts for each NPU...")

        # 1. 重组数据：将所有请求按NPU进行分组
        npu_tasks = {key: [] for key in self.npu_busy_time.keys()}
        for user_id in range(1, self.M + 1):
            for req in user_progress[user_id].get('completed_requests', []):
                npu_tasks[req['npu_key']].append(req)

        # 2. 为每个用户分配一个稳定的颜色
        # 使用matplotlib的颜色映射来确保用户颜色多样且一致
        cmap = plt.get_cmap('tab20', self.M) 
        user_color_map = {i: cmap(i-1) for i in range(1, self.M + 1)}

        # 3. 为每个处理过任务的NPU生成图表
        for npu_key, tasks in npu_tasks.items():
            if not tasks:
                continue  # 如果NPU没有处理任何任务，则跳过

            server_id, npu_id = npu_key
            fig, ax = plt.subplots(figsize=(20, 10))

            # Y轴代表不同的用户
            user_ids_on_this_npu = sorted(list(set(task['user_id'] for task in tasks)))
            user_to_y = {uid: i for i, uid in enumerate(user_ids_on_this_npu)}

            # 绘制每一个任务的条形
            for task in tasks:
                start_time = task['start_time']
                duration = task['completion_time'] - start_time
                user_id = task['user_id']
                y_pos = user_to_y[user_id]
                
                ax.barh(y=y_pos, width=duration, left=start_time, height=0.6,
                        color=user_color_map[user_id],
                        edgecolor='black',
                        label=f"User {user_id}")
                
                # 在条形中间添加文字，显示批次大小
                text_color = 'white' if sum(mcolors.to_rgb(user_color_map[user_id])[:3]) < 1.5 else 'black'
                ax.text(start_time + duration / 2, y_pos, f"B={task['B']}",
                        ha='center', va='center', color=text_color, fontsize=9, fontweight='bold')

            # 格式化图表
            ax.set_xlabel("Time (ms)", fontsize=12)
            ax.set_ylabel("User ID", fontsize=12)
            ax.set_title(f"NPU Timeline - Server {server_id} / NPU {npu_id}", fontsize=16, fontweight='bold')

            # 设置Y轴刻度和标签
            ax.set_yticks(range(len(user_ids_on_this_npu)))
            ax.set_yticklabels([f'User {uid}' for uid in user_ids_on_this_npu], fontsize=10)
            ax.invert_yaxis()

            ax.grid(True, which='major', axis='x', linestyle='--', linewidth=0.5)
            
            # 创建图例，并去重
            handles, labels = ax.get_legend_handles_labels()
            by_label = dict(zip(labels, handles))
            ax.legend(by_label.values(), by_label.keys(), title="User ID")

            fig.tight_layout()

            # 保存图表
            filename = f'./pics/npu_S{server_id}_N{npu_id}_gantt_chart.png'
            plt.savefig(filename)
            plt.close(fig)
            print(f"Saved chart for NPU (S{server_id}, N{npu_id}) to {filename}")
    
    def _plot_user_gantt_charts(self, user_progress):
        """
        为每个用户生成并保存一个甘特图，展示批处理任务的时间线。
        """
        print("\nGenerating Gantt charts for each user...")
        
        # 为不同NPU分配一个稳定的颜色
        all_npu_keys = sorted(list(self.npu_busy_time.keys()))
        colors = list(mcolors.TABLEAU_COLORS.values())
        npu_color_map = {npu_key: colors[i % len(colors)] for i, npu_key in enumerate(all_npu_keys)}

        for user_id in range(1, self.M + 1):
            requests = user_progress[user_id].get('completed_requests')
            if not requests:
                print(f"User {user_id} has no completed requests to plot. Skipping.")
                continue

            fig, ax = plt.subplots(figsize=(20, 10))

            # Y轴代表不同的NPU
            # 对该用户使用过的NPU进行排序和映射
            used_npu_keys = sorted(list(set(req['npu_key'] for req in requests)))
            npu_to_y = {npu: i for i, npu in enumerate(used_npu_keys)}

            # 绘制每一个请求的条形
            for req in requests:
                start_time = req['start_time']
                duration = req['completion_time'] - start_time
                npu_key = req['npu_key']
                y_pos = npu_to_y[npu_key]
                
                ax.barh(y=y_pos, width=duration, left=start_time, height=0.6,
                        label=f"NPU {npu_key}", 
                        color=npu_color_map[npu_key], 
                        edgecolor='black')
                
                # 在条形中间添加文字，显示批次大小
                text_color = 'white' if sum(mcolors.to_rgb(npu_color_map[npu_key])) < 1.5 else 'black'
                ax.text(start_time + duration / 2, y_pos, f"B={req['B']}", 
                        ha='center', va='center', color=text_color, fontsize=9, fontweight='bold')

            # 格式化图表
            ax.set_xlabel("Time (ms)", fontsize=12)
            ax.set_ylabel("Server / NPU", fontsize=12)
            ax.set_title(f"User {user_id} - Batch Processing Gantt Chart", fontsize=16, fontweight='bold')

            # 设置Y轴刻度和标签
            ax.set_yticks(range(len(used_npu_keys)))
            ax.set_yticklabels([f'Srv{k[0]}/NPU{k[1]}' for k in used_npu_keys], fontsize=10)
            ax.invert_yaxis()  # 让第一个NPU在顶部

            ax.grid(True, which='major', axis='x', linestyle='--', linewidth=0.5)
            fig.tight_layout()

            # 保存图表到文件
            filename = f'./pics/user_{user_id}_gantt_chart.png'
            plt.savefig(filename)
            plt.close(fig)  # 关闭图形，防止在内存中累积
            print(f"Saved chart for User {user_id} to {filename}")

    def _calculate_score(self, user_progress):
        total_score = 0
        K = 0
        
        # h(x) = 2^(-x/100)
        def h(x):
            return 2**(-x / 100.0)
        
        # p(x) = 2^(-x/200)
        def p(x):
            return 2**(-x / 200.0)

        for user_id in range(1, self.M + 1):
            user = self.user_map[user_id]
            schedule = self.schedules[user_id]
            end_i = user_progress[user_id]['end_time']
            
            # 计算迁移次数
            move_i = 0
            for j in range(len(schedule) - 1):
                npu1 = (schedule[j]['server_id'], schedule[j]['npu_id'])
                npu2 = (schedule[j+1]['server_id'], schedule[j+1]['npu_id'])
                if npu1 != npu2:
                    move_i += 1
            
            if end_i > user['e']:
                K += 1
            
            h_time_factor = (end_i - user['e']) / (user['e'] - user['s'])
            total_score += h(h_time_factor) * p(move_i) * 10000

        final_score = h(K) * total_score
        
        with open("result", 'w') as file:
            print("="*20 + " Scoring Result " + "="*20, file=file)
            print(f"Final Score: {final_score:.4f}", file=file)
            print(f"Total Users Missing Deadline (K): {K}", file=file)
            print("-" * 56, file=file)
            print(f"{'User ID':<10}{'End Time':<12}{'Deadline (e_i)':<15}{'Migrations':<12}", file=file)
            for user_id in range(1, self.M + 1):
                user = self.user_map[user_id]
                end_i = user_progress[user_id]['end_time']
                move_i = sum(1 for j in range(len(self.schedules[user_id]) - 1) if (self.schedules[user_id][j]['server_id'], self.schedules[user_id][j]['npu_id']) != (self.schedules[user_id][j+1]['server_id'], self.schedules[user_id][j+1]['npu_id']))
                print(f"{user_id:<10}{end_i:<12}{user['e']:<15}{move_i:<12}", file=file)
            print("="*56, file=file)

            # 1. 详细的用户请求报告
            print("\n" + "="*25 + " Detailed User Report " + "="*25, file=file)
            for user_id in range(1, self.M + 1):
                print(f"\n--- User {user_id} ---", file=file)
                completed_reqs = sorted(user_progress[user_id]['completed_requests'], key=lambda r: r['send_time'])
                header = f"{'Req#':<5}{'SendT':<8}{'Srv/NPU':<10}{'Batch':<7}{'ArrivalT':<10}{'StartT':<8}{'EndT':<8}{'Duration':<10}"
                print(header, file=file)
                print('-' * len(header), file=file)
                for idx, req in enumerate(completed_reqs):
                    duration = req['completion_time'] - req['start_time']
                    npu_str = f"{req['npu_key'][0]}/{req['npu_key'][1]}"
                    print(f"{idx+1:<5}{req['send_time']:<8}{npu_str:<10}{req['B']:<7}{req['arrival_time']:<10}{req['start_time']:<8}{req['completion_time']:<8}{duration:<10}", file=file)

            # 2. NPU利用率报告
            print("\n" + "="*25 + " NPU Utilization Report " + "="*24, file=file)
            print(f"Total Simulation Time: {self.total_simulation_time} ms\n", file=file)
            header = f"{'Server':<8}{'NPU':<5}{'Busy Time (ms)':<18}{'Utilization (%)':<15}"
            print(header, file=file)
            print('-' * len(header), file=file)
            
            sorted_npu_keys = sorted(self.npu_busy_time.keys())
            for npu_key in sorted_npu_keys:
                busy_time = self.npu_busy_time[npu_key]
                server_id, npu_id_val = npu_key
                util_percent = (busy_time / self.total_simulation_time) * 100 if self.total_simulation_time > 0 else 0
                print(f"{server_id:<8}{npu_id_val:<5}{busy_time:<18}{util_percent:.2f}", file=file)
            print("="*74, file=file)

    def _print_errors(self):
        print("="*20 + " Validation Failed " + "="*20, file=sys.stderr)
        for error in self.errors:
            print(f"[ERROR] {error}", file=sys.stderr)
        print("="*57, file=sys.stderr)


if __name__ == '__main__':
    if len(sys.argv) != 3:
        print("Usage: python scorer.py <input_file_path> <output_file_path>")
        sys.exit(1)
        
    input_file = sys.argv[1]
    output_file = sys.argv[2]
    # input_file = "D:\VScodeFile\C++\HUAWEI\data\data.in"
    # output_file = "D:\VScodeFile\C++\HUAWEI\src\out.txt"


    scorer = Scorer(input_path=input_file, output_path=output_file)
    scorer.run()