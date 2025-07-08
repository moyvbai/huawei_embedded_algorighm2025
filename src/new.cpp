#include <bits/stdc++.h>

#define READY 2
#define COMPLETED 1
#define MAX_SEND_COUNT 300
// #define ceil(x,y) ((x + y - 1) / y)


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
    int finishTime;     // 最后的结束时间
    std::vector<int> stimulateUsers;
    std::vector<int> sumMemory; // 显存占用的前缀和
    std::vector<int> completedUsers;
    std::vector<int> timeoutUsers;
    std::vector<int> memeoryUsage;
    std::vector<int> remainSendCount;
    std::unordered_map<int, int> userRemainCount;
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
        finishTime = 0;
        userRemainCount.clear();
        sumMemory = std::vector<int>(maxTime + 100, 0);
        completedUsers.clear();
        timeoutUsers.clear();
        memeoryUsage = std::vector<int>(maxTime + 100, 0);
        ans = std::vector<std::vector<std::array<int, 4> > >(M + 1);
    }

    int calculateInferenceTime (int batchSize) {
        return ceil(sqrtl(batchSize) / inferenceSpeed);
    }

    int calculateBatchSize(int memory) {
        return std::max((memory - B) / A, 0);
    }


    std::unordered_map<int, arr3> division() {
        int blockSize = (totalMemory / 2 - B) / A;
        int blockTime = calculateInferenceTime(blockSize);
        int bigBlockTime = calculateInferenceTime(maxBatchSize);

        std::unordered_map<int, arr3> result;
        for (int userId: stimulateUsers) {
            // 枚举大块的数量
            for (int i = 0; i <= ceil(1.0 * userRemainCount[userId] / maxBatchSize); i ++) {
                int j = (users[userId].requestCount - i * maxBatchSize) / blockSize;
                int res = users[userId].remainCount - i *maxBatchSize - j * blockSize;
                if (i + j > 280) continue;
                int porcessTime = std::max(latency[serverId][userId], bigBlockTime) * i;
                int ceilTime = ceil(1.0 * latency[serverId][userId] / blockTime) * blockTime;
                porcessTime += ceilTime * j;
                if (res > 0) porcessTime += (ceilTime + calculateInferenceTime(res));
                if (users[userId].startTime + porcessTime > users[userId].endTime) continue; 

                result[userId] = {i, j, res};
                break;
            }
        }

        return result;
    }

    void stimulate() {
        init();

        int blockSize = (totalMemory / 2 - B) / A;
        int blockTime = calculateInferenceTime(blockSize);
        
        // 提前划分好块
        std::unordered_map<int, arr3> userDivision = division();
        // 按照(请求到达时间, userId)的格式排序，
        std::set<arr2> waitingUsers;
        // 按照(prior, userId)的格式存储
        std::set<arr2> availableUsers;
        
        for (int userId: stimulateUsers) {
            waitingUsers.insert({users[userId].startTime + latency[serverId][userId], userId});
        }

        for (int arriveTime = 0; arriveTime <= maxTime; arriveTime ++) {
            std::clog << "arrive time: " << arriveTime << std::endl;
            int freeMemory = totalMemory - memeoryUsage[arriveTime];
            int freeBatchSize = calculateBatchSize(freeMemory);
            if (freeBatchSize <= 1) continue;

            while (!waitingUsers.empty() and (*waitingUsers.begin())[0] <= arriveTime) {
                arr2 tp = *waitingUsers.begin();
                waitingUsers.erase(waitingUsers.begin());
                int userId = tp[1];
                availableUsers.insert({users[userId].endTime, userId});
            }

            while(!availableUsers.empty()) {
                int freeMemory = totalMemory - memeoryUsage[arriveTime];
                int freeBatchSize = calculateBatchSize(freeMemory);
                if (freeBatchSize <= 1) break;

                int userId = (*availableUsers.begin())[1];
                int batchSize = 0;
                
                if (finishTime == arriveTime) { // 当前时刻发送的第一个块
                    availableUsers.erase(availableUsers.begin());
                    if (userDivision[userId][0] > 0) batchSize = maxBatchSize;
                    if ((userDivision[userId][1] > 0) and (userDivision[userId][0] <= 0 or (int)availableUsers.size() >= 4)) {
                        batchSize = blockSize;
                        userDivision[userId][1] -= 1;
                        finishTime = arriveTime + calculateInferenceTime(batchSize);
                    }

                    if (userDivision[userId][0] <= 0 and userDivision[userId][1] <= 0) {
                        if (userDivision[userId][2] > 0) {
                            batchSize = userDivision[userId][2];
                            userDivision[userId][2] = 0;
                            finishTime = arriveTime + blockTime;
                        }
                    }

                    if (batchSize == maxBatchSize) {
                        userDivision[userId][0] -= 1;
                        finishTime = arriveTime + calculateInferenceTime(maxBatchSize);
                    }
                
                } else if (freeBatchSize >= blockSize) { // 不是当前时刻第一个发送的块
                    if (userDivision[userId][1] > 0) {
                        availableUsers.erase(availableUsers.begin());
                        batchSize = blockSize;
                        userDivision[userId][1] -= 1;
                    } else {
                        availableUsers.erase(availableUsers.begin());
                        waitingUsers.insert({arriveTime, userId});
                    }

                } else { // 当前有小块需要补
                    if ((int)availableUsers.size() >= 3) { // 将两个用户剩余块合在一起
                        for (auto it = availableUsers.rbegin(); it != availableUsers.rend(); it ++) {
                            userId = (*it)[1];
                            int resBatchSize = userDivision[userId][2];
                            if (resBatchSize <= 0) continue;
                            int handleTime = calculateInferenceTime(resBatchSize);
                            // int mem = resBatchSize * A + B;
                            if (arriveTime + handleTime <= finishTime and resBatchSize <= freeBatchSize) {
                                batchSize = resBatchSize;
                                userDivision[userId][2] = 0;
                                availableUsers.erase(*it);
                                break;
                            } else {
                                availableUsers.erase(*it);
                                waitingUsers.insert({arriveTime, userId});
                            }
                        }
                    }

                }

                if (batchSize > 0) {
                    ans[userId].push_back({arriveTime - latency[serverId][userId], serverId, npuId, batchSize});
                    if (userDivision[userId][0] <= 0 and userDivision[userId][1] <= 0 and userDivision[userId][2] <= 0) {
                        completedUsers.push_back(userId);
                    } else {
                        waitingUsers.insert({arriveTime + latency[serverId][userId] + 1, userId});
                    }

                    int handleTime = calculateInferenceTime(batchSize);
                    for (int i = arriveTime; i < arriveTime + handleTime; i ++) {
                        memeoryUsage[i] += A * batchSize + B;
                    }

                }

            }

        }


        for(auto it = userRemainCount.begin(); it != userRemainCount.end(); it ++) {
            timeoutUsers.push_back(it->first);
        }

        
    }

    int queryFreeMemory(int l, int r) {
        return maxBatchSize * (r - l) - (sumMemory[r - l] - sumMemory[l - 1]);
    }
};


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
        return users[id1].startTime < users[id2].startTime;
    });
    
    int currentServerId = 1, currentNpuId = 1;
    for (int& userId: userOrder) {
        simulators[currentServerId][currentNpuId].stimulateUsers.push_back(userId);
        currentNpuId += 1;
        if (currentNpuId >= (int)npus[currentServerId].size()) {
            currentServerId += 1;
            currentNpuId = 1;
            if (currentServerId > N) {
                currentServerId = 1;
            }
        }
    }


    for (int i = 1; i <= N; i ++) {
        for (int j = 1; j <(int)npus[i].size(); j ++) {
            simulators[i][j].stimulate();
            std::vector<int> &ti = simulators[i][j].timeoutUsers;
            timeoutUsers.insert(timeoutUsers.end(), ti.begin(), ti.end());
        }
    }


    for (int i = 1; i <= N; i ++) {
        for (int j = 1; j <(int)npus[i].size(); j ++) {
            for (auto&k : simulators[i][j].completedUsers) {
                ans[k].insert(ans[k].end(), simulators[i][j].ans[k].begin(), simulators[i][j].ans[k].end());
            }

            for (auto&k : simulators[i][j].timeoutUsers) {
                users[k].remainCount = simulators[i][j].userRemainCount[k];
                ans[k].insert(ans[k].end(), simulators[i][j].ans[k].begin(), simulators[i][j].ans[k].end());
            }
        }
    }
   
    std::clog << "timeout count: " << timeoutUsers.size() << std::endl;
    
    for (auto &i: timeoutUsers) {
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
        int cnt = std::min((int)ans[i].size(), 3000);
        std::cout << cnt << std::endl;
        

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
