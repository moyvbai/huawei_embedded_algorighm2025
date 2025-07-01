# validator.py
# 华为嵌入式软件大赛算法组2025区域初赛
# 本地验证与评分脚本 (新增显存日志记录功能)
#
# 更新日志:
# - [新增] 显存日志: 记录每个NPU在每个时刻的显存占用，并保存到 memory_log.csv 文件。
# - [优化] 模拟循环: 采用事件驱动的NPU检查机制，显著提升模拟速度。
# - [新增] 使用 tqdm 库为模拟过程提供实时进度条。
# - [修正] 评分逻辑: 提前完成任务会获得奖励分数。
# - [修正] 校验逻辑: 在解析阶段增加对用户连续发送时间间隔的“快速失败”检查。
# - [修正] 校验逻辑: 增加对用户请求发送时间不得超过 1,000,000 的约束。
# - [重构] 模拟逻辑: NPU现在可以并行处理多个请求，每个请求独立计算耗时与完成时间。

import sys
import math
import collections
import csv # 【新增】导入csv模块用于写日志文件
from tqdm import tqdm

# --- 数据结构定义 (无变化) ---
Server = collections.namedtuple('Server', ['id', 'npu_count', 'k', 'memory', 'npu_list'])

class NPU:
    def __init__(self, server_id, npu_id, memory_limit):
        self.server_id, self.id, self.memory_limit = server_id, npu_id, memory_limit
        self.used_memory, self.queue, self.running_tasks = 0, [], []
    def __repr__(self):
        return f"NPU(Srv{self.server_id}-NPU{self.id})"

class User:
    def __init__(self, id, s, e, cnt):
        self.id, self.s, self.e, self.cnt_required = id, s, e, cnt
        self.cnt_processed, self.requests, self.last_npu_global_id = 0, [], -1
        self.migrations, self.finish_time, self.next_allowed_send_time = 0, -1, s
    def __repr__(self):
        return f"User(id={self.id}, required={self.cnt_required}, processed={self.cnt_processed})"

Request = collections.namedtuple('Request', ['user_id', 'server_id', 'npu_id', 'batch_size', 'send_time', 'arrival_time'])

# --- 错误处理 (无变化) ---
def fail_with_error(error_type, message):
    print(f"\nValidation Failed: [{error_type}]", file=sys.stderr)
    print(f"Details: {message}", file=sys.stderr)
    exit(1)

# --- 核心模拟与评分逻辑 ---
class Simulator:
    def __init__(self, input_path, output_path):
        self.servers, self.users, self.latencies = [], {}, {}
        self.mem_a, self.mem_b = 0, 0
        self.npu_map = {}
        self.total_samples_to_process = 0
        
        # 【新增】初始化内存日志记录器
        self.memory_log = []

        self.parse_input(input_path)
        self.parse_output(output_path)
        self.events = collections.defaultdict(list)
        self.populate_initial_events()

    # parse_input, parse_output, populate_initial_events 方法与上一版完全相同...
    def parse_input(self, file_path):
        print("--- 1. Parsing Input File ---")
        try:
            with open(file_path, 'r') as f:
                lines = [line.strip() for line in f.readlines()]
                n_servers = int(lines[0])
                npu_global_id_counter = 0
                for i in range(n_servers):
                    g, k, m = map(int, lines[1 + i].split())
                    server_id = i + 1
                    npu_list = [NPU(server_id, j + 1, m) for j in range(g)]
                    for npu in npu_list:
                        self.npu_map[npu_global_id_counter] = npu
                        npu_global_id_counter += 1
                    self.servers.append(Server(server_id, g, k, m, list(range(npu_global_id_counter - g, npu_global_id_counter))))
                offset = 1 + n_servers
                m_users = int(lines[offset])
                for i in range(m_users):
                    s, e, cnt = map(int, lines[offset + 1 + i].split())
                    user_id = i + 1
                    self.users[user_id] = User(user_id, s, e, cnt)
                    self.total_samples_to_process += cnt
                offset += 1 + m_users
                for i in range(n_servers):
                    server_id = i + 1
                    lat_list = map(int, lines[offset + i].split())
                    for j, lat in enumerate(lat_list):
                        self.latencies[(j + 1, server_id)] = lat
                self.mem_a, self.mem_b = map(int, lines[-1].split())
        except (IOError, IndexError, ValueError) as e:
            fail_with_error("Input File Error", f"Failed to read or parse input file '{file_path}': {e}")
        print("Input parsing complete.")

    def parse_output(self, file_path):
        print("--- 2. Parsing Output File ---")
        try:
            with open(file_path, 'r') as f:
                lines = [line.strip() for line in f.readlines()]
                if len(lines) != 2 * len(self.users):
                    fail_with_error("Invalid Output Format", f"Expected {2 * len(self.users)} lines, but got {len(lines)}.")
                for i in range(len(self.users)):
                    user_id, user = i + 1, self.users[i + 1]
                    t_i = int(lines[2 * i])
                    if not (1 <= t_i <= 300):
                        fail_with_error("Invalid Output Constraint", f"User {user_id}: T_i={t_i} is not in range [1, 300].")
                    parts = list(map(int, lines[2 * i + 1].split()))
                    if len(parts) != 4 * t_i:
                        fail_with_error("Invalid Output Format", f"User {user_id}: Expected {4*t_i} integers, but found {len(parts)}.")
                    user_total_samples, next_allowed_send_time = 0, user.s 
                    for j in range(t_i):
                        time, server_idx, npu_idx, b = parts[j*4:(j+1)*4]
                        if time > 1000000:
                            fail_with_error("Invalid User Send Time", f"User {user_id} request {j+1}: send time {time} exceeds the 1,000,000 limit.")
                        if time < next_allowed_send_time:
                             fail_with_error("Invalid User Send Time", f"User {user_id} request {j+1}: send time {time} is earlier than the allowed time {next_allowed_send_time}.")
                        if not (1 <= server_idx <= len(self.servers)):
                            fail_with_error("Invalid Server Index", f"User {user_id} request {j+1}: server index {server_idx} is out of bounds.")
                        server = self.servers[server_idx - 1]
                        if not (1 <= npu_idx <= server.npu_count):
                            fail_with_error("Invalid NPU Index", f"User {user_id} request {j+1}: NPU index {npu_idx} is out of bounds for server {server_idx}.")
                        if self.mem_a * b + self.mem_b > server.memory:
                            fail_with_error("Batchsize Exceeds Memory", f"User {user_id} request {j+1}: batch size {b} exceeds server {server_idx}'s memory.")
                        user_total_samples += b
                        arrival_time = time + self.latencies[(user_id, server_idx)]
                        user.requests.append(Request(user_id, server_idx, npu_idx, b, time, arrival_time))
                        next_allowed_send_time = arrival_time + 1
                    if user_total_samples != user.cnt_required:
                        fail_with_error("Samples Not Fully Processed", f"User {user_id}: Total batch sizes sum to {user_total_samples}, but {user.cnt_required} were required.")
        except (IOError, IndexError, ValueError) as e:
            fail_with_error("Output File Error", f"Failed to read or parse output file '{file_path}': {e}")
        print("Output parsing and validation complete.")

    def populate_initial_events(self):
        for user in self.users.values():
            for req in user.requests:
                self.events[req.send_time].append(('SEND', req))

    def run(self):
        print("--- 3. Starting Simulation (with Memory Logging) ---")
        current_time = 0
        processed_samples = 0
        server_npu_map = {(s.id, i + 1): self.npu_map[npu_global_id] for s in self.servers for i, npu_global_id in enumerate(s.npu_list)}
        npus_to_re_evaluate = set()

        with tqdm(total=self.total_samples_to_process, desc="Simulating", unit=" samples") as pbar:
            while processed_samples < self.total_samples_to_process:
                if current_time > 2000000:
                     fail_with_error("Simulation Timeout", "Simulation exceeded maximum time limit (2,000,000ms).")
                
                npus_to_re_evaluate.clear()

                # 1. 处理完成的任务
                for npu in self.npu_map.values():
                    finished_tasks = [task for task in npu.running_tasks if task['finish_time'] == current_time]
                    if finished_tasks:
                        npus_to_re_evaluate.add(npu)
                        for task in finished_tasks:
                            req, user = task['request'], self.users[task['request'].user_id]
                            samples_done = req.batch_size
                            user.cnt_processed += samples_done
                            processed_samples += samples_done
                            pbar.update(samples_done)
                            if user.cnt_processed == user.cnt_required:
                                user.finish_time = current_time
                            npu.used_memory -= task['memory_used']
                            npu.running_tasks.remove(task)

                # 2. 处理事件
                if current_time in self.events:
                    for event_type, payload in self.events.pop(current_time):
                        if event_type == 'SEND':
                            req, user = payload, self.users[payload.user_id]
                            if req.send_time < user.next_allowed_send_time:
                                fail_with_error("Invalid User Send Time (Runtime Check)", f"User {user.id} violation at t={req.send_time}.")
                            user.next_allowed_send_time = req.arrival_time + 1
                            server, npu_global_id = self.servers[req.server_id - 1], self.servers[req.server_id - 1].npu_list[req.npu_id - 1]
                            if user.last_npu_global_id != -1 and user.last_npu_global_id != npu_global_id:
                                user.migrations += 1
                            user.last_npu_global_id = npu_global_id
                            self.events[req.arrival_time].append(('ARRIVE', req))
                        elif event_type == 'ARRIVE':
                            npu = server_npu_map[(payload.server_id, payload.npu_id)]
                            npu.queue.append(payload)
                            npus_to_re_evaluate.add(npu)

                # 3. 检查并启动新任务
                for npu in npus_to_re_evaluate:
                    npu.queue.sort(key=lambda r: (r.arrival_time, r.user_id))
                    for req in list(npu.queue):
                        mem_needed = self.mem_a * req.batch_size + self.mem_b
                        if npu.used_memory + mem_needed <= npu.memory_limit:
                            npu.used_memory += mem_needed
                            npu.queue.remove(req)
                            server = self.servers[npu.server_id - 1]
                            if req.batch_size <= 0: continue
                            inference_speed = server.k * math.sqrt(req.batch_size)
                            time_needed = math.ceil(req.batch_size / inference_speed) if inference_speed > 0 else 0
                            npu.running_tasks.append({'request': req, 'finish_time': current_time + time_needed, 'memory_used': mem_needed})
                
                # 【新增】在每个时间点结束时，记录所有NPU的显存占用
                for npu_global_id, npu in self.npu_map.items():
                    self.memory_log.append({
                        'time': current_time,
                        'npu_global_id': npu_global_id,
                        'server_id': npu.server_id,
                        'npu_local_id': npu.id,
                        'used_memory': npu.used_memory,
                        'max_memory': npu.memory_limit
                    })

                pbar.set_postfix_str(f"Time: {current_time}ms")
                current_time += 1
        
        print(f"\nSimulation Finished at time: {current_time-1} ms.")

    # calculate_score 方法与上一版完全相同...
    def calculate_score(self):
        print("--- 4. Calculating Score ---")
        total_score, late_users_count = 0, 0
        for user in self.users.values():
            if user.finish_time > user.e:
                late_users_count += 1
            if user.finish_time == -1:
                fail_with_error("Scoring Error", f"User {user.id} did not finish. This indicates a simulation logic error.")
            h_arg = 0
            if user.e > user.s:
                h_arg = (user.finish_time - user.e) / (user.e - user.s)
            elif user.finish_time > user.e:
                 h_arg = float('inf')
            h_val = 2**(-h_arg / 100.0)
            p_val = 2**(-user.migrations / 200.0)
            total_score += h_val * p_val * 10000

        k_penalty = 2**(-late_users_count / 100.0)
        final_score = k_penalty * total_score
        print("\n--- Scoring Summary ---")
        print(f"Total Users: {len(self.users)}")
        print(f"Late Users (K): {late_users_count}")
        print(f"K Penalty h(K): {k_penalty:.4f}")
        print(f"Sum of User Scores (before K penalty): {total_score:.2f}")
        print("-------------------------")
        print(f"FINAL SCORE: {final_score:.4f}")
        print("-------------------------")
    
    # 【新增】保存显存日志到CSV文件的方法
    def save_memory_log(self, filepath="memory_log.csv"):
        """将记录的显存使用数据保存到CSV文件"""
        print(f"--- 5. Saving Memory Log ---")
        if not self.memory_log:
            print("No memory usage data was recorded.")
            return

        # 定义CSV文件的表头
        fieldnames = ['time', 'npu_global_id', 'server_id', 'npu_local_id', 'used_memory', 'max_memory']
        
        try:
            with open(filepath, 'w', newline='', encoding='utf-8') as f:
                writer = csv.DictWriter(f, fieldnames=fieldnames)
                writer.writeheader() # 写入表头
                writer.writerows(self.memory_log) # 批量写入数据
            print(f"Memory log successfully saved to '{filepath}'")
        except IOError as e:
            print(f"Error saving memory log to '{filepath}': {e}", file=sys.stderr)


if __name__ == "__main__":
    try:
        from tqdm import tqdm
    except ImportError:
        print("Error: 'tqdm' library not found. Please install it using 'pip install tqdm'", file=sys.stderr)
        sys.exit(1)

    if len(sys.argv) != 3:
        print("\nUsage: python validator.py <path_to_input_file> <path_to_output_file>")
        print("Example: python validator.py ./input.txt ./output.txt\n")
        sys.exit(1)

    try:
        simulator = Simulator(sys.argv[1], sys.argv[2])
        simulator.run()
        simulator.calculate_score()
        simulator.save_memory_log() #【新增】在所有操作完成后调用保存日志方法
    except SystemExit as e:
        if e.code != 1: raise
    except Exception as e:
        print(f"\nAn unexpected error occurred: {e}", file=sys.stderr)
        import traceback
        traceback.print_exc()