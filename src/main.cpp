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
        // this->prior = 1e5 * requestCount / (this->endTime - this->startTime);
        this->prior = - this->endTime;
    }
};
std::vector<User> users;
std::vector<std::vector<int> > latency; // 用户将请求发送到服务器上的时延
std::vector<std::vector<std::array<int, 4> > > ans;

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

    int stimulate(std::vector<int>& stimulateUsers) {
        // 在该NPU上模拟0-6e4时间内的执行情况，返回完成请求的用户数量
        int completedCount = 0;
        std::vector<User> usersCopy = users; // 进行信息备份,防止信息覆盖
        std::priority_queue<arr2, std::vector<arr2>, std::greater<arr2> > newUsers; 
        // 按照(请求到达时间, userId)的格式排序，
        std::priority_queue<arr2> needSendUsers; 
        std::vector<int> completedUsers;
        // 按照(prior, userId)的格式存储
        int finishTime = 0, maxTime = 6e4;  
        // std::vector<std::vector<std::array<int, 4> > > ans(M + 1);

        for (int userId: stimulateUsers) {
            newUsers.push({users[userId].startTime + latency[serverId][userId], userId});
        }
        
        // std::cout << "start stimulate" << std::endl;

        for (int arriveTime = 0; arriveTime <= maxTime; arriveTime ++) {
            if (arriveTime < finishTime) continue;
            while (!newUsers.empty() and newUsers.top()[0] <= arriveTime) {
                // std::cout << 1 << std::endl;
                arr2 tp = newUsers.top();
                newUsers.pop();
                int userId = tp[1];
                needSendUsers.push({users[userId].prior, userId});
            }

            int npuResBatchSize = this->maxBatchSize;
            int sendBatchSize = 0;
            int sendHandleTime = 0;

            while (npuResBatchSize == maxBatchSize and !needSendUsers.empty()) {
                // 如果存在能够在当前时刻到达的用户请求，就进行发送
                int userId = needSendUsers.top()[1];
                needSendUsers.pop();

                int batchSize = std::min(users[userId].remainCount, maxBatchSize);
                int handleTime = calculateInferenceTime(batchSize);
                int sendTime = arriveTime - latency[serverId][userId];

                // 如果当前用户的预测结束时间已经超时，那么该用户就不再发送任务
                int cnt = users[userId].remainCount / maxBatchSize;
                int resBatchSize = users[userId].remainCount - cnt * maxBatchSize;
                int maxBatchHandleTime = calculateInferenceTime(maxBatchSize);
                int resBatchHandleTime = calculateInferenceTime(resBatchSize);
                int predictEndTime = std::max(latency[serverId][userId], maxBatchHandleTime) * cnt 
                    + resBatchHandleTime + sendTime;

                if (predictEndTime <= users[userId].endTime) {
                    // std::clog << userId << std::endl;
                    ans[userId].push_back({sendTime, serverId, id, batchSize});
                    users[userId].remainCount -= batchSize;
                    npuResBatchSize -= batchSize;
                    sendBatchSize = batchSize;
                    sendHandleTime = handleTime;
                    finishTime = arriveTime + handleTime;

                    if (users[userId].remainCount > 0) {
                        newUsers.push({arriveTime + latency[serverId][userId] + 1, userId});
                    } else {
                        completedUsers.push_back(userId);
                        completedCount += 1;
                    }
                }

            }

            // 考虑batchsize小而多的情况，当有一个发送请求后，要把memory尽可能占满
            // while (npuResBatchSize > 0 and !needSendUsers.empty()) {
            //     int userId = needSendUsers.top()[1];
            //     needSendUsers.pop();
            //     // 推理时间计算公式为sqrt(bs) / k, 
                
            //     int batchSize = (sendHandleTime * inferenceSpeed) * (sendHandleTime * inferenceSpeed);
            //     batchSize = std::min(batchSize, npuResBatchSize);
            //     batchSize = std::min(batchSize, users[userId].remainCount);
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
            //             completedUsers.push_back(userId);
            //             completedCount += 1;
            //         }
            //     }
            // }


        }
        std::sort(completedUsers.begin(), completedUsers.end());
        for (auto& v: completedUsers) {
            std::clog << v << " ";
        }
        std::clog << std::endl << completedUsers.size() << std::endl;
        std::clog << "completed count: " << completedCount << std::endl;

        return completedCount;
    }
    
};
std::vector<std::vector<NPU> > npus(11); // 最多11个服务器

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

    std::cin >> A >> B;
    std::clog << A << " " << B << std::endl;
    // std::clog << "latency 4 131: " << latency[4][131] << std::endl;
    
    // 为每个用户分配一个推理时间最少的npu
    std::priority_queue<arr3> offLoad;

    std::vector<std::vector<std::vector<int> > > assignment(N + 1);
    for (int i = 1; i <= N; i ++) {
        assignment[i].resize(npus[i].size());
        for (int j = 1; j < (int)npus[i].size(); j ++) {
            npus[i][j].init();
            offLoad.push({0, i, j});
            // npuOffload.push({0, -npus[i][j].inferenceAbility, i, j});
        }
    }

    std::vector<std::array<int, 2> > userNpu(M + 1, {0, 0});
    ans.resize(M + 1);


    int currentServerId = 1, currentNpuId = 1;
    for (int i = 1; i <= M; i ++) {
        assignment[currentServerId][currentNpuId].push_back(i);
        userNpu[i] = {currentServerId, currentNpuId};

        currentNpuId += 1;
        if (currentNpuId >= (int)npus[currentServerId].size()) {
            currentServerId += 1;
            currentNpuId = 1;
            if (currentServerId > N) {
                currentServerId = 1;
            }
        }
    }

    // for (int i = 1; i <= M; i ++) {
    //     arr3 tp = offLoad.top();
    //     offLoad.pop();
    //     int lastOffLoad = tp[0], serverId = tp[1], npuId = tp[2];
    //     NPU& npu = npus[serverId][npuId];
    //     int cnt = users[i].remainCount / npu.maxBatchSize;
    //     int resBatchSize = users[i].remainCount - cnt * npu.maxBatchSize;
    //     int maxBatchHandleTime = npu.calculateInferenceTime(npu.maxBatchSize);
    //     int resBatchHandleTime = npu.calculateInferenceTime(resBatchSize);
    //     int currentOffLoad = lastOffLoad - maxBatchHandleTime - resBatchHandleTime;
    //     offLoad.push({currentOffLoad, serverId, npuId});
    //     assignment[serverId][npuId].push_back(i);
    //     userNpu[i] = {serverId, npuId};
    // }


    for (int i = 1; i <= N; i ++) {
        assignment[i].resize(npus[i].size());
        for (int j = 1; j < (int)npus[i].size(); j ++) {
            if(assignment[i][j].size() > 0) {
                npus[i][j].stimulate(assignment[i][j]);
            }
        }
    }


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
