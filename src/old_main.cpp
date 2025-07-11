#include <bits/stdc++.h>



#define READY 2
#define COMPLETED 1
#define MAX_SEND_COUNT 300
// #define ceil(x,y) ((x + y - 1) / y)



using namespace std::chrono;

using arr2 = std::array<int, 2>;
using arr3 = std::array<int, 3>;
using arr4 = std::array<int, 4>;


// ������1��ʼ
int N;  // ����������
int M; // �û�����
int A, B, G; // �Դ���batchsize֮��Ĺ�ϵ��Mem = a * bs + b


class User {
public:
    int id; // �û�id
    int startTime; // ����ʼʱ��
    int endTime; // �������ʱ��
    int requestCount; // ��������
    int remainCount; // ʣ���δ��ɵ���������

    int prior; // �û������ȼ�
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
std::vector<std::vector<int> > latency; // �û��������͵��������ϵ�ʱ��

class NPU {
public:
    int serverId; // �����ķ�����id
    int id; // ���
    int totalMemory; // �Դ���������λ��GB��
    int maxBatchSize; // ����batch size
    int freeBatchSize;  // ʣ���Դ棨��λ��GB��
    int inferenceSpeed; // �����ٶȣ���λ������/�룩
    int inferenceAbility; // ���������� sqrt(bc) * k;
    int finishTime; // �������ʱ�䣬Ĭ��Ϊ0


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
std::vector<std::vector<NPU> > npus(11); // ���11��������


class Simulator {
public:
    int serverId; // ������Id
    int npuId; // npu��Id
    int maxTime; // ģ������ʱ��
    int totalMemory; // �Դ���������λ��GB
    int maxBatchSize;
    int inferenceSpeed; // �����ٶȣ���λ������/�룩
    int finishTime;     // ���Ľ���ʱ��
    std::vector<int> stimulateUsers;
    std::vector<int> sumMemory; // �Դ�ռ�õ�ǰ׺��
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

    void stimulate() {
        init();

        int blockSize = (totalMemory / 2 - B) / A;
        int blockTime = calculateInferenceTime(blockSize);
        
        std::vector<int> maxSmallBlockCount(M + 1, 0);

        // ��ǰ���ֺÿ�
        for (int userId: stimulateUsers) {
            userRemainCount[userId] = users[userId].remainCount;
            int mx = ceil(1.0 * userRemainCount[userId] / blockSize);
            for (int i = mx; i >= 0; i --) {
                int j = (userRemainCount[userId] - i * blockSize) / maxBatchSize;
                int res = userRemainCount[userId] - j * maxBatchSize - i * blockSize;
                if (res > 0) j += 1;
                if (i + j > 300) continue;
                else {
                    maxSmallBlockCount[userId] = i;
                    break;
                }
            }
        }

        std::priority_queue<arr2, std::vector<arr2>, std::greater<arr2> > waitingUsers; 
        // ����(���󵽴�ʱ��, userId)�ĸ�ʽ����
        std::priority_queue<arr2, std::vector<arr2>, std::greater<arr2> > availableUsers; 
        // ����(prior, userId)�ĸ�ʽ�洢

        for (int userId: stimulateUsers) {
            waitingUsers.push({users[userId].startTime + latency[serverId][userId], userId});
        }

        for (int arriveTime = 0; arriveTime <= maxTime; arriveTime ++) {
            int freeMemory = totalMemory - memeoryUsage[arriveTime];
            int freeBatchSize = calculateBatchSize(freeMemory);
            if (freeBatchSize <= 1) continue;

            while (!waitingUsers.empty() and waitingUsers.top()[0] <= arriveTime) {
                arr2 tp = waitingUsers.top();
                waitingUsers.pop();
                int userId = tp[1];
                availableUsers.push({users[userId].requestCount, userId});
            }

            
            int skipCount = 0;
            while(!availableUsers.empty()) {
                int freeMemory = totalMemory - memeoryUsage[arriveTime];
                int freeBatchSize = calculateBatchSize(freeMemory);
                if (freeBatchSize <= 1) break;

                int userId = availableUsers.top()[1];
                availableUsers.pop();
                int batchSize = std::min(maxBatchSize, userRemainCount[userId]);
                int handleTime = calculateInferenceTime(batchSize);
                int endTime = arriveTime + handleTime;
                int cnt = userRemainCount[userId] / blockSize;
                int res = userRemainCount[userId] - cnt * blockSize;
                int resTime = calculateInferenceTime(res);
                int processTime = std::max(blockTime, latency[serverId][userId]) * cnt + resTime;

                
                bool f1 = maxSmallBlockCount[userId] > 0;
                bool f2 = (arriveTime + 2 * processTime) <= users[userId].endTime;
                bool f3 = (arriveTime + calculateInferenceTime(std::min(blockSize, userRemainCount[userId]))) <= users[userId].endTime;
                bool f4 = availableUsers.size() >= 2;
                bool f5 = userRemainCount[userId] <= blockSize;
                bool f6 = (freeBatchSize < maxBatchSize and freeBatchSize >= blockSize); // ��ǰʣ��һ��С��
                if ((f1 and f2 and f3)) {
                    // ��������Ϊ��С�����������Ҳ���ʱ
                    if (f6 or (!f6 and f4) or f5) {
                        // ��ʱ����һ��С��
                        batchSize = std::min(blockSize, userRemainCount[userId]);
                        int handleTime = calculateInferenceTime(batchSize);
                        endTime = arriveTime + blockTime;
                        maxSmallBlockCount[userId] -= 1;
                    }
                } 

                // ��ʱ����һ��С�飬���ǲ����㷢��С�������
                if ((freeBatchSize <= blockSize) and (batchSize > blockSize)) {
                    waitingUsers.push({arriveTime, userId});
                    continue;
                }

                // if (arriveTime + processTime + 1000 <= users[userId].endTime and skipCount <= 2 and f4) {
                //     waitingUsers.push({arriveTime, userId});
                //     skipCount += 1;
                //     continue;
                // }

                int sendTime = arriveTime - latency[serverId][userId];
                ans[userId].push_back({sendTime, serverId, npuId, batchSize});
                userRemainCount[userId] -= batchSize;
                

                if (userRemainCount[userId] > 0) {
                    waitingUsers.push({arriveTime + latency[serverId][userId] + 1, userId});
                } else {
                    
                    if (arriveTime + handleTime <= users[userId].endTime) {
                        completedUsers.push_back(userId);
                        userRemainCount.erase(userId);
                    }
                }
                
                if (batchSize <= blockSize) batchSize = blockSize;
                else batchSize = maxBatchSize;

                for (int t = arriveTime; t < endTime; t ++) {
                    memeoryUsage[t] += (batchSize * A + B);
                }
            }

        }


        for(auto it = userRemainCount.begin(); it != userRemainCount.end(); it ++) {
            timeoutUsers.push_back(it->first);
        }


        // �����Դ���ۻ���
        for(int i = 1; i <= maxTime; i ++) {
            sumMemory[i] = sumMemory[i - 1] + memeoryUsage[i];
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

    simulators[1][1].stimulateUsers = simulators[1][1].completedUsers;
    simulators[1][1].stimulate();
    std::clog << "again: " << simulators[1][1].timeoutUsers.size() << std::endl;
    std::clog << "before iter, timeout count: " << timeoutUsers.size() << std::endl;
    for (int userId: timeoutUsers) {
        // std::clog << "try " << userId << std::endl;
        bool assignSuccess = false;
        for (int i = 1; i <= N; i ++) {
            if (assignSuccess) continue;
            for (int j = 1; j <(int)npus[i].size(); j ++) {
                std::vector<int> originCompletedUsers = simulators[i][j].completedUsers;
                simulators[i][j].stimulateUsers = simulators[i][j].completedUsers;
                simulators[i][j].stimulateUsers.push_back(userId);
                // std::clog << "again: " << simulators[1][1].stimulateUsers.size() << std::endl;
                simulators[i][j].stimulate();
                // std::clog << "again: " << simulators[1][1].completedUsers.size() << std::endl;
                // std::clog << "again: " << simulators[1][1].timeoutUsers.size() << std::endl;
                if ((int)simulators[i][j].timeoutUsers.size() == 0) {
                    // std::clog << userId << " iter success!" << std::endl;
                    // std::clog << "serverId, npuId: " << i << " " << j << std::endl;
                    // std::clog << "completed count: " << simulators[i][j].completedUsers.size() << std::endl;
                    assignSuccess = true;
                } else {
                    // std::clog << "recover! " << originCompletedUsers.size() << std::endl;
                    simulators[i][j].stimulateUsers = originCompletedUsers;
                    // simulators[i][j].stimulate();
                    // std::clog << "recover over!" << std::endl;
                }
            }
        }
    }
    
    timeoutUsers.clear();

    for (int i = 1; i <= N; i ++) {
        for (int j = 1; j <(int)npus[i].size(); j ++) {
            std::vector<int> &ti = simulators[i][j].timeoutUsers;
            timeoutUsers.insert(timeoutUsers.end(), ti.begin(), ti.end());

            for (auto&k : simulators[i][j].completedUsers) {
                users[k].remainCount = simulators[i][j].userRemainCount[k];
                ans[k].insert(ans[k].end(), simulators[i][j].ans[k].begin(), simulators[i][j].ans[k].end());
            }

            for (auto&k : simulators[i][j].timeoutUsers) {
                users[k].remainCount = simulators[i][j].userRemainCount[k];
                ans[k].insert(ans[k].end(), simulators[i][j].ans[k].begin(), simulators[i][j].ans[k].end());
            }
        }
    }
    

    std::clog << "timeout count: " << timeoutUsers.size() << std::endl;
    

    for (int i = 1; i <= M; i ++) {
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
    auto start_time = high_resolution_clock::now();
    solve();
    auto end_time = high_resolution_clock::now();
    auto duration = duration_cast<milliseconds>(end_time - start_time);
    std::cerr << "execution time: " << duration.count() << "ms." << std::endl;
    return 0;
}
