#include <bits/stdc++.h>
#include <iostream>
#include <vector>
#include <string>
#include <numeric>
#include <algorithm>
#include <cmath>
#include <memory>
#include <map>
#include <list>
#include <cstdio>
#include <chrono>
#include <queue>
#include <set>


using arr2 = std::array<int, 2>;
using arr3 = std::array<int, 3>;
using arr4 = std::array<int, 4>;

// ===================================================================
// START: Logger System
// ===================================================================
// #define DEBUG
#ifdef DEBUG
    #define LOG(...) do { \
        fprintf(stderr, "[DEBUG] "); \
        fprintf(stderr, __VA_ARGS__); \
        fprintf(stderr, "\n"); \
    } while(0)
#else
    #define LOG(...) do {} while(0)
#endif
// ===================================================================
// END: Logger System
// ===================================================================


// ===================================================================
// START: Data Structures & Classes
// ===================================================================

struct NPU {
    int server_id;
    int npu_id;
    int k;
    int memory;

    int calculateInferenceTime (int batchSize) {
        return ceil(sqrtl(batchSize) / k);
    }
};

struct User {
    int id;
    int s;
    int e;
    int cnt;
};

struct Schedule {
    int time;
    int server_id;
    int npu_id;
    int batch_size;
};

class ProblemData {
public:
    int n_servers;
    int m_users;
    std::vector<std::vector<NPU>> npus; 
    std::vector<User> users;
    std::vector<std::vector<int>> latency;
    int mem_a, mem_b;

    void read(std::istream& in) {
        in >> n_servers;
        npus.resize(n_servers + 1);
        for (int i = 1; i <= n_servers; ++i) { 
            int g, k, m; 
            in >> g >> k >> m;
            npus[i].resize(g + 1);
            for (int j = 1; j <= g; j++) {
                npus[i][j].server_id = i;
                npus[i][j].npu_id = j;
                npus[i][j].k = k;
                npus[i][j].memory = m;
            }
        }

        in >> m_users;
        users.resize(m_users + 1);
        for (int i = 1; i <= m_users; ++i) { 
            users[i].id = i; 
            in >> users[i].s >> users[i].e >> users[i].cnt; 
        }

        latency.assign(n_servers + 1, std::vector<int>(m_users + 1));
        for (int i = 1; i <= n_servers; ++i) { 
            for (int j = 1; j <= m_users; ++j) { 
                in >> latency[i][j]; 
            } 
        }
        in >> mem_a >> mem_b;
    }
};

struct SolverResult {
    std::vector<std::vector<Schedule> > solution;
    int completed_user_count = 0; 
};

class Solver {
public:
    virtual std::string name() const = 0;
    virtual SolverResult solve(ProblemData& data) = 0;
    virtual ~Solver() = default;

    static void print_solution(const std::vector<std::vector<Schedule>>& solution, int m_users) {
        if (solution.empty() || solution.size() <= 1) return;
        for (int i = 1; i <= m_users; ++i) {
            const auto& user_schedule = solution[i];
            if (user_schedule.empty()) {
                std::cout << 0 << "\n\n";
                continue;
            }
            std::cout << user_schedule.size() << "\n";
            for (size_t j = 0; j < user_schedule.size(); ++j) {
                const auto& sch = user_schedule[j];
                std::cout << sch.time << " " << sch.server_id << " " << sch.npu_id << " " << sch.batch_size << (j == user_schedule.size() - 1 ? "" : " ");
            }
            std::cout << "\n";
        }
    }
};

struct NpuSimulationResult {
    std::map<int, std::vector<Schedule>> schedules; 
    std::vector<int> completed_users;
    std::vector<int> timeout_users;
    std::map<int, int> remaining_samples;
    std::vector<int> memory_usage; 
    int finish_time;
};


// ===================================================================
// START: Solver Implementations
// ===================================================================

class SimpleSolver : public Solver {
public:
    std::string name() const override { return "SimpleSolver (Fixed Target)"; }
    SolverResult solve(ProblemData& data) override {
        LOG("Running %s...", name().c_str());
        SolverResult result;
        result.solution.resize(data.m_users + 1);

        // 目标固定为服务器1, NPU 1
        const int TARGET_SERVER_ID = 1;
        const int TARGET_NPU_ID = 1;
        
        if (data.n_servers < TARGET_SERVER_ID || data.npus[TARGET_SERVER_ID].size() <= TARGET_NPU_ID) {
            LOG("Error: Target NPU (1,1) does not exist.");
            return result;
        }
        const auto& target_npu = data.npus[TARGET_SERVER_ID][TARGET_NPU_ID];

        // 计算最大批次大小
        int max_bs = (target_npu.memory - data.mem_b) / data.mem_a;
        max_bs = std::min(max_bs, 1000);
        if (max_bs <= 0) max_bs = 1;

        // 为每个用户独立生成调度方案
        for (int user_id = 1; user_id <= data.m_users; ++user_id) {
            const auto& user = data.users[user_id];
            std::vector<Schedule> user_schedules;
            int remaining_samples = user.cnt;
            int user_can_send_time = user.s;

            // “能发就发”逻辑
            while(remaining_samples > 0) {
                int batch_size = std::min(remaining_samples, max_bs);
                int send_time = user_can_send_time;
                
                user_schedules.push_back({send_time, TARGET_SERVER_ID, TARGET_NPU_ID, batch_size});
                
                // 更新该用户下一个可发送时间
                int latency = data.latency[TARGET_SERVER_ID][user_id];
                int arrival_time = send_time + latency;
                user_can_send_time = arrival_time + 1;

                remaining_samples -= batch_size;
            }
            result.solution[user_id] = user_schedules;
        }
        result.completed_user_count = 1;

        return result;
    }
};


class TwoBlockSolver : public Solver {
public:
    std::string name() const override { return "TwoBlockSolver"; }
    
    NpuSimulationResult simulate(NPU& npu, ProblemData& data, const std::vector<int>& simulate_users) {
        // LOG("runing simluate ...");
        int M = data.m_users, N = data.n_servers, A = data.mem_a, B = data.mem_b;
        int max_time = 6e4, server_id = npu.server_id, npu_id = npu.npu_id;
        int max_batch_size = (npu.memory - B) / A, memory = npu.memory;
        std::vector<User>& users = data.users;
        std::vector<std::vector<int>>& latency = data.latency;

        NpuSimulationResult result;
        std::map<int, std::vector<Schedule>>& schedules = result.schedules; 
        std::vector<int>& completed_users = result.completed_users;
        std::vector<int>& timeout_users = result.timeout_users;
        std::map<int, int>& remaining_samples = result.remaining_samples;
        std::vector<int>& memory_usage = result.memory_usage; 
        
        memory_usage.resize(max_time + 100, 0);
        
        for (auto& user_id: simulate_users) {
            remaining_samples[user_id] = data.users[user_id].cnt;
        }

        int block_size = (npu.memory / 2 - B) / A;
        int blockTime = npu.calculateInferenceTime(block_size);

        std::vector<int> max_small_block_count(M + 1, 0);

        // 提前划分好块
        for (int userId: simulate_users) {
            int mx = ceil(1.0 * remaining_samples[userId] / block_size);
            for (int i = mx; i >= 0; i --) {
                int j = (remaining_samples[userId] - i * block_size) / max_batch_size;
                int res = remaining_samples[userId] - j * max_batch_size - i * block_size;
                if (res > 0) j += 1;
                if (i + j > 300) continue;
                else {
                    max_small_block_count[userId] = i;
                    break;
                }
            }
        }


        // --- 3. 时间步进模拟 ---
        // first: 到达时间, second: 用户ID
        std::priority_queue<arr2, std::vector<arr2>, std::greater<arr2>> waiting_users; 
        // first: 优先级 (此处用剩余样本数作为简单优先级), second: 用户ID
        std::priority_queue<arr2, std::vector<arr2>, std::greater<arr2>> available_users; 

        for (int user_id: simulate_users) {
            waiting_users.push({data.users[user_id].s + data.latency[server_id][user_id], user_id});
        }

        for (int current_time = 0; current_time <= max_time; current_time++) {
            // 将到达的用户放入可用队列
            while (!waiting_users.empty() && waiting_users.top()[0] <= current_time) {
                int user_id = waiting_users.top()[1];
                waiting_users.pop();
                available_users.push({users[user_id].e, user_id}); // 使用初始样本数作为优先级
            }

            while(!available_users.empty()) {
                int free_memory = memory - memory_usage[current_time];
                int free_batch_size = (free_memory - B) / A;
                if (free_batch_size <= 0) break;

                int user_id = available_users.top()[1];
                available_users.pop();

                // 默认发送最大批次
                int batch_size = std::min(max_batch_size, remaining_samples[user_id]);

                // --- 启发式决策：是否改为发送小块 ---
                int small_block_time_cost = npu.calculateInferenceTime(block_size);
                int small_block_count = remaining_samples[user_id] / block_size;
                int remaing_block = remaining_samples[user_id] - small_block_count * block_size;
                int remaing_block_time_cost = npu.calculateInferenceTime(remaing_block);
                int estimated_total_time_cost = std::max(small_block_time_cost, data.latency[server_id][user_id]) * small_block_count;
                if (remaing_block > 0) estimated_total_time_cost += remaing_block_time_cost;

                bool has_small_block_quota = max_small_block_count[user_id] > 0;
                bool small_block_is_safe = (current_time + 2 * estimated_total_time_cost) <= data.users[user_id].e;
                bool can_send_small_now = (current_time + npu.calculateInferenceTime(std::min(block_size, remaining_samples[user_id]))) <= data.users[user_id].e;
                
                bool should_send_small_block = false;
                if (has_small_block_quota && small_block_is_safe && can_send_small_now) {
                    bool is_last_block = remaining_samples[user_id] <= block_size;
                    bool has_enough_free_memory_for_small = free_batch_size >= block_size;
                    bool has_enough_free_memory_for_big = free_batch_size >= max_batch_size;
                    bool other_users_waiting = available_users.size() >= 4;
                    

                    if (has_enough_free_memory_for_small && (other_users_waiting || (!has_enough_free_memory_for_big))) {
                        should_send_small_block = true;
                    }
                }
                
                if (should_send_small_block) {
                    batch_size = std::min(block_size, remaining_samples[user_id]);
                    max_small_block_count[user_id]--;
                }


                int handle_time = npu.calculateInferenceTime(batch_size);
                int end_time = current_time + handle_time;
                // 如果当前空闲内存不足以发送大块，但本来计划发送大块，则等待
                if (batch_size > free_batch_size or end_time > max_time) {
                    waiting_users.push({current_time, user_id}); // 放回队列等待
                    continue; // 当前tick无法再分配，检查下一个tick
                }

                
                int send_time = current_time - data.latency[server_id][user_id];
                schedules[user_id].push_back({send_time, server_id, npu_id, batch_size});
                remaining_samples[user_id] -= batch_size;

                // 占用内存
                if (batch_size <= block_size) batch_size = block_size;
                else batch_size = max_batch_size;
                
                int memory_to_occupy = A * batch_size + B;
                for (int t = current_time; t < current_time + npu.calculateInferenceTime(batch_size); t++) {
                    memory_usage[t] += memory_to_occupy;
                }

                // 检查用户是否还有后续请求
                if (remaining_samples[user_id] <= 0) {
                    if (current_time + handle_time <= users[user_id].e) {
                        completed_users.push_back(user_id);
                        remaining_samples.erase(user_id);
                    }
                } else {
                    waiting_users.push({current_time + data.latency[server_id][user_id] + 1, user_id}); // 简化：下一个tick即可再次请求
                }
            }
        }

        // --- 4. 最终统计 ---
        for(auto& v : remaining_samples) {
            timeout_users.push_back(v.first);
        }
        
        return result;
    }
    
    SolverResult solve(ProblemData& data) override {
        LOG("Running %s...", name().c_str());
        
        std::vector<User>& users = data.users;
        std::vector<std::vector<NPU> >& npus = data.npus;
        int M = data.m_users, N = data.n_servers, A = data.mem_a, B = data.mem_b;
        
        SolverResult result;
        result.solution.resize(data.m_users + 1);
        
        std::vector<int> user_order;
        for(int i = 1; i <= data.m_users; i ++) {
            user_order.push_back(i);
        }

        std::sort(user_order.begin(), user_order.end(), [&](int u1, int u2) {
            return data.users[u1].cnt < data.users[u2].cnt;
        });

        
        std::vector<std::vector<std::vector<int> > > simulate_users(data.n_servers + 1);
        std::vector<std::vector<NpuSimulationResult> > simulate_result(data.n_servers + 1);
        for (int i = 1; i <= data.n_servers; i ++) {
            simulate_users[i].resize(data.npus[i].size());
            simulate_result[i].resize(data.npus[i].size());
        }


        int server_id = 1, npu_id = 1;
        for (auto& user_id: user_order) {
            simulate_users[server_id][npu_id].push_back(user_id);

            npu_id += 1;
            if (npu_id >= (int)data.npus[server_id].size()) {
                server_id += 1;
                npu_id = 1;
                if (server_id > data.n_servers) {
                    server_id = 1;
                }
            }
        }

        std::set<int> completed_users;
        LOG("prepare simulate!");
        LOG("user count: %d", simulate_users[1][1].size());
        LOG("%d", data.npus[1].size());
        for (int i = 1; i <= data.n_servers; i ++) {
            for (int j = 1; j < (int)data.npus[i].size(); j ++) {
                simulate_result[i][j] = simulate(npus[i][j], data, simulate_users[i][j]);                
                while ((int)simulate_result[i][j].timeout_users.size() != 0) {
                    simulate_users[i][j] = simulate_result[i][j].completed_users;
                    simulate_result[i][j] = simulate(npus[i][j], data, simulate_users[i][j]);
                }

                for (auto& user_id: simulate_result[i][j].completed_users) {
                    completed_users.insert(user_id);
                }
            }
        }

        std::vector<int> timeout_users;
        for (int i = 1; i <= data.m_users; i ++) {
            if (completed_users.count(i) == 0) {
                timeout_users.push_back(i);
            }
        }

        std::sort(timeout_users.begin(), timeout_users.end(), [&](int u1, int u2) {
            return users[u1].cnt < users[u2].cnt;
        });


        LOG("before iter, timeout users count: %d", (int)timeout_users.size());
        for (auto& user_id: timeout_users) {
            bool assign_success = false;
            for (int i = 1; i <= data.n_servers; i ++) {
                if (assign_success) break;
                for (int j = 1; j < data.npus[i].size(); j ++) {
                    if (assign_success) break;
                    std::vector<int> new_simulate_users = simulate_users[i][j];
                    new_simulate_users.push_back(user_id);
                    NpuSimulationResult res = simulate(npus[i][j], data, new_simulate_users);
                    if (res.completed_users.size() > simulate_result[i][j].completed_users.size()) {
                        simulate_users[i][j] = new_simulate_users;
                        simulate_result[i][j] = res;
                        assign_success = true;
                        // LOG("user %d iter success, current completed count: %d", user_id, res.completed_users.size());
                    }
                }
            }
        }
        
        std::vector<int> remaing_sample(data.m_users + 1, 0);
        for (int i = 1; i <= data.m_users; i ++) remaing_sample[i] = users[i].cnt;
        for (int i = 1; i <= data.n_servers; i ++) {
            for (int j = 1; j < data.npus[i].size(); j ++) {
                for (auto& user_id: simulate_users[i][j]) {
                    remaing_sample[user_id] = simulate_result[i][j].remaining_samples[user_id];
                    result.solution[user_id].insert(result.solution[user_id].end(), 
                        simulate_result[i][j].schedules[user_id].begin(), simulate_result[i][j].schedules[user_id].end());
                }
            }
        }

        LOG("handle timeout user!");
        for (int i = 1; i <= data.m_users; i ++) {
            if (remaing_sample[i] == 0) result.completed_user_count += 1;
            int start_time = 6e4 + 21;
            while (remaing_sample[i] > 0) {
                int batch_size = std::min(remaing_sample[i], ((npus[1][1].memory - B) / A));
                remaing_sample[i] -= batch_size;
                start_time += (21);
                result.solution[i].push_back({start_time, 1, 1, batch_size});
            }
        }

        LOG("finish users count: %d", result.completed_user_count);


        return result;
    }
};



class AutoBlockSolver : public Solver {
public:
    std::string name() const override { return "AutoBlockSolver"; }
    
    NpuSimulationResult simulate(NPU& npu, ProblemData& data, const std::vector<int>& simulate_users) {
        // LOG("runing simluate ...");
        int M = data.m_users, N = data.n_servers, A = data.mem_a, B = data.mem_b;
        int max_time = 6e4, server_id = npu.server_id, npu_id = npu.npu_id;
        int max_batch_size = (npu.memory - B) / A, memory = npu.memory;
        int max_block_count = npu.memory / (2 * B);
        std::vector<User>& users = data.users;
        std::vector<std::vector<int>>& latency = data.latency;

        NpuSimulationResult result;
        std::map<int, std::vector<Schedule>>& schedules = result.schedules; 
        std::vector<int>& completed_users = result.completed_users;
        std::vector<int>& timeout_users = result.timeout_users;
        std::map<int, int>& remaining_samples = result.remaining_samples;
        std::vector<int>& memory_usage = result.memory_usage; 
        memory_usage.resize(max_time + 100, 0);
        int& finish_time = result.finish_time;
        finish_time = 0;
        
        for (auto& user_id: simulate_users) {
            remaining_samples[user_id] = data.users[user_id].cnt;
        }

        std::vector<int> remaining_send_count(M + 1, 300);
        auto can_send = [&](int time, int user_id, int batch) {
            if (remaining_send_count[user_id] <= 0) return false;
            if (remaining_samples[user_id] - batch > (remaining_send_count[user_id] - 1) * max_batch_size) return false;
            int batch_handle_time = npu.calculateInferenceTime(batch);
            int cnt = remaining_samples[user_id] / batch;
            int res = remaining_samples[user_id] - (cnt) * batch;
            int process_time = std::max(batch_handle_time, latency[server_id][user_id]);
            if (res > 0) {
                process_time = cnt * process_time + npu.calculateInferenceTime(res);
            } else {
                process_time = std::max(cnt - 1, 0) * process_time + batch_handle_time; 
            }
            return time + 1 * process_time <= users[user_id].e;
        };


        // --- 3. 时间步进模拟 ---
        // first: 到达时间, second: 用户ID
        std::priority_queue<arr2, std::vector<arr2>, std::greater<arr2>> waiting_users; 
        // first: 优先级 (此处用剩余样本数作为简单优先级), second: 用户ID
        std::priority_queue<arr2, std::vector<arr2>, std::greater<arr2>> available_users; 

        for (int user_id: simulate_users) {
            waiting_users.push({data.users[user_id].s + data.latency[server_id][user_id], user_id});
        }

        for (int current_time = 0; current_time <= max_time; current_time++) {
            // LOG("current time: %d", current_time);
            // 将到达的用户放入可用队列
            while (!waiting_users.empty() && waiting_users.top()[0] <= current_time) {
                int user_id = waiting_users.top()[1];
                waiting_users.pop();
                available_users.push({ users[user_id].e, user_id}); // 使用初始样本数作为优先级
            }

            while(!available_users.empty()) {
                int free_memory = memory - memory_usage[current_time];
                int free_batch_size = (free_memory - B) / A;
                if (free_batch_size <= 0) break;

                int user_id = available_users.top()[1];
                available_users.pop();

                // LOG("user id: %d", user_id);
                // LOG("finish time: %d, current time: %d", finish_time, current_time);

                if (finish_time <= current_time) {
                    // LOG("finish time == current time");
                    // 根据优先级最高的用户和当前队列中用户数量来决定
                    int cnt = sqrtl(available_users.size() + 1);
                    for (int i = std::min(cnt, max_block_count); i >= 1; i --) {
                        // LOG("try %d block", i);
                        // 优先级最高的确定batch size
                        int block_size = (memory / i - B) / A;
                        int block_time = npu.calculateInferenceTime(block_size);
                        block_size = (block_time * npu.k) * (block_time * npu.k);
                        int batch_size = std::min(block_size, remaining_samples[user_id]);
                        batch_size = std::min(batch_size, free_batch_size);
                        int handle_time = npu.calculateInferenceTime(batch_size);

                        // LOG("batch size: %d", batch_size);
                        // LOG("can send: %d", can_send(user_id, batch_size));

                        // if (user_id == 118 and batch_size == 1) {
                        //     LOG("current time: %d", current_time);
                        //     LOG("can send: %d", can_send(current_time, user_id, batch_size));
                        //     LOG("remain count: %d", remaining_send_count[user_id]);
                        //     LOG("remain sample count: %d", remaining_samples[user_id]);
                        // }



                        if (can_send(current_time, user_id, batch_size)) {
                            int send_time = current_time - data.latency[server_id][user_id];
                            schedules[user_id].push_back({send_time, server_id, npu_id, batch_size});
                            finish_time = current_time + block_time;
                            remaining_samples[user_id] -= batch_size;
                            remaining_send_count[user_id] -= 1;
                            // 检查用户是否还有后续请求
                            if (remaining_samples[user_id] <= 0) {
                                if (current_time + handle_time <= users[user_id].e) {
                                    completed_users.push_back(user_id);
                                    remaining_samples.erase(user_id);
                                }
                            } else {
                                waiting_users.push({current_time + data.latency[server_id][user_id] + 1, user_id}); // 简化：下一个tick即可再次请求
                            }

                            // 维护内存占用
                            int memory_to_occupy = A * batch_size + B;
                            for (int t = current_time; t < current_time + handle_time; t++) {
                                memory_usage[t] += memory_to_occupy;
                            }
                            // assert(i <= 2);
                            break;
                        }
                        
                    }
                    
                    // LOG("finish time: %d", finish_time);
                    continue;
                }

                // 对于剩余用户，保证结束时间不超过finish time的前提下，能发就发
                int remaining_time = finish_time - current_time;
                int max_send_batch_size = (remaining_time * npu.k) * (remaining_time * npu.k);
                int batch_size = std::min(remaining_samples[user_id], max_send_batch_size);
                batch_size = std::min(batch_size, free_batch_size);

                if (can_send(current_time, user_id, batch_size)) {
                    int handle_time = npu.calculateInferenceTime(batch_size);
                    int send_time = current_time - data.latency[server_id][user_id];
                    schedules[user_id].push_back({send_time, server_id, npu_id, batch_size});
                    remaining_samples[user_id] -= batch_size;
                    remaining_send_count[user_id] -= 1;

                    // 检查用户是否还有后续请求
                    if (remaining_samples[user_id] <= 0) {
                        if (current_time + handle_time <= users[user_id].e) {
                            completed_users.push_back(user_id);
                            remaining_samples.erase(user_id);
                        }
                    } else {
                        waiting_users.push({current_time + data.latency[server_id][user_id] + 1, user_id}); // 简化：下一个tick即可再次请求
                    }

                    // 维护内存占用
                    int memory_to_occupy = A * batch_size + B;
                    for (int t = current_time; t < current_time + handle_time; t++) {
                        memory_usage[t] += memory_to_occupy;
                    }

                } else {
                    waiting_users.push({current_time, user_id}); // 简化：下一个tick即可再次请求
                }




            }
        }

        // --- 4. 最终统计 ---
        for(auto& v : remaining_samples) {
            timeout_users.push_back(v.first);
        }
        
        return result;
    }
    
    SolverResult solve(ProblemData& data) override {
        LOG("Running %s...", name().c_str());
        
        std::vector<User>& users = data.users;
        std::vector<std::vector<NPU> >& npus = data.npus;
        int M = data.m_users, N = data.n_servers, A = data.mem_a, B = data.mem_b;
        
        SolverResult result;
        result.solution.resize(data.m_users + 1);
        
        std::vector<int> user_order;
        for(int i = 1; i <= data.m_users; i ++) {
            user_order.push_back(i);
        }

        std::sort(user_order.begin(), user_order.end(), [&](int u1, int u2) {
            return data.users[u1].cnt < data.users[u2].cnt;
        });

        
        std::vector<std::vector<std::vector<int> > > simulate_users(data.n_servers + 1);
        std::vector<std::vector<NpuSimulationResult> > simulate_result(data.n_servers + 1);
        for (int i = 1; i <= data.n_servers; i ++) {
            simulate_users[i].resize(data.npus[i].size());
            simulate_result[i].resize(data.npus[i].size());
        }

        std::vector<int> timeout_users;
        for (int i = 1; i <= data.m_users; i ++) {
            timeout_users.push_back(i);
        }

        std::sort(timeout_users.begin(), timeout_users.end(), [&](int u1, int u2) {
            return users[u1].cnt < users[u2].cnt;
        });

        for (int i = M - 1; i >= 0; i --) {
            if (timeout_users[i] == 10) {
                LOG("user 10 position: %d", i);
                break;
            }
        }

        // LOG("user 10: %d", users[10].cnt);
        LOG("begin iter");
        int round = 10;
        while (round --) {
            LOG("round: %d", round);
            std::vector<int> new_timeout_users;
            int idx = 0, sz = timeout_users.size();
            int success_count = 0;
            int max_try_users_count = 500;
            while (idx < sz) {
                bool assign_success = false;
                // if (timeout_users[idx] == 10) {
                //     LOG("user 10 idx: %d", idx);
                // }
                for (int i = 1; i <= data.n_servers; i ++) {
                    if (assign_success) break;
                    for (int j = 1; j < data.npus[i].size(); j ++) {
                        if (assign_success) break;
                        std::vector<int> new_simulate_users = simulate_users[i][j];
                        int end = std::min(idx + max_try_users_count, (int)timeout_users.size());
                        new_simulate_users.insert(new_simulate_users.end(), 
                            timeout_users.begin() + idx, timeout_users.begin() + end);
                        NpuSimulationResult res = simulate(npus[i][j], data, new_simulate_users);
                        // LOG("simulate success! user count: %d, new user id: %d", new_simulate_users.size(), timeout_users[idx]);
                        if (res.timeout_users.size() == 0) {
                            // if (timeout_users[idx] == 10)
                            // LOG("simulate success! user count: %d, new user id: %d", new_simulate_users.size(), timeout_users[idx]);
                            simulate_users[i][j] = new_simulate_users;
                            simulate_result[i][j] = res;
                            assign_success = true;
                            success_count += (end - idx);
                        }
                    }
                }
                if (max_try_users_count == 1 and !assign_success) {
                    // LOG("assign fail, current idx: %d", idx);
                    new_timeout_users.push_back(timeout_users[idx]);
                    idx += 1;
                    max_try_users_count = 2;
                }


                if (assign_success) idx += max_try_users_count;
                else {
                    max_try_users_count = std::max(1, max_try_users_count / 2);
                }
            }
            
            timeout_users = new_timeout_users;
            std::sort(timeout_users.begin(), timeout_users.end(), [&](int u1, int u2) {
                return users[u1].cnt < users[u2].cnt;
            });
            LOG("success count: %d", success_count);
            if (success_count == 0) break;
        }

        
        std::vector<int> remaing_sample(data.m_users + 1, 0);
        for (int i = 1; i <= data.m_users; i ++) remaing_sample[i] = users[i].cnt;
        for (int i = 1; i <= data.n_servers; i ++) {
            for (int j = 1; j < data.npus[i].size(); j ++) {
                for (auto& user_id: simulate_users[i][j]) {
                    remaing_sample[user_id] = simulate_result[i][j].remaining_samples[user_id];
                    result.solution[user_id].insert(result.solution[user_id].end(), 
                        simulate_result[i][j].schedules[user_id].begin(), simulate_result[i][j].schedules[user_id].end());
                }
            }
        }

        LOG("handle timeout user!");
        for (int i = 1; i <= data.m_users; i ++) {
            if (remaing_sample[i] == 0) result.completed_user_count += 1;
            int start_time = 6e4 + 21;
            while (remaing_sample[i] > 0) {
                int batch_size = std::min(remaing_sample[i], ((npus[1][1].memory - B) / A));
                remaing_sample[i] -= batch_size;
                start_time += (21);
                result.solution[i].push_back({start_time, 1, 1, batch_size});
            }
        }

        LOG("finish users count: %d", result.completed_user_count);


        return result;
    }
};


class AutoBlockWithReverseFillSolver : public Solver {
public:
    std::string name() const override { return "AutoBlockWithReverseFillSolver"; }
    
    NpuSimulationResult simulate(NPU& npu, ProblemData& data, const std::vector<int>& simulate_users) {
        LOG("runing simluate ...");
        int M = data.m_users, N = data.n_servers, A = data.mem_a, B = data.mem_b;
        int max_time = 6e4, server_id = npu.server_id, npu_id = npu.npu_id;
        int max_batch_size = (npu.memory - B) / A, memory = npu.memory;
        int max_block_count = npu.memory / (2 * B);
        std::vector<User>& users = data.users;
        std::vector<std::vector<int>>& latency = data.latency;

        NpuSimulationResult result;
        std::map<int, std::vector<Schedule>>& schedules = result.schedules; 
        std::vector<int>& completed_users = result.completed_users;
        std::vector<int>& timeout_users = result.timeout_users;
        std::map<int, int>& remaining_samples = result.remaining_samples;
        std::vector<int>& memory_usage = result.memory_usage; 
        memory_usage.resize(max_time + 100, 0);
        int& finish_time = result.finish_time;
        finish_time = 0;
        
        for (auto& user_id: simulate_users) {
            remaining_samples[user_id] = data.users[user_id].cnt;
        }

        std::vector<int> remaining_send_count(M + 1, 300);
        auto can_send = [&](int user_id, int batch) {
            if (remaining_send_count[user_id] <= 0) return false;
            return remaining_samples[user_id] - batch <= (remaining_send_count[user_id] - 1) * max_batch_size; 
        };


        // --- 3. 时间步进模拟 ---
        // first: 到达时间, second: 用户ID
        std::priority_queue<arr2, std::vector<arr2>, std::greater<arr2>> waiting_users; 
        // first: 优先级 (此处用剩余样本数作为简单优先级), second: 用户ID
        std::set<arr2> available_users; 

        for (int user_id: simulate_users) {
            waiting_users.push({data.users[user_id].s + data.latency[server_id][user_id], user_id});
        }

        int global_block_time = 1;
        for (int current_time = 0; current_time <= max_time; current_time++) {
            // LOG("current time: %d", current_time);
            // 将到达的用户放入可用队列
            while (!waiting_users.empty() && waiting_users.top()[0] <= current_time) {
                int user_id = waiting_users.top()[1];
                waiting_users.pop();
                available_users.insert({ users[user_id].e, user_id}); // 使用初始样本数作为优先级
            }

            while(!available_users.empty()) {
                int free_memory = memory - memory_usage[current_time];
                int free_batch_size = (free_memory - B) / A;
                if (free_batch_size <= 0) break;

                auto it = available_users.begin();
                int user_id = available_users.begin()->at(1);

                // LOG("user id: %d", user_id);
                // LOG("finish time: %d, current time: %d", finish_time, current_time);

                if (finish_time <= current_time) {
                    // LOG("finish time == current time");
                    // 根据优先级最高的用户和当前队列中用户数量来决定
                    int cnt = sqrtl(available_users.size());
                    for (int i = std::min(cnt, max_block_count); i >= 1; i --) {
                        // LOG("try %d block", i);
                        // 优先级最高的确定batch size
                        int block_size = (memory / i - B) / A;
                        int block_time = npu.calculateInferenceTime(block_size);
                        block_size = (block_time * npu.k) * (block_time * npu.k);
                        int batch_size = std::min(block_size, remaining_samples[user_id]);
                        batch_size = std::min(batch_size, free_batch_size);
                        int handle_time = npu.calculateInferenceTime(batch_size);

                        // LOG("batch size: %d", batch_size);
                        // LOG("can send: %d", can_send(user_id, batch_size));

                        if (can_send(user_id, batch_size)) {
                            int send_time = current_time - data.latency[server_id][user_id];
                            schedules[user_id].push_back({send_time, server_id, npu_id, batch_size});
                            finish_time = current_time + block_time;
                            remaining_samples[user_id] -= batch_size;
                            remaining_send_count[user_id] -= 1;
                            global_block_time = block_time;
                            // 检查用户是否还有后续请求
                            if (remaining_samples[user_id] <= 0) {
                                if (current_time + handle_time <= users[user_id].e) {
                                    completed_users.push_back(user_id);
                                    remaining_samples.erase(user_id);
                                }
                            } else {
                                waiting_users.push({current_time + data.latency[server_id][user_id] + 1, user_id}); // 简化：下一个tick即可再次请求
                            }

                            // 维护内存占用
                            int memory_to_occupy = A * batch_size + B;
                            for (int t = current_time; t < current_time + handle_time; t++) {
                                memory_usage[t] += memory_to_occupy;
                            }

                            break;
                        }
                        
                    }
                    available_users.erase(it);
                    // LOG("finish time: %d", finish_time);
                    continue;
                }

                // 对于剩余用户，保证结束时间不超过finish time的前提下，能发就发
                int remaining_time = finish_time - current_time;
                if (remaining_time != global_block_time) {
                    // 如果需要进行填补，优先使用优先级低的用户
                    it = available_users.end();
                    it --;
                    user_id = it->at(1);
                    // break;
                }

                int max_send_batch_size = (remaining_time * npu.k) * (remaining_time * npu.k);
                int batch_size = std::min(remaining_samples[user_id], max_send_batch_size);
                batch_size = std::min(batch_size, free_batch_size);

                if (can_send(user_id, batch_size)) {
                    int handle_time = npu.calculateInferenceTime(batch_size);
                    int send_time = current_time - data.latency[server_id][user_id];
                    schedules[user_id].push_back({send_time, server_id, npu_id, batch_size});
                    remaining_samples[user_id] -= batch_size;
                    remaining_send_count[user_id] -= 1;

                    // 检查用户是否还有后续请求
                    if (remaining_samples[user_id] <= 0) {
                        if (current_time + handle_time <= users[user_id].e) {
                            completed_users.push_back(user_id);
                            remaining_samples.erase(user_id);
                        }
                    } else {
                        waiting_users.push({current_time + data.latency[server_id][user_id] + 1, user_id}); // 简化：下一个tick即可再次请求
                    }

                    // 维护内存占用
                    int memory_to_occupy = A * batch_size + B;
                    for (int t = current_time; t < current_time + handle_time; t++) {
                        memory_usage[t] += memory_to_occupy;
                    }

                } else {
                    waiting_users.push({current_time, user_id}); // 简化：下一个tick即可再次请求
                }

                available_users.erase(it);
            }
        }

        // --- 4. 最终统计 ---
        for(auto& v : remaining_samples) {
            timeout_users.push_back(v.first);
        }
        
        return result;
    }
    
    SolverResult solve(ProblemData& data) override {
        LOG("Running %s...", name().c_str());
        
        std::vector<User>& users = data.users;
        std::vector<std::vector<NPU> >& npus = data.npus;
        int M = data.m_users, N = data.n_servers, A = data.mem_a, B = data.mem_b;
        
        SolverResult result;
        result.solution.resize(data.m_users + 1);
        
        std::vector<int> user_order;
        for(int i = 1; i <= data.m_users; i ++) {
            user_order.push_back(i);
        }

        std::sort(user_order.begin(), user_order.end(), [&](int u1, int u2) {
            return data.users[u1].cnt < data.users[u2].cnt;
        });

        
        std::vector<std::vector<std::vector<int> > > simulate_users(data.n_servers + 1);
        std::vector<std::vector<NpuSimulationResult> > simulate_result(data.n_servers + 1);
        for (int i = 1; i <= data.n_servers; i ++) {
            simulate_users[i].resize(data.npus[i].size());
            simulate_result[i].resize(data.npus[i].size());
        }

        std::vector<int> timeout_users;
        for (int i = 1; i <= data.m_users; i ++) {
            timeout_users.push_back(i);
        }

        std::sort(timeout_users.begin(), timeout_users.end(), [&](int u1, int u2) {
            return users[u1].cnt < users[u2].cnt;
        });


        LOG("begin iter");
        int round = 1;
        while (round --) {
            LOG("round: %d", round);
            std::vector<int> new_timeout_users;
            int idx = 0, sz = timeout_users.size();
            int max_try_users_count = 500;
            while (idx < sz) {
                bool assign_success = false;
                for (int i = 1; i <= data.n_servers; i ++) {
                    if (assign_success) break;
                    for (int j = 1; j < data.npus[i].size(); j ++) {
                        if (assign_success) break;
                        std::vector<int> new_simulate_users = simulate_users[i][j];
                        int end = std::min(idx + max_try_users_count, (int)timeout_users.size());
                        new_simulate_users.insert(new_simulate_users.end(), 
                            timeout_users.begin() + idx, timeout_users.begin() + end);
                        NpuSimulationResult res = simulate(npus[i][j], data, new_simulate_users);
                        if (res.timeout_users.size() == 0) {
                            LOG("simulate success! user count: %d", new_simulate_users.size());
                            simulate_users[i][j] = new_simulate_users;
                            simulate_result[i][j] = res;
                            assign_success = true;
                        }
                    }
                }
                if (max_try_users_count == 1 and !assign_success) {
                    LOG("assign fail, current idx: %d", idx);
                    new_timeout_users.push_back(timeout_users[idx]);
                    idx += 1;
                    max_try_users_count = 2;
                }


                if (assign_success) idx += max_try_users_count;
                else {
                    max_try_users_count = std::max(1, max_try_users_count / 2);
                }
            }
            
            timeout_users = new_timeout_users;
            std::sort(timeout_users.begin(), timeout_users.end(), [&](int u1, int u2) {
                return users[u1].cnt < users[u2].cnt;
            });
        }

        
        std::vector<int> remaing_sample(data.m_users + 1, 0);
        for (int i = 1; i <= data.m_users; i ++) remaing_sample[i] = users[i].cnt;
        for (int i = 1; i <= data.n_servers; i ++) {
            for (int j = 1; j < data.npus[i].size(); j ++) {
                for (auto& user_id: simulate_users[i][j]) {
                    remaing_sample[user_id] = simulate_result[i][j].remaining_samples[user_id];
                    result.solution[user_id].insert(result.solution[user_id].end(), 
                        simulate_result[i][j].schedules[user_id].begin(), simulate_result[i][j].schedules[user_id].end());
                }
            }
        }

        LOG("handle timeout user!");
        for (int i = 1; i <= data.m_users; i ++) {
            if (remaing_sample[i] == 0) result.completed_user_count += 1;
            int start_time = 6e4 + 21;
            while (remaing_sample[i] > 0) {
                int batch_size = std::min(remaing_sample[i], ((npus[1][1].memory - B) / A));
                remaing_sample[i] -= batch_size;
                start_time += (21);
                result.solution[i].push_back({start_time, 1, 1, batch_size});
            }
        }

        LOG("finish users count: %d", result.completed_user_count);


        return result;
    }
};


// ===================================================================
// START: Main Function
// ===================================================================

int main() {
    auto program_start_time = std::chrono::steady_clock::now();
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(NULL);

    ProblemData data;
    data.read(std::cin);

    LOG("Data loaded. N_Servers: %d, M_Users: %d", data.n_servers, data.m_users);
    if (data.n_servers > 0 && data.npus[1].size() > 1) {
        LOG("NPU (1,1) properties: k=%d, memory=%d", 
            data.npus[1][1].k, data.npus[1][1].memory);
    }
    
    std::vector<std::unique_ptr<Solver>> solvers;
    // solvers.push_back(std::make_unique<SimpleSolver>());
    solvers.push_back(std::make_unique<TwoBlockSolver>());
    solvers.push_back(std::make_unique<AutoBlockSolver>());
    // solvers.push_back(std::make_unique<AutoBlockWithReverseFillSolver>());
    
    std::vector<std::vector<Schedule>> best_solution;
    int max_completed_users = -1;
    std::string best_solver_name = "None";

    LOG("==============================================");
    LOG("Starting Solver Competition...");
    LOG("==============================================");

    for (const auto& solver : solvers) {
        LOG("--- Running Solver: %s ---", solver->name().c_str());
        SolverResult result = solver->solve(data);
        // **注意: 使用新的变量名**
        LOG("--- [Result] Solver: %s | Predicted Completed Users: %d ---", 
            solver->name().c_str(), result.completed_user_count);

        // **注意: 使用新的变量名**
        if (result.completed_user_count > max_completed_users) {
            LOG("!!! New Best Solution Found! Previous best: %d users.", max_completed_users);
            max_completed_users = result.completed_user_count;
            best_solution = result.solution;
            best_solver_name = solver->name();
        }
    }

    LOG("==============================================");
    LOG("Competition Finished!");
    LOG("Best Solver (by fast prediction): %s", best_solver_name.c_str());
    // **注意: 使用新的变量名**
    LOG("Predicted Max Completed Users: %d", max_completed_users);
    LOG("==============================================");

    Solver::print_solution(best_solution, data.m_users);

    auto program_end_time = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsed_seconds = program_end_time - program_start_time;
    fprintf(stderr, "Total Execution Time: %.4f seconds\n", elapsed_seconds.count());

    return 0;
}