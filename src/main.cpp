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
    
};
std::vector<std::vector<NPU> > npus(11); // ���11��������


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
        this->prior = 1e5 * requestCount / (this->endTime - this->startTime);
    }
};
std::vector<User> users;
std::vector<std::vector<int> > latency; // �û��������͵��������ϵ�ʱ��


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
    // �������(time, serverId, npuId, batchSize)
    std::vector<arr2> userNpu(M + 1); // �û�ָ����npu;

    std::priority_queue<arr2, std::vector<arr2>, std::greater<arr2> > newUsers; 
    // ����(time, userId)�ĸ�ʽ����
    std::priority_queue<arr4, std::vector<arr4>, std::greater<arr4> > npuOffload; 
    // �Զ����� (offload, inferenceAbility, serverId, npuId)�ı�׼��������

    std::priority_queue<arr2> needSendUsers;
    // ����(prior, userId)�ĸ�ʽ�洢


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
                // �����ʱ������Ϊ��ǰ�û�Ѱ��һ���µ�NPU
                // ����ҵõ����ͷ��䵽�µ�npu�ϣ�����Ҳ����������û�Ҳ�Ҳ�����Ӧnpu,ֱ��break
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

                        // �����ǰ�û�������δ��ɣ�����֮����ά������û�����Ϣ
                        if (users[userId].remainCount > 0) {
                            // std::cout << 
                            newUsers.push({sendTime + latency[serverId][userId] + 1, userId});
                        }

                    } 
                    
                }
            } else {
                // ��ǰnpu�ĸ��ؽϵͣ����Է�������
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

                    // �����ǰ�û�������δ��ɣ�����֮����ά������û�����Ϣ
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
