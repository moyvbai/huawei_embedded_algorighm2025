#include <bits/stdc++.h>

#define READY 2
#define COMPLETED 1
#define MAX_SEND_COUNT 300
// #define ceil(x,y) ((x + y - 1) / y)


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

    // int stimulate(std::vector<int>& stimulateUsers) {
    //     // �ڸ�NPU��ģ��0-6e4ʱ���ڵ�ִ��������������������û�����
    //     int completedCount = 0;
    //     std::vector<User> usersCopy = users; // ������Ϣ����,��ֹ��Ϣ����
    //     std::priority_queue<arr2, std::vector<arr2>, std::greater<arr2> > newUsers; 
    //     // ����(���󵽴�ʱ��, userId)�ĸ�ʽ����
    //     std::priority_queue<arr2> needSendUsers; 
    //     // ����(prior, userId)�ĸ�ʽ�洢
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
    //             // ��������ܹ��ڵ�ǰʱ�̵�����û����󣬾ͽ��з���
    //             int userId = needSendUsers.top()[1];
    //             needSendUsers.pop();

    //             int batchSize = std::min(users[userId].remainCount, maxBatchSize);
                
    //             // if (N == 1 and G == 1) batchSize = std::min(batchSize, 25);
    //             int handleTime = calculateInferenceTime(batchSize);
    //             int sendTime = arriveTime - latency[serverId][userId];

    //             // �����ǰ�û���Ԥ�����ʱ���Ѿ���ʱ����ô���û��Ͳ��ٷ�������
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

            // ����batchsizeС��������������һ�����������Ҫ��memory������ռ��
            // while (npuResBatchSize > 0 and !needSendUsers.empty()) {
            //     int userId = needSendUsers.top()[1];
            //     needSendUsers.pop();
            //     // ����ʱ����㹫ʽΪsqrt(bs) / k, 
                
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
        
        std::vector<int> leastBigBlockCount(M + 1, 0);

        // ��ǰ���ֺÿ�
        for (int userId: stimulateUsers) {
            userRemainCount[userId] = users[userId].remainCount;
            for (int i = 0; i <= userRemainCount[userId] / blockSize; i ++) {
                int j = (userRemainCount[userId] - i * maxBatchSize) / blockSize;
                int res = userRemainCount[userId] - i * maxBatchSize - j * blockSize;
                if (res > 0) j += 1;
                if (i + j > 300) continue;
                else {
                    leastBigBlockCount[userId] = i;
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
                availableUsers.push({users[userId].prior, userId});
            }

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

                
                bool f1 = leastBigBlockCount[userId] <= 0;
                bool f2 = (arriveTime + processTime) <= users[userId].endTime;
                bool f3 = (arriveTime + calculateInferenceTime(std::min(blockSize, userRemainCount[userId]))) <= users[userId].endTime;
                bool f4 = userRemainCount[userId] <= blockSize;
                bool f5 = availableUsers.size() >= 1;
                if (f4 or (f1 and f2 and f3 and f5)) {
                    // ��ʱ����һ��С��
                    batchSize = std::min(blockSize, userRemainCount[userId]);
                    int handleTime = calculateInferenceTime(batchSize);
                    endTime = arriveTime + blockTime;
                } else {
                    leastBigBlockCount[userId] --;
                }

                int sendTime = arriveTime - latency[serverId][userId];
                ans[userId].push_back({sendTime, serverId, npuId, batchSize});
                userRemainCount[userId] -= batchSize;
                

                if (userRemainCount[userId] > 0) {
                    waitingUsers.push({arriveTime + latency[serverId][userId] + 1, userId});
                } else {
                    
                    if (arriveTime + handleTime <= users[userId].endTime) {
                        // if (userId == 198) {
                        //     std::clog << "198:  over " << std::endl;
                        // }
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
            // if (it->first == 198) {
            //     std::clog << "198 timeout!" << std::endl;
            // }
            timeoutUsers.push_back(it->first);
        }

        // std::sort(timeoutUsers.begin(), timeoutUsers.end());
    
        // for (int &v: timeoutUsers) {
        //     std::clog << v << " ";
        // }
        // std::clog << std::endl;

        // system("pause");



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
            // simulators[i][j].stimulateUsers = simulators[i][j].completedUsers;
            // simulators[i][j].stimulate();
        }
    }
    std::clog << simulators[1][1].completedUsers.size() << std::endl;
    std::clog << simulators[1][1].timeoutUsers.size() << std::endl;
    std::sort(timeoutUsers.begin(), timeoutUsers.end());
    
    std::vector<int> &ti = simulators[1][1].completedUsers;
    std::sort(ti.begin(), ti.end());
    
    for (int &v: ti) {
        std::clog << v << " ";
    }
    std::clog << std::endl;
    std::clog << "timeout user: " << std::endl;


    for (int &v: timeoutUsers) {
        std::clog << v << " ";
    }
    std::clog << std::endl;

    std::sort(timeoutUsers.begin(), timeoutUsers.end(), [&](int id1, int id2) {
        return users[id1].startTime < users[id2].startTime;
    });

    std::clog << "before iter, timeout count: " << timeoutUsers.size() << std::endl;
    std::vector<int> newTimeoutUsers;
    // for (int userId: timeoutUsers) {
    //     // std::clog << "try " << userId << std::endl;
    //     bool assignSuccess = false;
    //     for (int i = 1; i <= N; i ++) {
    //         if (assignSuccess) continue;
    //         for (int j = 1; j <(int)npus[i].size(); j ++) {
    //             if (assignSuccess) continue;
    //             std::vector<int> originCompletedUsers = simulators[i][j].completedUsers;
    //             simulators[i][j].stimulateUsers = simulators[i][j].completedUsers;
    //             simulators[i][j].stimulateUsers.push_back(userId);
    //             simulators[i][j].stimulate();
    //             if ((int)simulators[i][j].timeoutUsers.size() == 0) {
    //                 // std::clog << userId << " iter success!" << std::endl;
    //                 // std::clog << "serverId, npuId: " << i << " " << j << std::endl;
    //                 // std::clog << "completed count: " << simulators[i][j].completedUsers.size() << std::endl;
    //                 assignSuccess = true;
    //             } else {
    //                 // std::clog << "recover! " << originCompletedUsers.size() << std::endl;
    //                 simulators[i][j].stimulateUsers = originCompletedUsers;
    //                 simulators[i][j].stimulate();
    //                 // std::clog << "recover over!" << std::endl;
    //             }
    //         }
    //     }
    //     if (!assignSuccess) newTimeoutUsers.push_back(userId);
    // }
    // std::clog << "over!" << std::endl;
    // std::clog << simulators[1][1].completedUsers.size() << std::endl;
    // std::clog << simulators[1][1].timeoutUsers.size() << std::endl;
    // timeoutUsers = newTimeoutUsers;

    // std::clog << "after iter, timeout count: " << timeoutUsers.size() << std::endl;

    for (int i = 1; i <= N; i ++) {
        for (int j = 1; j <(int)npus[i].size(); j ++) {
            // simulators[i][j].stimulate();
            // std::vector<int> &ti = simulators[i][j].timeoutUsers;
            // timeoutUsers.insert(timeoutUsers.end(), ti.begin(), ti.end());
            // simulators[i][j].stimulateUsers = simulators[i][j].completedUsers;
            // simulators[i][j].stimulate();
            
            for (auto&k : simulators[i][j].completedUsers) {
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
        int cnt = std::min((int)ans[i].size(), 3000);
        
        // if (cnt > 300) {
        //     std::vector<NPU> a(1e9);
        // }

        std::cout << cnt << std::endl;
        // assert((int)ans[i].size() <= 300);
        

        for (int j = 0; j < cnt; j ++) {
            // if (ans[i][j][0] > 1e6) {
            //     std::vector<NPU> a(1e9);
            // }
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
