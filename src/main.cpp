#pragma GCC optimize(2)
#pragma GCC optimize(3)

#include <bits/stdc++.h>
#define MAX_RUN_TIME (28)


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
// START: Data Structures & Classes
// ===================================================================

struct NPU {
    int server_id;
    int npu_id;
    int k;
    int memory;

    int calculate_time (int batchSize) const {
        assert(batchSize >= 0);
        return ceil(sqrtl(batchSize) / k);
    }
};

struct User {
    int id; // 用户id
    int s, e, cnt;
    int a, b;

    /*根据显存计算batch size*/
    int calculate_batch(int mem) const {
        return std::max(0, (mem - b) / a);
    };

    /*根据batch size计算memory*/
    int calculate_memory(int batch_size) const {
        return batch_size * a + b;
    }
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

        for (int i = 1; i <= m_users; i ++) {
            std::cin >> users[i].a >> users[i].b;
        }        
    }
};


// ===================================================================
// START: NPU相关基类定义
// ===================================================================

struct NpuSimulationResult {
    std::vector<std::vector<Schedule> > schedules; 
    std::vector<int> completed_users;
    std::vector<int> timeout_users;
    std::unordered_map<int, int> remaining_samples;
    std::vector<int> memory_usage; 
    int finish_time;
};


class NPUSimulateModule {
public:

    virtual ~NPUSimulateModule() = default;
    /*模块命名*/
    virtual std::string name() const = 0;
    /*传入npu实例和分配的用户，返回模拟结果*/
    virtual NpuSimulationResult run(const NPU& npu, const ProblemData& data, 
        const std::vector<int>& assigned_users) const = 0;

};


/*根据用户情况自动分配时间块*/
class NPUAutoTimeBlockModule: public NPUSimulateModule {
public:
    std::string name() const override { return "NPUAutoTimeBlockModule"; }
    NpuSimulationResult run(const NPU& npu, const ProblemData& data, 
        const std::vector<int>& assigned_users) const override {
        LOG("%s module is running!", name().c_str());

        // 一些数据相关的定义
        int M = data.m_users, N = data.n_servers, memory = npu.memory;
        int max_time = 6e4, server_id = npu.server_id, npu_id = npu.npu_id;
        
        const std::vector<User>& users = data.users;
        const std::vector<std::vector<int>>& latency = data.latency;

        // 定义返回结果，并重命名
        NpuSimulationResult result;
        auto& schedules = result.schedules; 
        auto& completed_users = result.completed_users;
        auto& timeout_users = result.timeout_users;
        auto& remaining_samples = result.remaining_samples;
        auto& memory_usage = result.memory_usage; 
        int& finish_time = result.finish_time;

        memory_usage.resize(max_time + 100, 0);
        schedules.resize(M + 1);
        finish_time = 0;

        // 定义一些模拟过程中需要记录的信息
        using user_prior = int;
        using pii = std::pair<int, int>;
        using pri = std::pair<user_prior, int>;
        int early_stop = 0; // 是否提前停止
        std::vector<int> remaining_send_count(M + 1, 300);
        std::vector<int> small_time(M + 1, 1), big_time(M + 1, 1);
        std::priority_queue<pii, std::vector<pii>, std::greater<pii> > waiting_users; // (time, user_id)
        std::priority_queue<pri, std::vector<pri>, std::greater<pri> > available_users; // (priority, user_id)

        
        for (auto &user_id: assigned_users) {
            int A = users[user_id].a, B = users[user_id].b;
            double best_util = 0;
            int best_time = 1;
            for (int t = 1; t <= 16; t ++) {
                int time_batch_size = npu.k * npu.k * t * t;
                int m = A * time_batch_size + B;
                double util = 1.0 * time_batch_size / m / t;
                if (util > best_util) {
                    best_util = util;
                    best_time = t;
                }
                if (m >= memory) break;
            }
            small_time[user_id] = best_time;
        }

        for (auto &user_id: assigned_users) {
            int A = users[user_id].a, B = users[user_id].b;
            double best_util = 0;
            int best_time = 1;
            for (int t = 1; t <= 16; t ++) {
                int time_batch_size = npu.k * npu.k * t * t;
                if (time_batch_size <= 20) continue;
                int m = A * time_batch_size + B;
                double util = 1.0 * time_batch_size / m / t;
                if (util > best_util) {
                    best_util = util;
                    best_time = t;
                }
                if (m >= memory) break;
            }
            big_time[user_id] = best_time;
            // LOG("small time: %d, big time: %d", small_time[user_id], big_time[user_id]);
        }
        

        // 定义一些功能性函数
        
        
        /*假设剩余请求都按照batch的形式发送，预计的处理时间*/
        auto calculate_handle_time = [&](int user_id, int sample_count, int batch_size) {
            int batch_handle_time = npu.calculate_time(batch_size);
            int cnt = sample_count / batch_size;
            int res = sample_count - (cnt) * batch_size;
            int process_time = std::max(batch_handle_time, latency[server_id][user_id]);
            if (res > 0) {
                process_time = cnt * process_time + npu.calculate_time(res);
            } else {
                process_time = std::max(cnt - 1, 0) * process_time + batch_handle_time; 
            }
            return process_time;
        };

        /*计算当前用户的优先级*/
        auto calculate_priority = [&](int time, int user_id) -> user_prior {
            int A = users[user_id].a, B = users[user_id].b;
            int block_time = npu.calculate_time(B / A);
            int batch_size = (npu.k * block_time) * (npu.k * block_time);
            return users[user_id].e - calculate_handle_time(user_id, remaining_samples[user_id], batch_size);
        };

        
        /*每毫秒开始，维护能发送的用户*/
        auto update_avaliable_users = [&](int time) {
            while (!waiting_users.empty() && waiting_users.top().first <= time) {
                int user_id = waiting_users.top().second;
                waiting_users.pop();
                available_users.push({calculate_priority(time, user_id), user_id});
            }
        };

        /*更新显存占用*/
        auto update_memory = [&](int time, int user_id, int batch_size) {
            int handle_time = npu.calculate_time(batch_size);
            for (int i = time; i < time + handle_time; i ++) {
                memory_usage[i] += users[user_id].calculate_memory(batch_size);
            }
        };

        /*在time时刻发送batch_size，并更新相关信息*/
        auto send = [&](int time, int user_id, int batch_size) {
            // LOG("time: %d. user id: %d, batch: %d", time, user_id, batch_size);
            // LOG("remain sample count: %d, remain send count: %d", remaining_samples[user_id], remaining_send_count[user_id]);
            // LOG("big time: %d, small time: %d", big_time[user_id], small_time[user_id]);
            int send_time = time - latency[server_id][user_id];
            schedules[user_id].push_back({send_time, server_id, npu_id, batch_size});
            remaining_samples[user_id] -= batch_size;
            remaining_send_count[user_id] -= 1;
            int handle_time = npu.calculate_time(batch_size);
            if (remaining_samples[user_id] <= 0) {
                if (time + handle_time <= users[user_id].e) {
                    completed_users.push_back(user_id);
                    remaining_samples.erase(user_id);
                } else {
                    early_stop = 1;
                }
            } else {
                waiting_users.push({time + latency[server_id][user_id] + 1, user_id});
            }
            update_memory(time, user_id, batch_size);
        };

        /*判断能够在某一个时间节点发送一个batch size*/
        auto can_send = [&](int time, int user_id, int batch) {
            if (batch <= 0) return false;
            if (remaining_send_count[user_id] <= 0) return false;
            if (remaining_samples[user_id] > remaining_send_count[user_id] * batch) return false;
            int process_time = calculate_handle_time(user_id, remaining_samples[user_id], batch);
            return time + 1 * process_time <= users[user_id].e;
        };

        auto can_send2 = [&](int time, int user_id, int batch) {
            if (batch <= 0) return false;
            if (remaining_send_count[user_id] <= 0) return false;
            int A = users[user_id].a, B = users[user_id].b;
            double r1 = big_time[user_id] * big_time[user_id];
            double r2 = small_time[user_id] * small_time[user_id];
            double rate = r2 / (r1 + r2); // 大块的比例
            int cnt2 = remaining_send_count[user_id] * rate;
            int cnt1 = remaining_send_count[user_id] - cnt2;
            int big_batch = big_time[user_id] * big_time[user_id] * npu.k * npu.k;
            big_batch = std::min(big_batch, users[user_id].calculate_batch(memory));
            if (cnt1 * batch + cnt2 * big_batch < remaining_samples[user_id]) return false;
            int process_time = calculate_handle_time(user_id, remaining_samples[user_id], batch);
            return time + process_time <= users[user_id].e;
        };



        /*根据当前的情况向后预测block_time的时间，返回处理的batchsize数量*/
        auto simulate = [&](int time, int block_time, int &best_id) {
            std::vector<pri> tmp_users;
            int free_memory = memory, used_batch = 0, s = 0;
            int best_util = 0;
            while (!available_users.empty()) {
                if (free_memory < 110) break;
                int user_id = available_users.top().second;
                tmp_users.push_back(available_users.top());
                available_users.pop();
                
                int batch_size = (block_time * npu.k) * (block_time * npu.k);
                int free_batch_size = users[user_id].calculate_batch(free_memory);
                // 此处可以将continue修改为break,牺牲一些准确率，加快速度
                // if (free_batch_size == 0) break;
                if (free_batch_size == 0) continue;
                batch_size = std::min(batch_size, free_batch_size);
                batch_size = std::min(batch_size, remaining_samples[user_id]);
                
                if (can_send2(time, user_id, batch_size)) {
                    int current_util = batch_size * users[user_id].a;
                    int current_memory = users[user_id].calculate_memory(batch_size);
                    if (free_memory - current_memory < 110) {
                        if (current_util > best_util) {
                            best_util = current_util;
                            best_id = user_id;
                        }
                    } 
                    if (best_util == 0) {
                        used_batch += batch_size;
                        s += batch_size * users[user_id].a;
                        free_memory -= users[user_id].calculate_memory(batch_size);
                    } 
                    // LOG("batch: %d, a: %d, free: %d", batch_size, users[user_id].a, free_memory);
                    // LOG("util: %d", s);
                } else {
                    continue;
                }
            }
            s += best_util;

            for(auto&v: tmp_users) {
                available_users.push(v);
            }
            return s;
        };

        /*在某个时间节点的发送策略*/
        auto send_strategy = [&](int time) {
            int best_id = -1, best_util = 0, simulate_id = -1;
            while (!available_users.empty()) {
                if (early_stop) break;
                int user_id = available_users.top().second; 
                int free_memory = memory - memory_usage[time];
                int free_batch_size = users[user_id].calculate_batch(free_memory);
                // LOG("time: %d, user id: %d", time, user_id);


                if (free_memory < 110) {
                    available_users.pop();
                    waiting_users.push({time, user_id}); 
                    break;
                } else if (free_batch_size <= 0) {
                    available_users.pop();
                    waiting_users.push({time, user_id}); 
                    // break;
                    continue;
                }
                
                if (finish_time <= time) { // 对于优先级最高的用户
                    int max_block_time = npu.calculate_time(free_batch_size);
                    int min_block_time = 1;
                    int best_block_time = 1, best_batch_size = 0;
                    double best_util = 0.0;
                    
                    for (int block_time = min_block_time; block_time <= max_block_time; block_time ++) {
                        int batch_size = (block_time * npu.k) * (block_time * npu.k);
                        batch_size = std::min(batch_size, free_batch_size);
                        batch_size = std::min(batch_size, remaining_samples[user_id]);
                        // LOG("time: %d, user id: %d, batch size: %d", time, user_id, batch_size);
                        // LOG("can send: %d", can_send2(time, user_id, batch_size));
                        if (can_send(time, user_id, batch_size)) {
                            double util = simulate(time, block_time, simulate_id);
                            // LOG("util: %.2f, block time: %d", util, block_time);
                            util /= block_time;
                            if (util > best_util) {
                                best_util = util; 
                                best_batch_size = batch_size;
                                best_block_time = block_time;
                                best_id = simulate_id;
                            }
                        }
                    }

                    available_users.pop();
                    if (can_send(time, user_id, best_batch_size)) {
                        // LOG("time: %d. user id: %d, batch: %d", time, user_id, best_batch_size);
                        send(time, user_id, best_batch_size);
                        finish_time = time + best_block_time;
                    } else {
                        early_stop = 1;
                    }


                } else { // 对于剩余需要填块的用户
                    int remaining_time = finish_time - time;
                    int batch_size = (remaining_time * npu.k) * (remaining_time * npu.k);
                    batch_size = std::min(remaining_samples[user_id], batch_size);
                    batch_size = std::min(batch_size, free_batch_size);

                    available_users.pop();
                    if (can_send2(time, user_id, batch_size)) {
                        int current_memory = users[user_id].calculate_memory(batch_size);
                        if (free_memory - current_memory < 110) {
                            if (user_id == best_id) {
                                send(time, user_id, batch_size);
                                break;
                            } else {
                                waiting_users.push({time, user_id}); 
                            }
                        } else {
                            send(time, user_id, batch_size);
                        }
                        
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
            waiting_users.push({data.users[user_id].s + data.latency[server_id][user_id], user_id});
        }
        

        // 开始进行模拟
        for (int time = 0; time <= max_time; time ++) {
            // LOG("current time: %d, %d", available_users.size(), waiting_users.size());
            if (early_stop) break;
            if (available_users.size() == 0 and waiting_users.size() == 0) break;
            update_avaliable_users(time);
            send_strategy(time);
        }

        // 统计超时用户
        for(auto& v : remaining_samples) {
            timeout_users.push_back(v.first);
        }

        LOG("simulate users count:%d, timeout users count: %d", assigned_users.size(), timeout_users.size());
        return result;

    }

};


// ===================================================================
// Iterator相关基类定义
// ===================================================================



struct IteratorResult {
    std::vector<std::vector<std::vector<int> > > simulate_users;
    std::vector<std::vector<NpuSimulationResult> > simulate_results;
};

/*迭代模块的基类*/
class IteratorModule {
public:
    virtual ~IteratorModule() = default;
    virtual std::string name() const = 0;
    virtual IteratorResult run(const ProblemData& data, const NPUSimulateModule& simulator) const = 0;
};

/*暴力迭代，对于每个超时用户，在所有npu中都进行检测是否能够放入，
迭代多轮
*/
class BruteIteratorModule: public IteratorModule {
public:
    std::string name() const override { return "BruteIteratorModule"; }
    IteratorResult run(const ProblemData& data, const NPUSimulateModule& simulator) const override {
        LOG("Running %s...", name().c_str());
        int M = data.m_users, N = data.n_servers;
        auto& users = data.users;
        auto& npus = data.npus;
        
        IteratorResult result;
        std::vector<std::vector<std::vector<int> > >& simulate_users = result.simulate_users;
        std::vector<std::vector<NpuSimulationResult> >& simulate_results = result.simulate_results;
        simulate_users.resize(data.n_servers + 1);
        simulate_results.resize(data.n_servers + 1);
        for (int i = 1; i <= data.n_servers; i ++) {
            simulate_users[i].resize(data.npus[i].size());
            simulate_results[i].resize(data.npus[i].size());
        }
        

        std::vector<int> timeout_users;
        for (int i = 1; i <= data.m_users; i ++) {
            timeout_users.push_back(i);
        }


        std::sort(timeout_users.begin(), timeout_users.end(), [&](int u1, int u2) {
            double u1_pri = users[u1].cnt * users[u1].a + users[u1].b;
            double u2_pri = users[u2].cnt * users[u2].a + users[u2].b;

            // u1_pri /= std::pow(2.0, -users[u1].id / 5000.0);
            // u2_pri /= std::pow(2.0, -users[u2].id / 5000.0);

            return u1_pri < u2_pri;
        });


        LOG("begin iter");
        auto program_start_time = std::chrono::steady_clock::now();
        int round = 1;
        std::vector<int> r = {1, 1, 1, 1, 1};
        while (round --) {
            LOG("round %d is running", round);
            std::vector<int> new_timeout_users;
            int idx = 0, sz = timeout_users.size();
            int success_count = 0;
            int max_try_users_count = std::min(100, (int)timeout_users.size());
            while (idx < sz) {
                bool assign_success = false;
                auto program_current_time = std::chrono::steady_clock::now();
                std::chrono::duration<double> elapsed_seconds = program_current_time - program_start_time;
                if (elapsed_seconds.count() >= MAX_RUN_TIME) break;
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
                            simulate_results[i][j] = res;
                            assign_success = true;
                            success_count += (end - idx);
                        }
                    }
                }
                if (max_try_users_count == r[round] and !assign_success) {
                    new_timeout_users.push_back(timeout_users[idx]);
                    idx += r[round];
                    max_try_users_count = r[round];
                }

                if (assign_success) idx += max_try_users_count;
                else {
                    max_try_users_count = std::max(r[round], max_try_users_count / 3);
                }
            }
            
            timeout_users = new_timeout_users;
            std::sort(timeout_users.begin(), timeout_users.end(), [&](int u1, int u2) {
                double u1_pri = users[u1].cnt * users[u1].a + users[u1].b;
                double u2_pri = users[u2].cnt * users[u2].a + users[u2].b;

                // u1_pri /= std::pow(2.0, -users[u1].id / 5000.0);
                // u2_pri /= std::pow(2.0, -users[u2].id / 5000.0);

                return u1_pri < u2_pri;
            });
            LOG("this round new success count: %d", success_count);
            if (success_count == 0) break;
            auto program_current_time = std::chrono::steady_clock::now();
            std::chrono::duration<double> elapsed_seconds = program_current_time - program_start_time;
            if (elapsed_seconds.count() >= MAX_RUN_TIME) break;
        }
        // assert(round <= 0);
        // bool f1 = (int)timeout_users.size() >= 300;
        // bool f2 = (int)timeout_users.size() <= 100;
        // assert(f1 or f2);
        return result;        
    }

};


// ===================================================================
// TimeoutHandle相关基类定义
// ===================================================================

struct SolverResult {
    std::vector<std::vector<Schedule> > solution;
    int completed_user_count = 0; 
};

/*超时用户处理模块的基类*/
class TimeoutHandlerModule {
public:
    virtual ~TimeoutHandlerModule() = default;
    virtual std::string name() const = 0;
    virtual SolverResult run(const ProblemData& data, IteratorResult& iteratorResult) const = 0;
};


class AutoTimeBlockHandlerModule: public TimeoutHandlerModule {
public:
    std::string name() const override { return "AutoTimeBlockHandlerModule"; }
    
    // 对于超时用户的处理，
    void simulate(const NPU& npu, const ProblemData& data, 
        std::vector<int>& assigned_users, NpuSimulationResult& result) const {
        LOG("%s module is running!", name().c_str());
        // 一些数据相关的定义
        int M = data.m_users, N = data.n_servers, memory = npu.memory;;
        int max_time = 1e6, server_id = npu.server_id, npu_id = npu.npu_id;
    
        auto& users = data.users;
        auto& latency = data.latency;

        // 复用之前的模拟结果
        auto& schedules = result.schedules; 
        std::vector<int>& completed_users = result.completed_users;
        std::vector<int>& timeout_users = result.timeout_users;
        auto& remaining_samples = result.remaining_samples;
        std::vector<int>& memory_usage = result.memory_usage; 
        memory_usage.resize(max_time + 100, 0);
        schedules.resize(M + 1);
        int& finish_time = result.finish_time;

        // 定义一些模拟过程中需要记录的信息
        std::vector<int> remaining_send_count(M + 1, 300);
        std::priority_queue<arr2, std::vector<arr2>, std::greater<arr2>> waiting_users; // (time, user_id)
        std::priority_queue<arr2, std::vector<arr2>, std::greater<arr2>> available_users; // (priority, user_id)


        // 定义一些功能性函数
        
        auto calculate_handle_time = [&](int user_id, int batch_size) {
            int batch_handle_time = npu.calculate_time(batch_size);
            int cnt = remaining_samples[user_id] / batch_size;
            int res = remaining_samples[user_id] - (cnt) * batch_size;
            int process_time = std::max(batch_handle_time, latency[server_id][user_id]);
            if (res > 0) {
                process_time = cnt * process_time + npu.calculate_time(res);
            } else {
                process_time = std::max(cnt - 1, 0) * process_time + batch_handle_time; 
            }
            return process_time;
        };
        // 每毫秒开始，维护能发送的用户
        auto avaliable_users_update = [&](int time) {
            while (!waiting_users.empty() && waiting_users.top()[0] <= time) {
                int user_id = waiting_users.top()[1];
                waiting_users.pop();
                available_users.push({users[user_id].e, user_id});
            }
        };

        auto memory_update = [&](int time, int user_id, int batch_size) {
            int handle_time = npu.calculate_time(batch_size);
            int A = users[user_id].a, B = users[user_id].b;
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
            memory_update(time, user_id, batch_size);
        };

        // 判断能够在某一个时间节点发送一个batch size
        auto can_send = [&](int time, int user_id, int batch) {
            if (batch <= 0) return false;
            if (remaining_send_count[user_id] <= 0) return false;
            int max_batch_size = (memory - users[user_id].b) / users[user_id].a;
            if (remaining_samples[user_id] - batch > (remaining_send_count[user_id] - 1) * max_batch_size) return false;
            return true;
        };

        // 在某个时间节点的发送策略
        auto send_strategy = [&](int time) {
            // LOG("send strategy begin");
            while (!available_users.empty()) {

                int user_id = available_users.top()[1];
                available_users.pop(); 
                int A = users[user_id].a, B = users[user_id].b;
                int free_batch_size = (memory - memory_usage[time] - B) / A;
                int max_batch_size = (memory - B) / A;

                if (finish_time <= time) { // 对于优先级最高的用户
                    // 枚举时间块
                    int max_block_time = npu.calculate_time(max_batch_size);
                    int min_block_time = 1;
                    if (available_users.size() <= 0) min_block_time = max_block_time;
                    if (available_users.size() == 1) min_block_time = max_block_time - 1;

                    int best_batch_size = 1, best_block_time = 1;
                    double best_util = 0;
                    for (int block_time = min_block_time; block_time <= max_block_time; block_time ++) {
                        int batch_size = (block_time * npu.k) * (block_time * npu.k);
                        batch_size = std::min(batch_size, free_batch_size);
                        batch_size = std::min(batch_size, remaining_samples[user_id]);
                        int handle_time = npu.calculate_time(batch_size);
                        if (can_send(time, user_id, batch_size)) {
                            double util = batch_size * 100.0 / handle_time / (A * batch_size + B);
                            if (util > best_util) {
                                best_util = util; 
                                best_batch_size = batch_size;
                                best_block_time = block_time;
                            }
                        }
                    }
                    if (can_send(time, user_id, best_batch_size)) {
                        send_update(time, user_id, best_batch_size);
                        finish_time = time + best_block_time;
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
            // LOG("send strategy end");
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
        for (int time = finish_time; time <= max_time; time ++) {
            // LOG("current time: %d", time);
            if (available_users.size() == 0 and waiting_users.size() == 0) break;
            // 更新可发送用户
            avaliable_users_update(time);

            send_strategy(time);
        }

        // 统计超时用户
        for(auto& v : remaining_samples) {
            timeout_users.push_back(v.first);
        }

    }
    
    
    SolverResult run(const ProblemData& data, IteratorResult& iteratorResult) const override {
        LOG("%s module is running!", name().c_str());
        // 对于超时用户的分配
        // 按照装入最大bathsiz的情况作为标准，令总的结束时间最短
        
        // 先定义好以后信息
        auto& npus = data.npus;
        auto& simulate_users = iteratorResult.simulate_users ;
        auto& simulate_results = iteratorResult.simulate_results;       
        SolverResult result;
        auto& solution = result.solution;
        auto& completed_user_count = result.completed_user_count;
        solution.resize(data.m_users + 1);


        std::set<int> timeout_users;
        for (int i = 1; i <= data.m_users; i ++) {
            timeout_users.insert(i);
        }

        std::priority_queue<arr3, std::vector<arr3>, std::greater<arr3> > prior;
        std::vector<std::vector<std::vector<int> > > simulate_timeout_users(data.n_servers + 1);

        for (int i = 1; i <= data.n_servers; i ++) {
            for (int j = 1; j < (int)data.npus[i].size(); j ++) {
                simulate_timeout_users[i].resize(data.npus[i].size());
                result.completed_user_count += iteratorResult.simulate_results[i][j].completed_users.size();
                prior.push({iteratorResult.simulate_results[i][j].finish_time, i, j});
                for (auto&v: simulate_results[i][j].completed_users) {
                    timeout_users.erase(v);
                }
            }
        }

        LOG("timeout users count: %d", timeout_users.size());

        for (auto&v: timeout_users) {
            arr3 tp = prior.top();
            prior.pop();
            int pri = tp[0], server_id = tp[1], npu_id = tp[2];
            int cnt = data.users[v].cnt, B = data.users[v].b, A = data.users[v].a;
            int max_batch_size = (npus[server_id][npu_id].memory - B) / A;
            int handle_time = data.npus[server_id][npu_id].calculate_time(max_batch_size);
            cnt = cnt / max_batch_size * handle_time;
            prior.push({pri + cnt, server_id, npu_id});
            simulate_timeout_users[server_id][npu_id].push_back(v);
        }

        LOG("timeout users assign over!, ready for handle %d users", simulate_timeout_users[1][1].size());

        for (int i = 1; i <= data.n_servers; i ++) {
            for (int j = 1; j < (int)data.npus[i].size(); j ++) {
                simulate(npus[i][j], data, simulate_timeout_users[i][j], simulate_results[i][j]);
            }
        }

        LOG("timeout users handle out!");
        for (int i = 1; i <= data.n_servers; i ++) {
            for (int j = 1; j < data.npus[i].size(); j ++) {
                for (int user_id = 1; user_id <= data.m_users; user_id ++) {
                    auto& schedule = simulate_results[i][j].schedules[user_id];
                    solution[user_id].insert(solution[user_id].end(), schedule.begin(), schedule.end());
                }
            }
        }

        LOG("finish users count: %d", result.completed_user_count);

        return result;
    };
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



class AutoTimeBlockSolver : public Solver {
public:
    std::string name() const override { return "AutoTimeBlockSolver"; }
    
    SolverResult solve(ProblemData& data) override {
        LOG("Running %s...", name().c_str());

        NPUAutoTimeBlockModule simulator;
        BruteIteratorModule iterator;
        AutoTimeBlockHandlerModule timeout_handler;
        IteratorResult iterator_result = iterator.run(data, simulator);
        
        return timeout_handler.run(data, iterator_result); 
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
    solvers.push_back(std::make_unique<AutoTimeBlockSolver>());

    
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