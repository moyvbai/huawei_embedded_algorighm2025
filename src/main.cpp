#include <bits/stdc++.h>

#define READY 2
#define COMPLETED 1


using arr2 = std::array<int, 2>;
using arr3 = std::array<int, 3>;
using arr4 = std::array<int, 4>;


// ������1��ʼ
int N;  // ����������
int M; // �û�����
int A, B; // �Դ���batchsize֮��Ĺ�ϵ��Mem = a * bs + b


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
        // this->prior = 1e5 * requestCount / (this->endTime - this->startTime);
        this->prior = - this->endTime;
    }
};
std::vector<User> users;
std::vector<std::vector<int> > latency; // �û��������͵��������ϵ�ʱ��
std::vector<std::vector<std::array<int, 4> > > ans;

class NPU {
public:
    int severId; // �����ķ�����id
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

    int stimulate(std::vector<int>& stimulateUsers) {
        // �ڸ�NPU��ģ��0-6e4ʱ���ڵ�ִ��������������������û�����
        int completedCount = 0;
        std::vector<User> usersCopy = users; // ������Ϣ����,��ֹ��Ϣ����
        std::priority_queue<arr2, std::vector<arr2>, std::greater<arr2> > newUsers; 
        // ����(time, userId)�ĸ�ʽ����
        std::priority_queue<arr2> needSendUsers;
        // ����(prior, userId)�ĸ�ʽ�洢
        int finishTime = 0, maxTime = 6e4;  
        // std::vector<std::vector<std::array<int, 4> > > ans(M + 1);

        for (int userId: stimulateUsers) {
            newUsers.push({users[userId].startTime, userId});
        }
        
        // std::cout << "start stimulate" << std::endl;

        for (int curretTime = 0; curretTime <= maxTime; curretTime ++) {
            while (!newUsers.empty() and newUsers.top()[0] <= curretTime) {
                // std::cout << 1 << std::endl;
                arr2 tp = newUsers.top();
                newUsers.pop();
                int userId = tp[1];
                needSendUsers.push({users[userId].prior, userId});
            }

            if (!needSendUsers.empty() and finishTime - curretTime <= 20) {
                // �����ǰ����ʱ����磬��ô�͸�npu�·���һ������
                int userId = needSendUsers.top()[1];
                needSendUsers.pop();

                // �����ǰ�û���Ԥ�����ʱ���Ѿ���ʱ����ô���û��Ͳ��ٷ�������

                int batchSize = std::min(users[userId].remainCount, maxBatchSize);
                int handleTime = calculateInferenceTime(batchSize);
                int sendTime = std::max(curretTime, finishTime - latency[severId][userId]);

                int cnt = users[userId].remainCount / maxBatchSize;
                int resBatchSize = users[userId].remainCount - cnt * maxBatchSize;
                int maxBatchHandleTime = calculateInferenceTime(maxBatchSize);
                int resBatchHandleTime = calculateInferenceTime(resBatchSize);
                int predictEndTime = std::max(latency[severId][userId], maxBatchHandleTime) * cnt 
                    + resBatchHandleTime + finishTime;

                int arriveTime = std::max(finishTime, sendTime + latency[severId][userId]);
                
                if (predictEndTime <= users[userId].endTime) {
                    finishTime = std::max(finishTime, sendTime + latency[severId][userId]) + handleTime;
                    ans[userId].push_back({sendTime, severId, id, batchSize});
                    users[userId].remainCount -= batchSize;
                    if (users[userId].remainCount > 0) {
                        // if (userId == 131) {
                            // std::clog << "sendTime: " << sendTime 
                            // << " next time: " << sendTime + latency[id][userId] + 1
                            // << std::endl;
                        // }
                        newUsers.push({sendTime + latency[severId][userId] + 1, userId});
                    } else {
                        if (finishTime <= users[userId].endTime) {
                            completedCount += 1;
                        }
                    }
                }
                

            }
        }
        std::clog << "completed count: " << completedCount << std::endl;

        return completedCount;
    }
    
};
std::vector<std::vector<NPU> > npus(11); // ���11��������

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
    
    // Ϊÿ���û�����һ������ʱ�����ٵ�npu
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


    // �����ʱ�����û�������û����ɣ���ԭ����npu�Ϸ���ʣ�������
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
