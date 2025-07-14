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
#define DEBUG
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

    int calculate_time (int batchSize) {
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


// ===================================================================
// START: NPU相关基类定义
// ===================================================================

struct NpuSimulationResult {
    std::map<int, std::vector<Schedule>> schedules; 
    std::vector<int> completed_users;
    std::vector<int> timeout_users;
    std::map<int, int> remaining_samples;
    std::vector<int> memory_usage; 
    int finish_time;
};

/**
 * @class NPUSimulateModule
 * @brief NPU内部调度模拟模块的抽象基类.
 * * 它的核心职责是接收一个NPU和分配给它的用户组，通过精确模拟给出一份详细的调度结果。
 */
class NPUSimulateModule {
public:
    // 接收NPU、全局数据和分配的用户列表
    
    virtual ~NPUSimulateModule() = default;
    virtual std::string name() const = 0;
    virtual NpuSimulationResult run(NPU& npu, ProblemData& data, std::vector<int>& assigned_users) = 0;

};


class NPUAutoBlockModule: public NPUSimulateModule {
public:
    std::string name() const override { return "NPUAutoBlockModule"; }
    NpuSimulationResult run(NPU& npu, ProblemData& data, std::vector<int>& assigned_users) override {
        // LOG("%s module is running!", name().c_str());
        // 一些数据相关的定义
        int M = data.m_users, N = data.n_servers, A = data.mem_a, B = data.mem_b;
        int max_time = 6e4, server_id = npu.server_id, npu_id = npu.npu_id;
        int max_batch_size = (npu.memory - B) / A, memory = npu.memory;
        int max_block_count = npu.memory / (2 * B);
        std::vector<User>& users = data.users;
        std::vector<std::vector<int>>& latency = data.latency;

        // 定义返回结果，并重命名
        NpuSimulationResult result;
        std::map<int, std::vector<Schedule>>& schedules = result.schedules; 
        std::vector<int>& completed_users = result.completed_users;
        std::vector<int>& timeout_users = result.timeout_users;
        std::map<int, int>& remaining_samples = result.remaining_samples;
        std::vector<int>& memory_usage = result.memory_usage; 
        memory_usage.resize(max_time + 100, 0);
        int& finish_time = result.finish_time;
        finish_time = 0;

        // 定义一些模拟过程中需要记录的信息
        std::vector<int> remaining_send_count(M + 1, 300);
        std::priority_queue<arr2, std::vector<arr2>, std::greater<arr2>> waiting_users; // (time, user_id)
        std::priority_queue<arr2, std::vector<arr2>, std::greater<arr2>> available_users; // (priority, user_id)


        // 定义一些功能性函数
        
        auto calculate_batch = [&](int mem) {return std::max(0, (mem - B) / A);};
        
        // 每毫秒开始，维护能发送的用户
        auto avaliable_users_update = [&](int time) {
            while (!waiting_users.empty() && waiting_users.top()[0] <= time) {
                int user_id = waiting_users.top()[1];
                waiting_users.pop();
                available_users.push({users[user_id].e, user_id});
            }
        };

        auto memory_update = [&](int time, int batch_size) {
            int handle_time = npu.calculate_time(batch_size);
            for (int i = time; i < time + handle_time; i ++) {
                memory_usage[i] += (batch_size * A + B);
            }
        };

        // 发送后对剩余数量，剩余发送次数，下次能发送时间更新
        auto send_update = [&](int time, int user_id, int batch_size) {
            int send_time = time - latency[server_id][user_id];
            schedules[user_id].push_back({send_time, server_id, npu_id, batch_size});
            remaining_samples[user_id] -= batch_size;
            remaining_send_count[user_id] -= 1;
            int handle_time = npu.calculate_time(batch_size);
            if (remaining_samples[user_id] <= 0) {
                if (time + handle_time <= users[user_id].e) {
                    completed_users.push_back(user_id);
                    remaining_samples.erase(user_id);
                }
            } else {
                waiting_users.push({time + latency[server_id][user_id] + 1, user_id});
            }
            memory_update(time, batch_size);
        };

        // 判断能够在某一个时间节点发送一个batch size
        auto can_send = [&](int time, int user_id, int batch) {
            if (remaining_send_count[user_id] <= 0) return false;
            if (remaining_samples[user_id] - batch > (remaining_send_count[user_id] - 1) * max_batch_size) return false;
            int batch_handle_time = npu.calculate_time(batch);
            int cnt = remaining_samples[user_id] / batch;
            int res = remaining_samples[user_id] - (cnt) * batch;
            int process_time = std::max(batch_handle_time, latency[server_id][user_id]);
            if (res > 0) {
                process_time = cnt * process_time + npu.calculate_time(res);
            } else {
                process_time = std::max(cnt - 1, 0) * process_time + batch_handle_time; 
            }
            return time + 1 * process_time <= users[user_id].e;
        };

        // 在某个时间节点的发送策略
        auto send_strategy = [&](int time) {
            while (!available_users.empty()) {
                int free_batch_size = calculate_batch(memory - memory_usage[time]);
                if (free_batch_size <= 0) break;

                int user_id = available_users.top()[1];
                available_users.pop();

                if (finish_time <= time) { // 对于优先级最高的用户
                    int cnt = sqrtl(available_users.size() + 1);
                    for (int i = std::min(cnt, max_block_count); i >= 1; i --) {
                        int block_size = (memory / i - B) / A;
                        int block_time = npu.calculate_time(block_size);
                        block_size = (block_time * npu.k) * (block_time * npu.k);
                        int batch_size = std::min(block_size, free_batch_size);
                        batch_size = std::min(batch_size, remaining_samples[user_id]);

                        if (can_send(time, user_id, batch_size)) {
                            send_update(time, user_id, batch_size);
                            finish_time = time + block_time;
                            break;
                        }
                    }
                } else { // 对于剩余需要填块的用户
                    int remaining_time = finish_time - time;
                    int max_send_batch_size = (remaining_time * npu.k) * (remaining_time * npu.k);
                    int batch_size = std::min(remaining_samples[user_id], max_send_batch_size);
                    batch_size = std::min(batch_size, free_batch_size);

                    if (can_send(time, user_id, batch_size)) {
                        send_update(time, user_id, batch_size);
                    } else {
                        waiting_users.push({time, user_id}); 
                    }
                }

            }
            return;
        };
        
        
        // 模拟前的初始化
        for (auto& user_id: assigned_users) {
            remaining_samples[user_id] = data.users[user_id].cnt;
        }
        for (int user_id: assigned_users) {
            waiting_users.push({data.users[user_id].s + data.latency[server_id][user_id], user_id});
        }

        // 开始进行模拟
        for (int time = 0; time <= max_time; time ++) {
            // 更新可发送用户
            avaliable_users_update(time);
            send_strategy(time);
        }

        // 统计超时用户
        for(auto& v : remaining_samples) {
            timeout_users.push_back(v.first);
        }
        return result;

    }

};


class NPUAutoBlockWithReverseFillModule: public NPUSimulateModule {
public:
    std::string name() const override { return "NPUAutoBlockWithReverseFillModule"; }
    NpuSimulationResult run(NPU& npu, ProblemData& data, std::vector<int>& assigned_users) override {
        // LOG("%s module is running!", name().c_str());
        // 一些数据相关的定义
        int M = data.m_users, N = data.n_servers, A = data.mem_a, B = data.mem_b;
        int max_time = 6e4, server_id = npu.server_id, npu_id = npu.npu_id;
        int max_batch_size = (npu.memory - B) / A, memory = npu.memory;
        int max_block_count = npu.memory / (2 * B);
        std::vector<User>& users = data.users;
        std::vector<std::vector<int>>& latency = data.latency;

        // 定义返回结果，并重命名
        NpuSimulationResult result;
        std::map<int, std::vector<Schedule>>& schedules = result.schedules; 
        std::vector<int>& completed_users = result.completed_users;
        std::vector<int>& timeout_users = result.timeout_users;
        std::map<int, int>& remaining_samples = result.remaining_samples;
        std::vector<int>& memory_usage = result.memory_usage; 
        memory_usage.resize(max_time + 100, 0);
        int& finish_time = result.finish_time;
        finish_time = 0;

        // 定义一些模拟过程中需要记录的信息
        std::vector<int> remaining_send_count(M + 1, 300);
        std::priority_queue<arr2, std::vector<arr2>, std::greater<arr2>> waiting_users; // (time, user_id)
        std::set<arr2> available_users; // (priority, user_id)
        int global_block_time = 0;

        // 定义一些功能性函数
        auto calculate_batch = [&](int mem) {return std::max(0, (mem - B) / A);};
        
        // 每毫秒开始，维护能发送的用户
        auto avaliable_users_update = [&](int time) {
            while (!waiting_users.empty() && waiting_users.top()[0] <= time) {
                int user_id = waiting_users.top()[1];
                waiting_users.pop();
                available_users.insert({users[user_id].e, user_id});
            }
        };

        auto memory_update = [&](int time, int batch_size) {
            int handle_time = npu.calculate_time(batch_size);
            for (int i = time; i < time + handle_time; i ++) {
                memory_usage[i] += (batch_size * A + B);
            }
        };

        // 发送后对剩余数量，剩余发送次数，下次能发送时间更新
        auto send_update = [&](int time, int user_id, int batch_size) {
            int send_time = time - latency[server_id][user_id];
            schedules[user_id].push_back({send_time, server_id, npu_id, batch_size});
            remaining_samples[user_id] -= batch_size;
            remaining_send_count[user_id] -= 1;
            int handle_time = npu.calculate_time(batch_size);
            if (remaining_samples[user_id] <= 0) {
                if (time + handle_time <= users[user_id].e) {
                    completed_users.push_back(user_id);
                    remaining_samples.erase(user_id);
                }
            } else {
                waiting_users.push({time + latency[server_id][user_id] + 1, user_id});
            }
            memory_update(time, batch_size);
        };

        // 判断能够在某一个时间节点发送一个batch size
        auto can_send = [&](int time, int user_id, int batch) {
            if (remaining_send_count[user_id] <= 0) return false;
            if (remaining_samples[user_id] - batch > (remaining_send_count[user_id] - 1) * max_batch_size) return false;
            int batch_handle_time = npu.calculate_time(batch);
            int cnt = remaining_samples[user_id] / batch;
            int res = remaining_samples[user_id] - (cnt) * batch;
            int process_time = std::max(batch_handle_time, latency[server_id][user_id]);
            if (res > 0) {
                process_time = cnt * process_time + npu.calculate_time(res);
            } else {
                process_time = std::max(cnt - 1, 0) * process_time + batch_handle_time; 
            }
            return time + 1 * process_time <= users[user_id].e;
        };

        // 在某个时间节点的发送策略
        auto send_strategy = [&](int time) {
            while (!available_users.empty()) {
                int free_batch_size = calculate_batch(memory - memory_usage[time]);
                if (free_batch_size <= 0) break;

                auto it = available_users.begin();
                int user_id = it->at(1);

                if (finish_time <= time) { // 对于优先级最高的用户
                    int cnt = sqrtl(available_users.size());
                    for (int i = std::min(cnt, max_block_count); i >= 1; i --) {
                        int block_size = (memory / i - B) / A;
                        int block_time = npu.calculate_time(block_size);
                        block_size = (block_time * npu.k) * (block_time * npu.k);
                        int batch_size = std::min(block_size, free_batch_size);
                        batch_size = std::min(batch_size, remaining_samples[user_id]);

                        if (can_send(time, user_id, batch_size)) {
                            send_update(time, user_id, batch_size);
                            finish_time = time + block_time;
                            global_block_time = block_time;
                            break;
                        }
                    }
                } else { // 对于剩余需要填块的用户
                    int remaining_time = finish_time - time;
                    if (remaining_time != global_block_time) {
                        it = available_users.end();
                        it --;
                        user_id = it->at(1);
                    }

                    int max_send_batch_size = (remaining_time * npu.k) * (remaining_time * npu.k);
                    int batch_size = std::min(remaining_samples[user_id], max_send_batch_size);
                    batch_size = std::min(batch_size, free_batch_size);

                    if (can_send(time, user_id, batch_size)) {
                        send_update(time, user_id, batch_size);
                    } else {
                        waiting_users.push({time, user_id}); 
                    }
                }
                available_users.erase(it);

            }
            return;
        };
        

        
        // 模拟前的初始化
        for (auto& user_id: assigned_users) {
            remaining_samples[user_id] = data.users[user_id].cnt;
        }
        for (int user_id: assigned_users) {
            waiting_users.push({data.users[user_id].s + data.latency[server_id][user_id], user_id});
        }

        // 开始进行模拟
        for (int time = 0; time <= max_time; time ++) {
            // 更新可发送用户
            avaliable_users_update(time);
            send_strategy(time);
        }

        // 统计超时用户
        for(auto& v : remaining_samples) {
            timeout_users.push_back(v.first);
        }
        return result;

    }

};


/**
 * @class UserAssignmentModule
 * @brief 用户分配模块的抽象基类.
 */
class UserAssignmentModule {
public:
    UserAssignmentModule(ProblemData& data) : data(data) {}
    virtual ~UserAssignmentModule() = default;
    virtual std::string name() const = 0;
    virtual std::vector<std::vector<std::vector<int>>> run() = 0;

    ProblemData& data;
};

/*未完成的类，暂时不要使用*/
class GreddyAssignmentModule : public UserAssignmentModule {
    GreddyAssignmentModule(ProblemData& data) : UserAssignmentModule(data) {}
    std::string name() const override { return "GreddyAssignmentModule"; }

    std::vector<std::vector<std::vector<int>>> run() override {
        LOG("Running Assignment Module: %s", name().c_str());
        // 定义一些初始信息
        int M = data.m_users, N = data.n_servers, A = data.mem_a, B = data.mem_b;
        std::vector<User>& users = data.users;
        std::vector<std::vector<int>>& latency = data.latency;

        std::vector<std::vector<std::vector<int>>> assignments;
        assignments.resize(N + 1);
        
        std::vector<int> user_order;
        for(int i = 1; i <= data.m_users; i ++) {
            user_order.push_back(i);
        }
        std::sort(user_order.begin(), user_order.end(), [&](int u1, int u2) {
            return data.users[u1].cnt < data.users[u2].cnt;
        });

        // TODO

        

        return assignments;

    }
};

// ===================================================================
// Iterator相关基类定义
// ===================================================================

struct SolverResult {
    std::vector<std::vector<Schedule> > solution;
    int completed_user_count = 0; 
};

/*迭代模块的基类*/
class IteratorModule {
public:
    virtual ~IteratorModule() = default;
    virtual std::string name() const = 0;
    virtual SolverResult run(ProblemData& data, NPUSimulateModule& simulator) = 0;
};

/*暴力迭代，对于每个超时用户，在所有npu中都进行检测是否能够放入，
迭代多轮
*/
class BruteIteratorModule: public IteratorModule {
public:
    std::string name() const override { return "BruteIteratorModule"; }
    SolverResult run(ProblemData& data, NPUSimulateModule& simulator) override {
        LOG("Running %s...", name().c_str());
        int M = data.m_users, N = data.n_servers, A = data.mem_a, B = data.mem_b;
        std::vector<User>& users = data.users;
        std::vector<std::vector<NPU> >& npus = data.npus;
        
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
        int round = 10;
        while (round --) {
            LOG("round %d is running", round);
            std::vector<int> new_timeout_users;
            int idx = 0, sz = timeout_users.size();
            int success_count = 0;
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
                        NpuSimulationResult res = simulator.run(npus[i][j], data, new_simulate_users);
                        if (res.timeout_users.size() == 0) {
                            simulate_users[i][j] = new_simulate_users;
                            simulate_result[i][j] = res;
                            assign_success = true;
                            success_count += (end - idx);
                        }
                    }
                }
                if (max_try_users_count == 1 and !assign_success) {
                    new_timeout_users.push_back(timeout_users[idx]);
                    idx += 1;
                    max_try_users_count = 1;
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
            LOG("this round new success count: %d", success_count);
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



// ===================================================================
// Solver相关基类定义
// ===================================================================

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


class AutoBlockSolver : public Solver {
public:
    std::string name() const override { return "AutoBlockSolver"; }
    
    SolverResult solve(ProblemData& data) override {
        LOG("Running %s...", name().c_str());
        NPUAutoBlockModule simulator;
        BruteIteratorModule iterator;
        return iterator.run(data, simulator);
    }
};


class AutoBlockWithReverseFillSolver : public Solver {
public:
    std::string name() const override { return "AutoBlockWithReverseFillSolver"; }
    
    SolverResult solve(ProblemData& data) override {
        LOG("Running %s...", name().c_str());
        
        LOG("Running %s...", name().c_str());
        NPUAutoBlockWithReverseFillModule simulator;
        BruteIteratorModule iterator;
        return iterator.run(data, simulator);
    }
};


// ===================================================================
// START: Main Function
// ===================================================================

int main() {
    auto program_start_time = std::chrono::steady_clock::now();
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(NULL);

    LOG("start");
    ProblemData data;
    data.read(std::cin);

    LOG("Data loaded. N_Servers: %d, M_Users: %d", data.n_servers, data.m_users);
    if (data.n_servers > 0 && data.npus[1].size() > 1) {
        LOG("NPU (1,1) properties: k=%d, memory=%d", 
            data.npus[1][1].k, data.npus[1][1].memory);
    }
    
    std::vector<std::unique_ptr<Solver>> solvers;
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