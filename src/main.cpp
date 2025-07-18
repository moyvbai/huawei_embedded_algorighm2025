#pragma GCC optimize(2)
#pragma GCC optimize(3)

#include <bits/stdc++.h>


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
// START: NPU��ػ��ඨ��
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
 * @brief NPU�ڲ�����ģ��ģ��ĳ������.
 * * ���ĺ���ְ���ǽ���һ��NPU�ͷ���������û��飬ͨ����ȷģ�����һ����ϸ�ĵ��Ƚ����
 */
class NPUSimulateModule {
public:
    // ����NPU��ȫ�����ݺͷ�����û��б�
    
    virtual ~NPUSimulateModule() = default;
    virtual std::string name() const = 0;
    virtual NpuSimulationResult run(NPU& npu, ProblemData& data, std::vector<int>& assigned_users) = 0;

};


class NPUAutoBatchSizeBlockModule: public NPUSimulateModule {
public:
    std::string name() const override { return "NPUAutoBatchSizeBlockModule"; }
    NpuSimulationResult run(NPU& npu, ProblemData& data, std::vector<int>& assigned_users) override {
        // LOG("%s module is running!", name().c_str());
        // һЩ������صĶ���
        int M = data.m_users, N = data.n_servers, A = data.mem_a, B = data.mem_b;
        int max_time = 6e4, server_id = npu.server_id, npu_id = npu.npu_id;
        int max_batch_size = (npu.memory - B) / A, memory = npu.memory;
        int max_block_count = npu.memory / (2 * B);
        std::vector<User>& users = data.users;
        std::vector<std::vector<int>>& latency = data.latency;

        // ���巵�ؽ������������
        NpuSimulationResult result;
        std::map<int, std::vector<Schedule>>& schedules = result.schedules; 
        std::vector<int>& completed_users = result.completed_users;
        std::vector<int>& timeout_users = result.timeout_users;
        std::map<int, int>& remaining_samples = result.remaining_samples;
        std::vector<int>& memory_usage = result.memory_usage; 
        memory_usage.resize(max_time + 100, 0);
        int& finish_time = result.finish_time;
        finish_time = 0;

        // ����һЩģ���������Ҫ��¼����Ϣ
        std::vector<int> remaining_send_count(M + 1, 300);
        std::priority_queue<arr2, std::vector<arr2>, std::greater<arr2>> waiting_users; // (time, user_id)
        std::priority_queue<arr2, std::vector<arr2>, std::greater<arr2>> available_users; // (priority, user_id)


        // ����һЩ�����Ժ���
        
        auto calculate_batch = [&](int mem) {return std::max(0, (mem - B) / A);};
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

        // ÿ���뿪ʼ��ά���ܷ��͵��û�
        auto avaliable_users_update = [&](int time) {
            while (!waiting_users.empty() && waiting_users.top()[0] <= time) {
                int user_id = waiting_users.top()[1];
                waiting_users.pop();
                available_users.push({users[user_id].e - 3 * calculate_handle_time(user_id, max_batch_size) / 2, user_id});
            }
        };

        auto memory_update = [&](int time, int batch_size) {
            int handle_time = npu.calculate_time(batch_size);
            for (int i = time; i < time + handle_time; i ++) {
                memory_usage[i] += (batch_size * A + B);
            }
        };

        // ���ͺ��ʣ��������ʣ�෢�ʹ������´��ܷ���ʱ�����
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

        // �ж��ܹ���ĳһ��ʱ��ڵ㷢��һ��batch size
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

        // ��ĳ��ʱ��ڵ�ķ��Ͳ���
        auto send_strategy = [&](int time) {
            while (!available_users.empty()) {
                int free_batch_size = calculate_batch(memory - memory_usage[time]);
                if (free_batch_size <= 0) break;

                int user_id = available_users.top()[1];
                available_users.pop(); 

                if (finish_time <= time) { // �������ȼ���ߵ��û�
                    int cnt = available_users.size() + 1;
                    for (int i = std::min(cnt, max_block_count); i >= 1; i --) {
                        int block_size = (memory / i - B) / A;
                        int block_time = npu.calculate_time(block_size);
                        block_size = (block_time * npu.k) * (block_time * npu.k);
                        int batch_size = std::min(block_size, free_batch_size);
                        batch_size = std::min(batch_size, remaining_samples[user_id]);
                        // block_time = npu.calculate_time(batch_size);
                        if (i == 1) {
                            // batch_size = std::min(36, batch_size);
                            block_time = npu.calculate_time(batch_size);
                        }

                        if (can_send(time, user_id, batch_size)) {
                            send_update(time, user_id, batch_size);
                            finish_time = time + block_time;
                            break;
                        }
                    }
                } else { // ����ʣ����Ҫ�����û�
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
        
        
        // ģ��ǰ�ĳ�ʼ��
        for (auto& user_id: assigned_users) {
            remaining_samples[user_id] = data.users[user_id].cnt;
        }
        for (int user_id: assigned_users) {
            waiting_users.push({data.users[user_id].s + data.latency[server_id][user_id], user_id});
        }

        // ��ʼ����ģ��
        for (int time = 0; time <= max_time; time ++) {
            // ���¿ɷ����û�
            avaliable_users_update(time);
            send_strategy(time);
        }

        // ͳ�Ƴ�ʱ�û�
        for(auto& v : remaining_samples) {
            timeout_users.push_back(v.first);
        }
        return result;

    }

};



class NPUAutoTimeBlockModule: public NPUSimulateModule {
public:
    std::string name() const override { return "NPUAutoTimeBlockModule"; }
    NpuSimulationResult run(NPU& npu, ProblemData& data, std::vector<int>& assigned_users) override {
        // LOG("%s module is running!", name().c_str());
        // һЩ������صĶ���
        int M = data.m_users, N = data.n_servers, A = data.mem_a, B = data.mem_b;
        int max_time = 6e4, server_id = npu.server_id, npu_id = npu.npu_id;
        int max_batch_size = (npu.memory - B) / A, memory = npu.memory;
        int max_block_count = npu.memory / (2 * B), max_block_time = npu.calculate_time(max_batch_size);
        int min_block_time = npu.calculate_time((npu.memory / max_block_count - B) / A);
        std::vector<User>& users = data.users;
        std::vector<std::vector<int>>& latency = data.latency;

        // ���巵�ؽ������������
        NpuSimulationResult result;
        std::map<int, std::vector<Schedule>>& schedules = result.schedules; 
        std::vector<int>& completed_users = result.completed_users;
        std::vector<int>& timeout_users = result.timeout_users;
        std::map<int, int>& remaining_samples = result.remaining_samples;
        std::vector<int>& memory_usage = result.memory_usage; 
        memory_usage.resize(max_time + 100, 0);
        int& finish_time = result.finish_time;
        finish_time = 0;

        // ����һЩģ���������Ҫ��¼����Ϣ
        std::vector<int> remaining_send_count(M + 1, 300);
        std::priority_queue<arr2, std::vector<arr2>, std::greater<arr2>> waiting_users; // (time, user_id)
        std::priority_queue<arr3, std::vector<arr3>, std::greater<arr3>> available_users; // (priority, user_id)


        // ����һЩ�����Ժ���
        
        auto calculate_batch = [&](int mem) {return std::max(0, (mem - B) / A);};
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
        // ÿ���뿪ʼ��ά���ܷ��͵��û�
        auto avaliable_users_update = [&](int time) {
            while (!waiting_users.empty() && waiting_users.top()[0] <= time) {
                int user_id = waiting_users.top()[1];
                waiting_users.pop();
                available_users.push({users[user_id].e - 3 * calculate_handle_time(user_id, max_batch_size) / 2, 1, user_id});
            }
        };

        auto memory_update = [&](int time, int batch_size) {
            int handle_time = npu.calculate_time(batch_size);
            for (int i = time; i < time + handle_time; i ++) {
                memory_usage[i] += (batch_size * A + B);
            }
        };

        // ���ͺ��ʣ��������ʣ�෢�ʹ������´��ܷ���ʱ�����
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

        // �ж��ܹ���ĳһ��ʱ��ڵ㷢��һ��batch size
        auto can_send = [&](int time, int user_id, int batch) {
            if (remaining_send_count[user_id] <= 0) return false;
            if (remaining_samples[user_id] - batch > (remaining_send_count[user_id] - 1) * max_batch_size) return false;
            // return true;
            int batch_handle_time = npu.calculate_time(batch);
            int process_time = calculate_handle_time(user_id, max_batch_size);
            return time + 10 * process_time / 10 <= users[user_id].e;
        };

        // ��ĳ��ʱ��ڵ�ķ��Ͳ���
        auto send_strategy = [&](int time) {
            while (!available_users.empty()) {
                int free_batch_size = calculate_batch(memory - memory_usage[time]);
                if (free_batch_size <= 0) break;

                int user_id = available_users.top()[2];
                available_users.pop(); 

                if (finish_time <= time) { // �������ȼ���ߵ��û�
                    // ö��ʱ���
                    int min_time = min_block_time;
                    int best_batch_size = 1, best_block_time = 1;
                    double best_util = 0;
                    // if (available_users.size() <= 2) min_time = max_block_time; 
                    for (int block_time = std::max(min_block_time, min_time); block_time <= max_block_time; block_time ++) {
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
                    
                } else { // ����ʣ����Ҫ�����û�
                    int remaining_time = finish_time - time;
                    int max_send_batch_size = (remaining_time * npu.k) * (remaining_time * npu.k);
                    int batch_size = std::min(remaining_samples[user_id], max_send_batch_size);
                    batch_size = std::min(batch_size, free_batch_size);

                    if (can_send(time, user_id, batch_size) and batch_size > 4) {
                        send_update(time, user_id, batch_size);
                    } else {
                        waiting_users.push({time, user_id}); 
                    }
                }

            }
            return;
        };
        
        
        // ģ��ǰ�ĳ�ʼ��
        for (auto& user_id: assigned_users) {
            remaining_samples[user_id] = data.users[user_id].cnt;
        }
        for (int user_id: assigned_users) {
            waiting_users.push({data.users[user_id].s + data.latency[server_id][user_id], user_id});
        }

        // ��ʼ����ģ��
        for (int time = 0; time <= max_time; time ++) {
            // ���¿ɷ����û�
            avaliable_users_update(time);
            send_strategy(time);
        }

        // ͳ�Ƴ�ʱ�û�
        for(auto& v : remaining_samples) {
            timeout_users.push_back(v.first);
        }
        return result;

    }

};


// ===================================================================
// Iterator��ػ��ඨ��
// ===================================================================

struct IteratorResult {
    std::vector<std::vector<std::vector<int> > > simulate_users;
    std::vector<std::vector<NpuSimulationResult> > simulate_results;
};

/*����ģ��Ļ���*/
class IteratorModule {
public:
    virtual ~IteratorModule() = default;
    virtual std::string name() const = 0;
    virtual IteratorResult run(ProblemData& data, NPUSimulateModule& simulator) = 0;
};

/*��������������ÿ����ʱ�û���������npu�ж����м���Ƿ��ܹ����룬
��������
*/
class BruteIteratorModule: public IteratorModule {
public:
    std::string name() const override { return "BruteIteratorModule"; }
    IteratorResult run(ProblemData& data, NPUSimulateModule& simulator) override {
        LOG("Running %s...", name().c_str());
        int M = data.m_users, N = data.n_servers, A = data.mem_a, B = data.mem_b;
        std::vector<User>& users = data.users;
        std::vector<std::vector<NPU> >& npus = data.npus;
        
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
                            simulate_results[i][j] = res;
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

        return result;        
    }

};

// ===================================================================
// TimeoutHandle��ػ��ඨ��
// ===================================================================

struct SolverResult {
    std::vector<std::vector<Schedule> > solution;
    int completed_user_count = 0; 
};

/*��ʱ�û�����ģ��Ļ���*/
class TimeoutHandlerModule {
public:
    virtual ~TimeoutHandlerModule() = default;
    virtual std::string name() const = 0;
    virtual SolverResult run(ProblemData& data, IteratorResult& iteratorResult) = 0;
};


class AutoTimeBlockHandlerModule: public TimeoutHandlerModule {
public:
    std::string name() const override { return "AutoTimeBlockHandlerModule"; }
    
    // ���ڳ�ʱ�û��Ĵ���
    void simulate(NPU& npu, ProblemData& data, std::vector<int>& assigned_users, NpuSimulationResult& result) {
        LOG("%s module is running!", name().c_str());
        // һЩ������صĶ���
        int M = data.m_users, N = data.n_servers, A = data.mem_a, B = data.mem_b;
        int max_time = 5e5, server_id = npu.server_id, npu_id = npu.npu_id;
        int max_batch_size = (npu.memory - B) / A, memory = npu.memory;
        int max_block_count = npu.memory / (2 * B), max_block_time = npu.calculate_time(max_batch_size);
        int min_block_time = npu.calculate_time((npu.memory / max_block_count - B) / A);
        std::vector<User>& users = data.users;
        std::vector<std::vector<int>>& latency = data.latency;

        // ����֮ǰ��ģ����
        std::map<int, std::vector<Schedule>>& schedules = result.schedules; 
        std::vector<int>& completed_users = result.completed_users;
        std::vector<int>& timeout_users = result.timeout_users;
        std::map<int, int>& remaining_samples = result.remaining_samples;
        std::vector<int>& memory_usage = result.memory_usage; 
        memory_usage.resize(max_time + 100, 0);
        int& finish_time = result.finish_time;

        // ����һЩģ���������Ҫ��¼����Ϣ
        std::vector<int> remaining_send_count(M + 1, 300);
        std::priority_queue<arr2, std::vector<arr2>, std::greater<arr2>> waiting_users; // (time, user_id)
        std::priority_queue<arr2, std::vector<arr2>, std::greater<arr2>> available_users; // (priority, user_id)


        // ����һЩ�����Ժ���
        
        auto calculate_batch = [&](int mem) {return std::max(0, (mem - B) / A);};
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
        // ÿ���뿪ʼ��ά���ܷ��͵��û�
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

        // ���ͺ��ʣ��������ʣ�෢�ʹ������´��ܷ���ʱ�����
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

        // �ж��ܹ���ĳһ��ʱ��ڵ㷢��һ��batch size
        auto can_send = [&](int time, int user_id, int batch) {
            if (remaining_send_count[user_id] <= 0) return false;
            if (remaining_samples[user_id] - batch > (remaining_send_count[user_id] - 1) * max_batch_size) return false;
            return true;
        };

        // ��ĳ��ʱ��ڵ�ķ��Ͳ���
        auto send_strategy = [&](int time) {
            while (!available_users.empty()) {
                int free_batch_size = calculate_batch(memory - memory_usage[time]);
                if (free_batch_size <= 0) break;

                int user_id = available_users.top()[1];
                available_users.pop(); 

                if (finish_time <= time) { // �������ȼ���ߵ��û�
                    // ö��ʱ���
                    int min_time = min_block_time;
                    int best_batch_size = 1, best_block_time = 1;
                    double best_util = 0;
                    if (available_users.size() <= 2) min_time = max_block_time; 
                    for (int block_time = std::max(min_block_time, min_time); block_time <= max_block_time; block_time ++) {
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
                    
                } else { // ����ʣ����Ҫ�����û�
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
        
        
        // ģ��ǰ�ĳ�ʼ��
        for (auto& user_id: assigned_users) {
            remaining_samples[user_id] = data.users[user_id].cnt;
        }
        for (int user_id: assigned_users) {
            waiting_users.push({data.users[user_id].s + data.latency[server_id][user_id], user_id});
        }

        // ��ʼ����ģ��
        for (int time = finish_time; time <= max_time; time ++) {
            if (available_users.size() == 0 and waiting_users.size() == 0) break;
            // ���¿ɷ����û�
            avaliable_users_update(time);
            send_strategy(time);
        }

        // ͳ�Ƴ�ʱ�û�
        for(auto& v : remaining_samples) {
            timeout_users.push_back(v.first);
        }

    }
    
    
    SolverResult run(ProblemData& data, IteratorResult& iteratorResult) override {
        LOG("%s module is running!", name().c_str());
        // ���ڳ�ʱ�û��ķ���
        // ����װ�����bathsiz�������Ϊ��׼�����ܵĽ���ʱ�����
        
        // �ȶ�����Ժ���Ϣ
        int A = data.mem_a, B = data.mem_b;
        std::vector<std::vector<NPU> >& npus = data.npus;
        std::vector<std::vector<std::vector<int> > >& simulate_users = iteratorResult.simulate_users ;
        std::vector<std::vector<NpuSimulationResult> >& simulate_results = iteratorResult.simulate_results;       
        SolverResult result;
        result.solution.resize(data.m_users + 1);


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
            int cnt = data.users[v].cnt;
            int max_batch_size = (npus[server_id][npu_id].memory - B) / A;
            int handle_time = data.npus[server_id][npu_id].calculate_time(max_batch_size);
            cnt = cnt / max_batch_size * handle_time;
            prior.push({pri + cnt, server_id, npu_id});
            simulate_timeout_users[server_id][npu_id].push_back(v);
        }

        LOG("timeout users assign over!, ready for handle %d", simulate_timeout_users[1][1].size());

        for (int i = 1; i <= data.n_servers; i ++) {
            for (int j = 1; j < (int)data.npus[i].size(); j ++) {
                simulate(npus[i][j], data, simulate_timeout_users[i][j], simulate_results[i][j]);
            }
        }

        LOG("timeout users handle out!");
        for (int i = 1; i <= data.n_servers; i ++) {
            for (int j = 1; j < data.npus[i].size(); j ++) {
                for (auto& item: simulate_results[i][j].schedules) {
                    int user_id = item.first;
                    result.solution[user_id].insert(result.solution[user_id].end(), 
                        simulate_results[i][j].schedules[user_id].begin(), simulate_results[i][j].schedules[user_id].end());
                }
            }
        }

        LOG("finish users count: %d", result.completed_user_count);

        return result;
    };
};


// ===================================================================
// Solver��ػ��ඨ��
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


class AutoBathSizeBlockSolver : public Solver {
public:
    std::string name() const override { return "AutoBathSizeBlockSolver"; }
    
    SolverResult solve(ProblemData& data) override {
        LOG("Running %s...", name().c_str());
        NPUAutoBatchSizeBlockModule simulator;
        BruteIteratorModule iterator;
        AutoTimeBlockHandlerModule timeout_handler;
        IteratorResult iterator_result = iterator.run(data, simulator);
        
        return timeout_handler.run(data, iterator_result); 
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
    // solvers.push_back(std::make_unique<AutoBathSizeBlockSolver>());
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
        // **ע��: ʹ���µı�����**
        LOG("--- [Result] Solver: %s | Predicted Completed Users: %d ---", 
            solver->name().c_str(), result.completed_user_count);

        // **ע��: ʹ���µı�����**
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
    // **ע��: ʹ���µı�����**
    LOG("Predicted Max Completed Users: %d", max_completed_users);
    LOG("==============================================");

    Solver::print_solution(best_solution, data.m_users);

    auto program_end_time = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsed_seconds = program_end_time - program_start_time;
    fprintf(stderr, "Total Execution Time: %.4f seconds\n", elapsed_seconds.count());

    return 0;
}