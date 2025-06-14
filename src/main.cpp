#include <bits/stdc++.h>

#define READY 2
#define COMPLETED 1


using arr2 = std::array<int, 2>;
using arr3 = std::array<int, 3>;
using arr4 = std::array<int, 4>;


// 计数从1开始
int N;  // 服务器数量
int M; // 用户数量
int A, B; // 显存与batchsize之间的关系，Mem = a * bs + b


class NPU {
public:
    int severId; // 所属的服务器id
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
        this->severId = serverId;
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
    
};
std::vector<std::vector<NPU> > npus(11); // 最多11个服务器


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
        this->prior = 1e5 * requestCount / (this->endTime - this->startTime);
    }
};
std::vector<User> users;
std::vector<std::vector<int> > latency; // 用户将请求发送到服务器上的时延


void solve() {
    std::cin >> N;
    for (int i = 1; i <= N; i ++) {
        int gi, ki, mi; 
        std::cin >> gi >> ki >> mi;
        npus[i].resize(gi + 1);
        for (int j = 1; j <= gi; j ++) {
            npus[i][j] = NPU(i, j, mi, ki); 
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

    std::clog << "delay 1 3: " << latency[1][1] << std::endl; 
    std::cin >> A >> B;
    std::clog << A << " " << B << std::endl;
    
    
    std::vector<std::vector<std::array<int, 4> > > ans(M + 1); 
    // 最终输出(time, serverId, npuId, batchSize)
    std::vector<arr2> userNpu(M + 1); // 用户指定的npu;

    std::priority_queue<arr2, std::vector<arr2>, std::greater<arr2> > newUsers; 
    // 按照(time, userId)的格式排序，
    std::priority_queue<arr4, std::vector<arr4>, std::greater<arr4> > npuOffload; 
    // 自动按照 (offload, inferenceAbility, serverId, npuId)的标准进行排序

    std::priority_queue<arr2> needSendUsers;
    // 按照(prior, userId)的格式存储


    for (int i = 1; i <= M; i ++) {
        newUsers.push({users[i].startTime, i});
    }

    for (int i = 1; i <= N; i ++) {
        for (int j = 1; j < (int)npus[i].size(); j ++) {
            npus[i][j].init();
            npuOffload.push({0, -npus[i][j].inferenceAbility, i, j});
        }
    }

    // std::cout << newUsers.top()[0] << std::endl;
    int completedUserCount = 0;
    for (int currentTime = 0; currentTime <= 6e4; currentTime ++) {
        if (completedUserCount >= M) break;
        // std::cout << "current time: " << currentTime << std::endl;
        while (!newUsers.empty() and newUsers.top()[0] <= currentTime) {
            arr2 tp = newUsers.top();
            newUsers.pop();
            int userId = tp[1];
            needSendUsers.push({users[userId].prior, userId});
            // std::cout << "new user time: " << tp[0] << std::endl;
        }


        while (!needSendUsers.empty()) {
            int userId = needSendUsers.top()[1];
            int serverId = userNpu[userId][0], npuId = userNpu[userId][1];
            // std::cout << "userId, npu: " << userId << " " << serverId << " " << npuId << std::endl;
            if (serverId == 0 or npus[serverId][npuId].finishTime - currentTime >= 60) {
                // 满足此时条件，为当前用户寻找一个新的NPU
                // 如果找得到，就分配到新的npu上，如果找不到，后续用户也找不到对应npu,直接break
                arr4 topNpu = npuOffload.top();
                if (topNpu[0] - currentTime >= 20) {
                    // std::cout << "cannot find npu with less offload" << std::endl;
                    break;
                } else {
                    needSendUsers.pop();
                    serverId = topNpu[2], npuId = topNpu[3];
                    userNpu[userId] = {serverId, npuId};
                    
                    // std::cout << "new npuId: " << serverId << " " << npuId << std::endl;

                    NPU& npu = npus[serverId][npuId];
                    int sendTime = std::max(currentTime, npu.finishTime - latency[serverId][userId]);
                    int batchSize = std::min(npu.maxBatchSize, users[userId].remainCount);
                    int handleTime = npu.calculateInferenceTime(batchSize);
                    if (sendTime < users[userId].endTime) {
                        ans[userId].push_back({sendTime, serverId, npuId, batchSize});
                        users[userId].remainCount -= batchSize;
                    
                        npu.finishTime = std::max(npu.finishTime, sendTime + latency[serverId][userId]) + handleTime;
                        npuOffload.pop();
                        npuOffload.push({npu.finishTime, -npu.inferenceAbility, serverId, npuId});

                        // 如果当前用户的请求未完成，就在之后再维护这个用户的信息
                        if (users[userId].remainCount > 0) {
                            // std::cout << 
                            newUsers.push({sendTime + latency[serverId][userId] + 1, userId});
                        }

                    } 
                    
                }
            } else {
                // 当前npu的负载较低，可以发送请求
                needSendUsers.pop();
                NPU& npu = npus[serverId][npuId];
                int sendTime = std::max(currentTime, npu.finishTime - latency[serverId][userId]);
                int batchSize = std::min(npu.maxBatchSize, users[userId].remainCount);
                int handleTime = npu.calculateInferenceTime(batchSize);
                
                if (sendTime < users[userId].endTime) {
                    ans[userId].push_back({sendTime, serverId, npuId, batchSize});
                    users[userId].remainCount -= batchSize;
                
                    npu.finishTime = std::max(npu.finishTime, sendTime + latency[serverId][userId]) + handleTime;
                    npuOffload.pop();
                    npuOffload.push({npu.finishTime, -npu.inferenceAbility, serverId, npuId});

                    // 如果当前用户的请求未完成，就在之后再维护这个用户的信息
                    if (users[userId].remainCount > 0) {
                        // std::cout << 
                        newUsers.push({sendTime + latency[serverId][userId] + 1, userId});
                    }

                } 
            }
        }

    }


    // for (int i = 1; i <= M; i ++) { 
    //     sort(ans[i].begin(), ans[i].end());
    //     // std::cout << ans[i].size() << std::endl;
    //     for (int j = 0; j < (int)ans[i].size(); j ++) {
    //         // std::cout << ans[i][j][0] << " " << ans[i][j][1] << " "
    //         //     << ans[i][j][2] << " " << ans[i][j][3] << " ";
    //         if (ans[i][j][0] < users[i].startTime) {
    //             std::vector<NPU> allnpus(1e9);
    //         }
    //         if (j) {
    //             int d = ans[i][j][0] - ans[i][j - 1][0];
    //             if (d < 0) {
    //                 // std::vector<NPU> allnpus(1e9);
    //             }
    //         }
    //     }
    //     // std::cout << std::endl;
    // }


    // 如果此时还有用户的请求没有完成，在原本的npu上分配剩余的任务，
    for (int i = 1; i <= M; i ++) {
        int serverId = userNpu[i][0], npuId = userNpu[i][1];
        if (serverId == 0) {
            serverId = 1, npuId = 1;
        }
        int startTime = 6e4 + 21;
        while (users[i].remainCount > 0) {
            int batchSize = std::min(users[i].remainCount, npus[serverId][npuId].maxBatchSize);
            users[i].remainCount -= batchSize;
            startTime += (latency[serverId][i] + 1);
            ans[i].push_back({startTime, serverId, npuId, batchSize});
        }
    }

    for (int i = 1; i <= M; i ++) { 
        sort(ans[i].begin(), ans[i].end());
        std::cout << ans[i].size() << std::endl;
        for (int j = 0; j < (int)ans[i].size(); j ++) {
            // if (ans[i][j][0] < users[i].startTime) {
            //     std::vector<NPU> allnpus(1e9);
            // }
            
            std::cout << ans[i][j][0] << " " << ans[i][j][1] << " "
                << ans[i][j][2] << " " << ans[i][j][3] << " ";
            if (j) {
                int d = ans[i][j][0] - ans[i][j - 1][0];
                if (d <= latency[ans[i][j][1]][i]) {
                    // std::vector<NPU> allnpus(1e9);
                }
            }
        }
        std::cout << std::endl;
    }
}



int main() {
    // std::ios::sync_with_stdio(false); std::cin.tie(0); std::clog.tie(0);
    solve();
    return 0;
}
