#include <bits/stdc++.h>

#define READY 2
#define COMPLETED 1


using arr2 = std::array<int, 2>;
using arr3 = std::array<int, 3>;
using arr4 = std::array<int, 4>;


// 计数从1开始
int N;  // 服务器数量
int M; // 用户数量
int A, B, G; // 显存与batchsize之间的关系，Mem = a * bs + b


class User {
public:
    int id; // 用户id
    int startTime; // 请求开始时间
    int endTime; // 请求结束时间
    int requestCount; // 请求数量
    int remainCount; // 剩余的未完成的请求数量

    int prior; // 用户的优先级
    User(){};
    User(int id, int startTime, int endTime, int requestCount) {
        this->id = id;
        this->startTime = startTime;
        this->endTime = endTime;
        this->requestCount = requestCount;
        this->remainCount = requestCount;
        this->prior = this->endTime;
    }
};
std::vector<User> users;
std::vector<std::vector<int> > latency; // 用户将请求发送到服务器上的时延

class NPU {
public:
    int serverId; // 所属的服务器id
    int id; // 编号
    int totalMemory; // 显存容量（单位：GB）
    int maxBatchSize; // 最大的batch size
    int freeBatchSize;  // 剩余显存（单位：GB）
    int inferenceSpeed; // 推理速度（单位：推理/秒）
    int inferenceAbility; // 推理能力， sqrt(bc) * k;
    int finishTime; // 请求完成时间，默认为0


    int calculateInferenceTime (int batchSize) {
        return ceil(sqrtl(batchSize) / inferenceSpeed);
    }

    
    NPU(){};
    NPU(int serverId, int id, int totalMemory, int inferenceSpeed) {
        this->serverId = serverId;
        this->id = id;
        this->totalMemory = totalMemory;
        this->inferenceSpeed = inferenceSpeed;
    } 

    void init() {
        this->maxBatchSize = (totalMemory - B) / A;
        this->freeBatchSize = this->maxBatchSize;
        this->inferenceAbility = sqrtl(this->maxBatchSize) * inferenceSpeed;
        this->finishTime = 0;
    }

    // int stimulate(std::vector<int>& stimulateUsers) {
    //     // 在该NPU上模拟0-6e4时间内的执行情况，返回完成请求的用户数量
    //     int completedCount = 0;
    //     std::vector<User> usersCopy = users; // 进行信息备份,防止信息覆盖
    //     std::priority_queue<arr2, std::vector<arr2>, std::greater<arr2> > newUsers; 
    //     // 按照(请求到达时间, userId)的格式排序，
    //     std::priority_queue<arr2> needSendUsers; 
    //     // 按照(prior, userId)的格式存储
    //     int finishTime = 0, maxTime = 6e4;  
    //     // std::vector<std::vector<std::array<int, 4> > > ans(M + 1);

    //     for (int userId: stimulateUsers) {
    //         newUsers.push({users[userId].startTime + latency[serverId][userId], userId});
    //     }
        
    //     // std::cout << "start stimulate" << std::endl;

    //     for (int arriveTime = 0; arriveTime <= maxTime; arriveTime ++) {
    //         if (arriveTime < finishTime) continue;
    //         while (!newUsers.empty() and newUsers.top()[0] <= arriveTime) {
    //             // std::cout << 1 << std::endl;
    //             arr2 tp = newUsers.top();
    //             newUsers.pop();
    //             int userId = tp[1];
    //             needSendUsers.push({users[userId].prior, userId});
    //         }

    //         int npuResBatchSize = this->maxBatchSize;
    //         int sendBatchSize = 0;
    //         int sendHandleTime = 0;

    //         while (npuResBatchSize == maxBatchSize and !needSendUsers.empty()) {
    //             // 如果存在能够在当前时刻到达的用户请求，就进行发送
    //             int userId = needSendUsers.top()[1];
    //             needSendUsers.pop();

    //             int batchSize = std::min(users[userId].remainCount, maxBatchSize);
                
    //             // if (N == 1 and G == 1) batchSize = std::min(batchSize, 25);
    //             int handleTime = calculateInferenceTime(batchSize);
    //             int sendTime = arriveTime - latency[serverId][userId];

    //             // 如果当前用户的预测结束时间已经超时，那么该用户就不再发送任务
    //             int cnt = users[userId].remainCount / maxBatchSize;
    //             int resBatchSize = users[userId].remainCount - cnt * maxBatchSize;
    //             int maxBatchHandleTime = calculateInferenceTime(maxBatchSize);
    //             int resBatchHandleTime = calculateInferenceTime(resBatchSize);
    //             int predictEndTime = std::max(latency[serverId][userId], maxBatchHandleTime) * cnt 
    //                 + resBatchHandleTime + sendTime;

    //             if (predictEndTime <= users[userId].endTime) {
    //                 // std::clog << userId << std::endl;
    //                 ans[userId].push_back({sendTime, serverId, id, batchSize});
    //                 users[userId].remainCount -= batchSize;
    //                 npuResBatchSize -= batchSize;
    //                 sendBatchSize = batchSize;
    //                 sendHandleTime = handleTime;
    //                 finishTime = arriveTime + handleTime;

    //                 if (users[userId].remainCount > 0) {
    //                     newUsers.push({arriveTime + latency[serverId][userId] + 1, userId});
    //                 } else {
    //                     completedCount += 1;
    //                 }
    //             }

    //         }

            // 考虑batchsize小而多的情况，当有一个发送请求后，要把memory尽可能占满
            // while (npuResBatchSize > 0 and !needSendUsers.empty()) {
            //     int userId = needSendUsers.top()[1];
            //     needSendUsers.pop();
            //     // 推理时间计算公式为sqrt(bs) / k, 
                
            //     int batchSize = (sendHandleTime * inferenceSpeed) * (sendHandleTime * inferenceSpeed);
            //     batchSize = std::min(batchSize, npuResBatchSize);
            //     batchSize = std::min(batchSize, users[userId].remainCount);
            //     // batchSize = std::min(batchSize, 30);
            //     int sendTime = arriveTime - latency[serverId][userId];

            //     int cnt = users[userId].remainCount / maxBatchSize;
            //     int resBatchSize = users[userId].remainCount - cnt * maxBatchSize;
            //     int maxBatchHandleTime = calculateInferenceTime(maxBatchSize);
            //     int resBatchHandleTime = calculateInferenceTime(resBatchSize);
            //     int predictEndTime = std::max(latency[serverId][userId], maxBatchHandleTime) * cnt 
            //         + resBatchHandleTime + sendTime;

            //     if (predictEndTime <= users[userId].endTime) {
            //         // std::clog << userId << std::endl;
            //         ans[userId].push_back({sendTime, serverId, id, batchSize});
            //         users[userId].remainCount -= batchSize;
            //         npuResBatchSize -= batchSize;

            //         if (users[userId].remainCount > 0) {
            //             newUsers.push({arriveTime + latency[serverId][userId] + 1, userId});
            //         } else {
            //             // completedUsers.push_back(userId);
            //             completedCount += 1;
            //         }
            //     }
            // }


        // }
        // std::clog << "completed count: " << completedCount << std::endl; 

        // return completedCount;
    // }
    
};
std::vector<std::vector<NPU> > npus(11); // 最多11个服务器


class Simulator {
public:
    int serverId; // 服务器Id
    int npuId; // npu的Id
    int maxTime; // 模拟的最大时刻
    int totalMemory; // 显存容量（单位：GB
    int maxBatchSize;
    int inferenceSpeed; // 推理速度（单位：推理/秒）
    std::vector<int> stimulateUsers;
    std::vector<int> sumMemory; // 显存占用的前缀和
    std::vector<int> completedUsers;
    std::vector<int> timeoutUsers;
    std::vector<int> memeoryUsage;
    std::vector<std::vector<std::array<int, 4> > > ans;

    Simulator(){};    
    Simulator(int serverId, int npuId, int totalMemory, int inferenceSpeed, int maxTime = 6e4) {
        this->serverId = serverId;
        this->npuId = npuId;
        this->totalMemory = totalMemory;
        this->inferenceSpeed = inferenceSpeed;
        this->maxTime = maxTime;
        this->maxBatchSize = (totalMemory - B) / A;
        init();
    }

    void init() {
        sumMemory = std::vector<int>(maxTime + 1, 0);
        completedUsers.clear();
        timeoutUsers.clear();
        memeoryUsage = std::vector<int>(maxTime + 1, 0);
        ans = std::vector<std::vector<std::array<int, 4> > >(M + 1);
    }

    int calculateInferenceTime (int batchSize) {
        return ceil(sqrtl(batchSize) / inferenceSpeed);
    }

    void stimulate() {
        init();
        std::unordered_map<int, int> userRemainCount;
        for (int userId: stimulateUsers) {
            userRemainCount[userId] = users[userId].remainCount;
        }

        std::priority_queue<arr2, std::vector<arr2>, std::greater<arr2> > waitingUsers; 
        // 按照(请求到达时间, userId)的格式排序，
        std::priority_queue<arr2, std::vector<arr2>, std::greater<arr2> > availableUsers; 
        // 按照(prior, userId)的格式存储
        int finishTime = 0;  

        for (int userId: stimulateUsers) {
            waitingUsers.push({users[userId].startTime + latency[serverId][userId], userId});
        }

        for (int arriveTime = 0; arriveTime <= maxTime; arriveTime ++) {
            if (arriveTime < finishTime) continue;
            while (!waitingUsers.empty() and waitingUsers.top()[0] <= arriveTime) {
                arr2 tp = waitingUsers.top();
                waitingUsers.pop();
                int userId = tp[1];
                availableUsers.push({users[userId].prior, userId});
            }

            int npuResBatchSize = this->maxBatchSize;
            int sendBatchSize = 0;
            int sendHandleTime = 0;

            while (npuResBatchSize == maxBatchSize and !availableUsers.empty()) {
                // 如果存在能够在当前时刻到达的用户请求，就进行发送
                int userId = availableUsers.top()[1];
                availableUsers.pop();

                int batchSize = std::min(userRemainCount[userId], maxBatchSize);
                int handleTime = calculateInferenceTime(batchSize);
                int sendTime = arriveTime - latency[serverId][userId];

                // 如果当前用户的预测结束时间已经超时，那么该用户就不再发送任务
                int cnt = userRemainCount[userId] / maxBatchSize;
                int resBatchSize = userRemainCount[userId] - cnt * maxBatchSize;
                int maxBatchHandleTime = calculateInferenceTime(maxBatchSize);
                int resBatchHandleTime = calculateInferenceTime(resBatchSize);
                int predictEndTime = std::max(latency[serverId][userId], maxBatchHandleTime) * cnt 
                    + resBatchHandleTime + sendTime;

                if (predictEndTime <= users[userId].endTime) {
                    ans[userId].push_back({sendTime, serverId, npuId, batchSize});
                    userRemainCount[userId] -= batchSize;
                    npuResBatchSize -= batchSize;
                    sendBatchSize = batchSize;
                    sendHandleTime = handleTime;
                    finishTime = arriveTime + handleTime;

                    // 维护显存占用
                    for(int i = arriveTime; i < finishTime; i ++) {
                        memeoryUsage[i] += batchSize;
                    }


                    if (userRemainCount[userId] > 0) {
                        waitingUsers.push({arriveTime + latency[serverId][userId] + 1, userId});
                    } else {
                        completedUsers.push_back(userId);
                        userRemainCount.erase(userId);
                    }
                }

            }
        }
        for(auto it = userRemainCount.begin(); it != userRemainCount.end(); it ++) {
            timeoutUsers.push_back(it->first);
        }

        // 计算显存的累积和
        for(int i = 1; i <= maxTime; i ++) {
            sumMemory[i] = sumMemory[i - 1] + memeoryUsage[i];
        }
        
    }

    int queryFreeMemory(int l, int r) {
        return maxBatchSize * (r - l) - (sumMemory[r - l] - sumMemory[l - 1]);
    }
};

// struct cmpUser {
//     bool operator()(int id1, int id2) const {
//         return users[id1].requestCount < users[id2].requestCount;
//     }
// };

void solve() {
    std::cin >> N;
    for (int i = 1; i <= N; i ++) {
        int gi, ki, mi; 
        std::cin >> gi >> ki >> mi;
        G = gi;
        npus[i].resize(gi + 1);
        for (int j = 1; j <= gi; j ++) {
            npus[i][j] = NPU(i, j, mi, ki); 
            // npus[i][j].init();
        }
    }

    std::cin >> M;
    users.resize(M + 1);
    for (int i = 1; i <= M; i ++) {
        int si, ei, cnti; 
        std::cin >> si >> ei >> cnti;
        users[i] = User(i, si, ei, cnti);
    }

    latency.resize(N + 1, std::vector<int>(M + 1));
    for (int i = 1; i <= N; i ++) {
        for (int j = 1; j <= M; j ++) {
            std::cin >> latency[i][j];
        }
    }

    std::cin >> A >> B;
    std::clog << A << " " << B << std::endl;

    std::vector<std::vector<std::array<int, 4> > > ans(M + 1);
    std::vector<std::vector<Simulator> > simulators(N + 1);
    
    for (int i = 1; i <= N; i ++) {
        simulators[i].resize(npus[i].size());
        for (int j = 1; j <(int)npus[i].size(); j ++) {
            npus[i][j].init();
            simulators[i][j] = Simulator(i, j, npus[i][j].totalMemory, npus[i][j].inferenceSpeed);
        }
        
    } 

    std::vector<int> userOrder, timeoutUsers;
    for (int i = 1; i <= M; i ++) {
        userOrder.push_back(i);
    }
    std::sort(userOrder.begin(), userOrder.end(), [&](int id1, int id2) {
        return users[id1].requestCount < users[id2].requestCount;
    });
    
    for (int& userId: userOrder) {
        bool assignSuccess = false;
        for (int i = 1; i <= N; i ++) {
            if (assignSuccess) continue;
            for (int j = 1; j < (int)npus[i].size(); j ++) {
                if (assignSuccess) continue;
                int l = users[i].startTime + latency[i][userId];
                int r = users[i].endTime;
                int queryBatchSize = simulators[i][j].queryFreeMemory(l, r);
                if (queryBatchSize >= users[i].requestCount) {
                    std::vector<int> originCompletedUsers = simulators[i][j].completedUsers;
                    simulators[i][j].stimulateUsers = simulators[i][j].completedUsers;
                    simulators[i][j].stimulateUsers.push_back(userId);
                    simulators[i][j].stimulate();
                    if ((int)simulators[i][j].timeoutUsers.size() > 0) {
                        simulators[i][j].completedUsers = originCompletedUsers;
                        continue;
                    } else {
                        assignSuccess = true;
                    }
                }
            }
        }

        if (!assignSuccess) timeoutUsers.push_back(userId);
    }

    

    for (int i = 1; i <= N; i ++) {
        for (int j = 1; j <(int)npus[i].size(); j ++) {
            simulators[i][j].stimulateUsers = simulators[i][j].completedUsers;
            simulators[i][j].stimulate();
            for (int k = 1; k <= M; k ++) {
                ans[k].insert(ans[k].end(), simulators[i][j].ans[k].begin(), simulators[i][j].ans[k].end());
            }
        }
    }
    
    std::clog << "timeout count: " << timeoutUsers.size() << std::endl;
    
    for (int &i: timeoutUsers) {
        int startTime = 6e4 + 21;
        while (users[i].remainCount > 0) {
            int batchSize = std::min(users[i].remainCount, npus[1][1].maxBatchSize);
            users[i].remainCount -= batchSize;
            startTime += (latency[1][i] + 1);
            ans[i].push_back({startTime, 1, 1, batchSize});
        }
    }


    for (int i = 1; i <= M; i ++) { 
        sort(ans[i].begin(), ans[i].end());
        int cnt = std::min((int)ans[i].size(), 300);
        
        std::cout << cnt << std::endl;
        // assert((int)ans[i].size() <= 300);

        for (int j = 0; j < cnt; j ++) {
            std::cout << ans[i][j][0] << " " << ans[i][j][1] << " "
                << ans[i][j][2] << " " << ans[i][j][3] << " ";
        }
        std::cout << std::endl;
    }
}



int main() {
    // std::ios::sync_with_stdio(false); std::cin.tie(0); std::clog.tie(0);
    solve();
    return 0;
}
