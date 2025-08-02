// validator.cpp
// C++版高性能验证与评分脚本 (新评分规则版)

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <cmath>
#include <numeric>
#include <algorithm>
#include <set>
#include <iomanip>

// --- 数据结构定义 (无变化) ---
struct LogEntry {
    long long time;
    int npu_global_id, server_id, npu_local_id, used_memory, max_memory;
    int queue_size, running_tasks_count, completed_batch_size_npu;
    long long cumulative_batch_size;
    int cumulative_users_completed_on_time, cumulative_users_timeout;
};
struct Request {
    int user_id, server_id, npu_id, batch_size;
    long long send_time, arrival_time;
};
struct NPU {
    int server_id, local_id, memory_limit, used_memory = 0;
    std::vector<Request> queue;
    struct RunningTask { Request request; long long finish_time; int memory_used; };
    std::vector<RunningTask> running_tasks;
};
struct Server {
    int id, npu_count, k, memory;
    std::vector<int> npu_global_ids;
};
struct User {
    int id, cnt_required, cnt_processed = 0, last_npu_global_id = -1, migrations = 0;
    int a, b;
    long long s, e, finish_time = -1, next_allowed_send_time;
    bool has_finished = false;
    std::vector<Request> requests;
};

// --- 核心模拟器类 (无变化) ---
class Simulator {
public:
    Simulator(const std::string& input_path, const std::string& output_path);
    void run();
    void calculate_score();
    void save_log_file(const std::string& filepath = "simulation_log.txt");

private:
    std::vector<Server> servers;
    std::map<int, User> users;
    std::map<int, NPU> npu_map;
    std::map<std::pair<int, int>, int> latencies;
    long long total_samples_to_process = 0;
    std::vector<LogEntry> log_data;
    long long cumulative_batch_size_processed = 0;
    int cumulative_users_completed_on_time = 0;
    int cumulative_users_timeout = 0;
    enum class EventType { SEND, ARRIVE };
    std::map<long long, std::vector<std::pair<EventType, Request>>> events;
    void parse_input(const std::string& path);
    void parse_output(const std::string& path);
    void populate_initial_events();
    void fail_with_error(const std::string& error_type, const std::string& message);
};

// --- 主程序 (无变化) ---
int main(int argc, char* argv[]) {
    std::ios_base::sync_with_stdio(false);
    if (argc != 3) {
        std::cerr << "\nUsage: " << argv[0] << " <input.txt> <output.txt>\n";
        return 1;
    }
    try {
        Simulator simulator(argv[1], argv[2]);
        simulator.run();
        simulator.calculate_score();
        simulator.save_log_file();
    } catch (const std::exception& e) {
        std::cerr << "\nAn unexpected error occurred: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}

// --- Simulator类方法实现 ---
// ... parse_input, parse_output, run, populate_initial_events, fail_with_error, save_log_file 等函数均无变化 ...
// ... 此处省略未修改的函数，以突出重点 ...
Simulator::Simulator(const std::string& input_path, const std::string& output_path) {
    std::cout << "--- 1. Parsing Input File ---" << std::endl;
    parse_input(input_path);
    std::cout << "Input parsing complete." << std::endl;
    std::cout << "--- 2. Parsing Output File ---" << std::endl;
    parse_output(output_path);
    std::cout << "Output parsing and validation complete." << std::endl;
    populate_initial_events();
}

void Simulator::fail_with_error(const std::string& error_type, const std::string& message) {
    std::cerr << "\nValidation Failed: [" << error_type << "]" << std::endl;
    std::cerr << "Details: " << message << std::endl;
    exit(1);
}

void Simulator::parse_input(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) fail_with_error("Input File Error", "Could not open file: " + path);
    int n_servers; file >> n_servers;
    int npu_global_id_counter = 0;
    for (int i = 0; i < n_servers; ++i) {
        Server s; s.id = i + 1;
        file >> s.npu_count >> s.k >> s.memory;
        for (int j = 0; j < s.npu_count; ++j) {
            npu_map[npu_global_id_counter] = NPU{s.id, j + 1, s.memory};
            s.npu_global_ids.push_back(npu_global_id_counter++);
        }
        servers.push_back(s);
    }
    int m_users; file >> m_users;
    for (int i = 0; i < m_users; ++i) {
        User u; u.id = i + 1;
        file >> u.s >> u.e >> u.cnt_required;
        u.next_allowed_send_time = u.s;
        users[u.id] = u;
        total_samples_to_process += u.cnt_required;
    }
    for (int i = 0; i < n_servers; ++i) {
        for (int j = 0; j < m_users; ++j) {
            int lat; file >> lat;
            latencies[{j + 1, i + 1}] = lat;
        }
    }
    for (int i = 0; i < m_users; ++i) {
        int user_id = i + 1;
        if (users.count(user_id)) {
            file >> users.at(user_id).a >> users.at(user_id).b;
        }
    }
}

void Simulator::parse_output(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) fail_with_error("Output File Error", "Could not open file: " + path);
    std::string line;
    for (size_t i = 0; i < users.size(); ++i) {
        int user_id = i + 1; User& user = users.at(user_id);
        int t_i;
        if (!(file >> t_i)) fail_with_error("Output File Format", "Could not read T_i for user " + std::to_string(user_id));
        if (t_i < 1 || t_i > 300) fail_with_error("Invalid Output Constraint", "User " + std::to_string(user_id) + ": T_i=" + std::to_string(t_i) + " is not in range [1, 300].");
        std::getline(file, line);
        if (!std::getline(file, line)) fail_with_error("Output File Format", "Missing request line for user " + std::to_string(user_id));
        std::stringstream ss(line);
        long long user_total_samples = 0;
        long long temp_next_allowed_send_time = user.s; 
        for (int j = 0; j < t_i; ++j) {
            Request req; req.user_id = user_id;
            if (!(ss >> req.send_time >> req.server_id >> req.npu_id >> req.batch_size)) fail_with_error("Output File Format", "User " + std::to_string(user_id) + ": Not enough integers for request " + std::to_string(j+1));
            if (req.send_time < temp_next_allowed_send_time) fail_with_error("Invalid User Send Time (Static Check)", "User " + std::to_string(user_id) + " req " + std::to_string(j+1) + ": time " + std::to_string(req.send_time) + " < " + std::to_string(temp_next_allowed_send_time) + ".");
            if (req.send_time > 1000000) fail_with_error("Invalid User Send Time", "User " + std::to_string(user_id) + " req " + std::to_string(j + 1) + ": time " + std::to_string(req.send_time) + " > 1,000,000.");
            if (req.server_id < 1 || (size_t)req.server_id > servers.size()) fail_with_error("Invalid Server Index", "User " + std::to_string(user_id) + " req " + std::to_string(j + 1) + ": server index " + std::to_string(req.server_id) + " out of bounds.");
            const Server& server = servers[req.server_id - 1];
            if (req.npu_id < 1 || req.npu_id > server.npu_count) fail_with_error("Invalid NPU Index", "User " + std::to_string(user_id) + " req " + std::to_string(j + 1) + ": NPU index " + std::to_string(req.npu_id) + " out of bounds for server " + std::to_string(req.server_id));
            if (user.a * req.batch_size + user.b > server.memory) fail_with_error("Batchsize Exceeds Memory", "User " + std::to_string(user_id) + " req " + std::to_string(j + 1) + ": batch size " + std::to_string(req.batch_size) + " exceeds server " + std::to_string(req.server_id) + "'s memory.");
            user_total_samples += req.batch_size;
            req.arrival_time = req.send_time + latencies.at({user_id, req.server_id});
            user.requests.push_back(req);
            temp_next_allowed_send_time = req.arrival_time + 1;
        }
        if (user_total_samples != user.cnt_required) fail_with_error("Samples Not Fully Processed", "User " + std::to_string(user_id) + ": Total batch sizes sum to " + std::to_string(user_total_samples) + ", but " + std::to_string(user.cnt_required) + " were required.");
    }
}

void Simulator::populate_initial_events() {
    for(auto const& [id, user] : users) {
        for(const auto& req : user.requests) {
            events[req.send_time].push_back({EventType::SEND, req});
        }
    }
}

void Simulator::run() {
    std::cout << "--- 3. Starting Simulation ---" << std::endl;
    long long current_time = 0;
    long long processed_samples = 0;
    std::set<int> npus_to_re_evaluate;
    int last_percent = -1;
    const int BAR_WIDTH = 50;
    while (processed_samples < total_samples_to_process) {
        if (current_time > 2000000) fail_with_error("Simulation Timeout", "Simulation exceeded maximum time limit (2,000,000ms).");
        npus_to_re_evaluate.clear();
        std::map<int, int> completed_batch_per_npu_tick;
        for (auto& [gid, npu] : npu_map) {
            auto it = npu.running_tasks.begin();
            while (it != npu.running_tasks.end()) {
                if (it->finish_time == current_time) {
                    npus_to_re_evaluate.insert(gid);
                    User& user = users.at(it->request.user_id);
                    completed_batch_per_npu_tick[gid] += it->request.batch_size;
                    cumulative_batch_size_processed += it->request.batch_size;
                    if (!user.has_finished && user.cnt_processed + it->request.batch_size >= user.cnt_required) {
                        user.has_finished = true;
                        if (current_time <= user.e) {
                            cumulative_users_completed_on_time++;
                        } else {
                            cumulative_users_timeout++;
                        }
                    }
                    user.cnt_processed += it->request.batch_size;
                    processed_samples += it->request.batch_size;
                    if (user.cnt_processed >= user.cnt_required) user.finish_time = current_time;
                    npu.used_memory -= it->memory_used;
                    it = npu.running_tasks.erase(it);
                } else ++it;
            }
        }
        if (events.count(current_time)) {
            for (const auto& [type, payload] : events.at(current_time)) {
                if (type == EventType::SEND) {
                    User& user = users.at(payload.user_id);
                    if (payload.send_time < user.next_allowed_send_time) fail_with_error("Invalid User Send Time (Runtime Check)", "User " + std::to_string(user.id) + " violation at t=" + std::to_string(payload.send_time));
                    user.next_allowed_send_time = payload.arrival_time + 1;
                    const auto& server = servers[payload.server_id - 1];
                    int npu_global_id = server.npu_global_ids[payload.npu_id - 1];
                    if (user.last_npu_global_id != -1 && user.last_npu_global_id != npu_global_id) user.migrations++;
                    user.last_npu_global_id = npu_global_id;
                    events[payload.arrival_time].push_back({EventType::ARRIVE, payload});
                } else if (type == EventType::ARRIVE) {
                    const auto& server = servers[payload.server_id - 1];
                    int gid = server.npu_global_ids[payload.npu_id - 1];
                    npu_map.at(gid).queue.push_back(payload);
                    npus_to_re_evaluate.insert(gid);
                }
            }
            events.erase(current_time);
        }
        for (int gid : npus_to_re_evaluate) {
            auto& npu = npu_map.at(gid);
            std::sort(npu.queue.begin(), npu.queue.end(), [](const Request& a, const Request& b){ if (a.arrival_time != b.arrival_time) return a.arrival_time < b.arrival_time; return a.user_id < b.user_id; });
            auto it = npu.queue.begin();
            while (it != npu.queue.end()) {
                const User& user = users.at(it->user_id);
                int mem_needed = user.a * it->batch_size + user.b;
                if (npu.used_memory + mem_needed <= npu.memory_limit) {
                    npu.used_memory += mem_needed;
                    const auto& server = servers[npu.server_id - 1];
                    double inference_speed = (it->batch_size > 0) ? server.k * std::sqrt(it->batch_size) : 1.0;
                    long long time_needed = (inference_speed > 0) ? static_cast<long long>(std::ceil(it->batch_size / inference_speed)) : 0;
                    npu.running_tasks.push_back({*it, current_time + time_needed, mem_needed});
                    it = npu.queue.erase(it);
                } else ++it;
            }
        }
        
        std::set<int> all_changed_npus = npus_to_re_evaluate;
        for(const auto& [gid, _] : completed_batch_per_npu_tick) {
            all_changed_npus.insert(gid);
        }
        if (!all_changed_npus.empty()) {
            for (const auto& gid : all_changed_npus) {
                const auto& npu = npu_map.at(gid);
                int completed_batch_npu = completed_batch_per_npu_tick.count(gid) ? completed_batch_per_npu_tick[gid] : 0;
                log_data.push_back({
                    current_time, gid, npu.server_id, npu.local_id,
                    npu.used_memory, npu.memory_limit, static_cast<int>(npu.queue.size()),
                    static_cast<int>(npu.running_tasks.size()), completed_batch_npu,
                    cumulative_batch_size_processed, cumulative_users_completed_on_time,
                    cumulative_users_timeout
                });
            }
        }
        
        int percent = (total_samples_to_process > 0) ? static_cast<int>(100.0 * processed_samples / total_samples_to_process) : 100;
        if (percent > last_percent) {
            last_percent = percent;
            std::cout << "[";
            int pos = BAR_WIDTH * percent / 100;
            for (int i = 0; i < BAR_WIDTH; ++i) {
                if (i < pos) std::cout << "="; else if (i == pos) std::cout << ">"; else std::cout << " ";
            }
            std::cout << "] " << percent << "% | Time: " << current_time << "ms\r";
            std::cout.flush();
        }
        current_time++;
    }
    std::cout << std::endl;
    std::cout << "Simulation Finished at time: " << current_time - 1 << " ms." << std::endl;
}


void Simulator::calculate_score() {
    std::cout << "--- 4. Calculating Score ---" << std::endl;
    double total_score = 0;
    int late_users_count = 0;

    for (auto const& [id, user] : users) {
        if (user.finish_time > user.e) {
            late_users_count++;
        }
        if (user.finish_time == -1) {
            // 如果有用户未完成所有请求，这是一个逻辑错误，但为了评分健壮性，可以给一个极大的finish_time
            // fail_with_error("Scoring Error", "User " + std::to_string(id) + " did not finish.");
        }
    }

    // 【修改】应用新的评分规则
    for (auto const& [id, user] : users) {
        long long final_finish_time = user.finish_time;
        if (final_finish_time == -1) {
            // 对于未完成的用户，给予极大惩罚
            final_finish_time = 999999; 
        }

        double h_arg = 0;
        if (user.e > user.s) {
            h_arg = static_cast<double>(final_finish_time - user.e) / (user.e - user.s);
        } else if (final_finish_time > user.e) {
            h_arg = std::numeric_limits<double>::infinity();
        }

        double h_val = std::pow(2.0, -h_arg / 100.0);
        double p_val = std::pow(2.0, -static_cast<double>(user.migrations) / 200.0);
        
        // 新增 q(i) 因子
        double q_val = std::pow(2.0, -static_cast<double>(user.id) / 5000.0);
        
        total_score += h_val * p_val * q_val * 10000.0;
    }

    double k_penalty = std::pow(2.0, -static_cast<double>(late_users_count) / 100.0);
    double final_score = k_penalty * total_score;

    std::cout << std::fixed << std::setprecision(4);
    std::cout << "\n--- Scoring Summary ---" << std::endl;
    std::cout << "Total Users: " << users.size() << std::endl;
    std::cout << "Late Users (K): " << late_users_count << std::endl;
    std::cout << "K Penalty h(K): " << k_penalty << std::endl;
    std::cout << "Sum of User Scores (before K penalty): " << total_score << std::endl;
    std::cout << "-------------------------" << std::endl;
    std::cout << "FINAL SCORE: " << final_score << std::endl;
    std::cout << "-------------------------" << std::endl;
}

void Simulator::save_log_file(const std::string& filepath) {
    // ... 此函数无变化 ...
}