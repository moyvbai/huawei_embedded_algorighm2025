#include <bits/stdc++.h>

#define READY 2
#define COMPLETED 1


// ������1��ʼ
int N;  // ����������
int M; // �û�����
int A, B; // �Դ���batchsize֮��Ĺ�ϵ��Mem = a * bs + b

class Request {
public:
    int requestTime; // �����ʱ��
    int userId;         // �û���Id
    int batchSize;     // batchsize
    int serverId;       // ����ķ�����id
    int npuId;          // �����npu��id

    Request(){};
    Request(int requestTime, int userId, int batchSize) {
        this->requestTime = requestTime;
        this->userId = userId;
        this->batchSize = batchSize;
    }

    bool operator<(const Request& other) const { // ����С�ں�����
        if (this->requestTime == other.requestTime) {
            return this->userId < other.userId;
        } else {
            return this->requestTime < other.requestTime;
        }
    }; 

};


class Event {
public: 
    int time; // �¼���ʱ��
    int type; // ��ʾ��ǰ���¼�����
    double prior; // �¼����ȼ�
    Request request;

    bool operator<(const Event& other) const { // ����С�ں�����
        if (this->time == other.time) {
            if (this->type == other.type) {
                return this->prior < other.prior;
            } else {
                return this->type > other.type;
            }
            
        } else {
            return this->time > other.time;
        }
    }

};

std::priority_queue<Event> eventQueue;


class NPU {
public:
    int severId; // �����ķ�����id
    int id; // ���
    int totalMemory; // �Դ���������λ��GB��
    int freeMemory;  // ʣ���Դ棨��λ��GB��
    int inferenceSpeed; // �����ٶȣ���λ������/�룩
    std::vector<Request> requests; // �������


    int calculateInferenceTime (int batchSize) {
        return ceil(sqrtl(batchSize) / inferenceSpeed);
    }

    void handleRequest(int currentTime) {
        // 1. �Ƴ���������������
        // 2. ���ӵ�ǰʱ�̽��յ�������    
        // 3. ����������
        // 4. �Ӷ�������β����ɨ�����������ϸ����������Դ��δ�����������Դ棬����Ϊ������Ը�
        // �������������Դ��

        // std::cout << "��ǰNPU" << this->severId << " " << this->id << std::endl;
        // std::cout << "current requests size: " << requests.size() << std::endl;
        std::sort(requests.begin(), requests.end());
        std::vector<Request> new_requests;
        // std::cout << "current time: " << currentTime << std::endl;
        for (int i = 0; i < (int)requests.size(); ++i) {
            // std::cout << requests[i].requestTime << " " << requests[i].userId << " " << requests[i].batchSize << std::endl;
            if (requests[i].requestTime <= currentTime and requests[i].batchSize * A + B <= this->freeMemory) {
                Event reqcomp;
                // std::cout << requests[i].batchSize << std::endl;
                // std::cout << calculateInferenceTime(it->batchSize) << std::endl;
                reqcomp.time = currentTime + calculateInferenceTime(requests[i].batchSize);
                reqcomp.prior = 1;
                reqcomp.type = COMPLETED;
                reqcomp.request = requests[i];

                // std::cout << "request complete time: " << reqcomp.time << std::endl;
                eventQueue.push(reqcomp);
                this->freeMemory -= (requests[i].batchSize * A + B);
            } else {
                new_requests.push_back(requests[i]);
            }
        }
        // std::cout << "it over! " << std::endl;
        requests = std::move(new_requests);
        // std::cout << "move over!" << std::endl;
    }

    int predictHandleTime() {
        // ���ݵ�ǰNPU������״̬��Ԥ��NPU���������ʱ��
        return calculateInferenceTime((this->totalMemory - B) / A) * ((int)requests.size() + 0);
    }

    // ���캯��
    NPU(){};
    NPU(int serverId, int id, int totalMemory, int inferenceSpeed) {
        this->severId = serverId;
        this->id = id;
        this->totalMemory = totalMemory;
        this->freeMemory = totalMemory;
        this->inferenceSpeed = inferenceSpeed;
    } 
    
};

class Server {
public:
    int id; // ������id
    int npuCount; // NPU����

    std::vector<NPU> npus; // NPUʵ��
    
    Server(){};
    Server(int id, int npuCount, int k, int m) {
        this->id = id;
        this->npuCount = npuCount;

        npus.resize(npuCount + 1);
        for (int i = 1; i <= npuCount; i ++) {
            npus[i] = NPU(id, i, m, k);
        }
    }
};
std::vector<Server> servers;


class User {
public:
    int id; // �û�id
    int startTime; // ����ʼʱ��
    int endTime; // �������ʱ��
    int requestCount; // ��������
    int remainCount; // ʣ���δ��ɵ���������
    User(){};
    User(int id, int startTime, int endTime, int requestCount) {
        this->id = id;
        this->startTime = startTime;
        this->endTime = endTime;
        this->requestCount = requestCount;
        this->remainCount = requestCount;
    }
};
std::vector<User> users;
std::vector<std::vector<int> > latency;

void read_data() {
    std::cin >> N;
    servers.resize(N + 1);
    for (int i = 1; i <= N; i ++) {
        int gi, ki, mi; 
        std::cin >> gi >> ki >> mi;
        servers[i] = Server(i, gi, ki, mi);
    }

    std::cin >> M;
    
    users.resize(M + 1);
    for (int i = 1; i <= M; i ++) {
        int si, ei, cnti; 
        std::cin >> si >> ei >> cnti;
        users[i] = User(i, si, ei, cnti);
        
        Event e;
        e.time = si;
        e.prior = 3;
        e.type = READY;
        e.request.userId = i;
        eventQueue.push(e);
    }


    latency.resize(N + 1, std::vector<int>(M + 1));
    for (int i = 1; i <= N; i ++) {
        for (int j = 1; j <= M; j ++) {
            std::cin >> latency[i][j];
        }
    }

    std::cin >> A >> B;
    std::clog << A << " " << B << std::endl;
}

void solve() {
    std::vector<std::vector<std::array<int, 4> > > ans(M + 1); // �������
    std::vector<std::array<int, 2> > userNpu(M + 1, {1, 1}); // �û�ָ����npu;
    std::set<std::array<int, 2> > needUpdateNPU;
    int cnt = 0;
    int cnt1 = 0, cnt2 = 0;
    while (!eventQueue.empty()) {
        Event e = eventQueue.top(); // ��������һ��Eventָ��
        eventQueue.pop();
        int currentTime = e.time;
        Request& request = e.request;
        
        // std::cout << "time: " << currentTime << std::endl;
        // std::cout << e.type << std::endl;
        cnt += 1;
        // if (cnt > 1000) break;

        if (e.type == COMPLETED) {
            // ��RequestCompletedEvent
            int serverId = request.serverId;
            int npuId = request.npuId;
            int batchSize = request.batchSize;
            
            if (batchSize) servers[serverId].npus[npuId].freeMemory += (A * batchSize + B);
            // std::cout << "batch size: " << batchSize << std::endl;
            // std::cout << "serverId " << serverId << " npuId: " << npuId
                // << " remain memory: " << servers[serverId].npus[npuId].freeMemory << std::endl;;
            needUpdateNPU.insert({serverId, npuId});
            // servers[serverId].npus[npuId].handleRequest(currentTime);

            // std::cout << "serverId " << serverId << " npuId: " << npuId
                // << " remain memory: " << servers[serverId].npus[npuId].freeMemory << std::endl;;
            

        } else if (e.type == READY) {
            // ��RequestReadyEvent
            // �û���ʱ���Է������󣬵��ǲ�һ�����뷢��
            // �����ǰ��npu�ϸ��ؽ�С���ͷ������󣬷���
            //      ������ҵ�һ�����رȽ�С��npu����ô�͸���npu������
            //        ��ǰʱ��Ƭ���������󣬵���һ��ʱ��Ƭ�ٽ����ж�
            
            int userId = request.userId;
            int serverId = userNpu[userId][0], npuId = userNpu[userId][1];
            bool needSend = true;
            if (serverId == 0) {
                std::vector<std::array<int, 3> > allNpus;
                for (int i = 1; i <= N; i ++) {
                    for (int j = 1; j <= servers[i].npuCount; j ++) {
                        allNpus.push_back({servers[i].npus[j].predictHandleTime(), i, j});
                    }
                }
                sort(allNpus.begin(), allNpus.end());
                serverId = allNpus[0][1], npuId = allNpus[0][2];
                userNpu[userId] = {serverId, npuId};
                needSend = true;
            } else {
                int lastNpuHanleTime = servers[serverId].npus[npuId].predictHandleTime();
                if (lastNpuHanleTime >= 0) { // ��һ��npu�ĸ��ؽϵͣ����ı�npu
                    needSend = true;
                } else { // ����Ѱ��һ�����ظ��͵�npu
                    std::vector<std::array<int, 3> > allNpus;
                    for (int i = 1; i <= N; i ++) {
                        for (int j = 1; j <= servers[i].npuCount; j ++) {
                            allNpus.push_back({servers[i].npus[j].predictHandleTime(), i, j});
                        }
                    }
                    sort(allNpus.begin(), allNpus.end());
                    if (allNpus[0][0] <= 30) { // �������һ�����ص͵�npu
                        serverId = allNpus[0][1], npuId = allNpus[0][2];
                        userNpu[userId] = {serverId, npuId};
                        needSend = true;
                        cnt1 += 1;
                    }
                }
            }

            // ȷ����Ҫ���͵�npu���֮�󣬼��Է���
            if (needSend) {
                // std::cout << "userId: " << userId << " is Ready" << std::endl;
                Request request;
                request.requestTime = currentTime + latency[serverId][userId];
                request.batchSize = (servers[serverId].npus[npuId].totalMemory - B) / A;
                request.batchSize = std::min(request.batchSize, users[userId].remainCount);
                request.serverId = serverId;
                request.npuId = npuId;
                request.userId = userId;
                users[userId].remainCount -= request.batchSize;
                if (request.batchSize) servers[serverId].npus[npuId].requests.push_back(request);

                Event newReady, newCompleted;
                newReady.time = currentTime + latency[serverId][userId] + 1;
                newReady.type = READY;
                newReady.request = request;
                newReady.prior = 1.0 * users[userId].remainCount / (users[userId].endTime - newReady.time);

                newCompleted.time = currentTime + latency[serverId][userId];
                newCompleted.type = COMPLETED;
                newCompleted.request = request;
                newCompleted.request.batchSize = 0;

                if (users[userId].remainCount) eventQueue.push(newReady);
                eventQueue.push(newCompleted);

                ans[userId].push_back({currentTime, serverId, npuId, request.batchSize});
                // std::cout << "user " << userId << " send to " << serverId << " " << npuId 
                //     << " batch size: "<< request.batchSize << "\n";
                // std::cout << "arrive time: " << request.requestTime << "\n";
                // std::cout << "remain count: " << users[userId].remainCount << "\n";

            } else { // ���¸�ʱ��Ƭ�ٿ���Ҫ��Ҫ����
                Event newReady;
                newReady.time = currentTime / 100 + 100;
                newReady.type = READY;
                newReady.request.userId = userId;
                newReady.prior = 1.0 * users[userId].remainCount / (users[userId].endTime - newReady.time);
                eventQueue.push(newReady);
                cnt2 += 1;

                // std::cout << "next time to consider" << "\n";
            }
           
        } 

        if (eventQueue.empty() or currentTime != eventQueue.top().time) {
            for (auto it = needUpdateNPU.begin(); it != needUpdateNPU.end(); it ++) {
                int serverId = it->at(0), npuId = it->at(1);
                servers[serverId].npus[npuId].handleRequest(currentTime);
            }
            needUpdateNPU.clear();
        }

    }
    std::clog << "total count: " << cnt1 << " " << cnt2 << std::endl;
    for (int i = 1; i <= M; i ++) {
        std::cout << ans[i].size() << std::endl;
        for (int j = 0; j < (int)ans[i].size(); j ++) {
            std::cout << ans[i][j][0] << " " << ans[i][j][1] << " "
                << ans[i][j][2] << " " << ans[i][j][3] << " ";
        }
        std::cout << std::endl;
    }
}


 
void bad_strategy() {
    std::vector<std::vector<std::array<int, 4> > > ans(M + 1); // �������
    int start_time = 6e4 + 1;
    for (int i = 1; i <= M; i ++) {
        while (users[i].remainCount) {
            int batch = (servers[1].npus[1].totalMemory - B) / A;
            batch = std::min(batch, users[i].remainCount);
            ans[i].push_back({start_time, 1, 1, batch});
            users[i].remainCount -= batch;
            start_time += 21;
        }
    }

    for (int i = 1; i <= M; i ++) {
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
    read_data();
    if (N == 1 and servers[1].npuCount == 1) {
        bad_strategy();
    } else {
        solve();
    }
    return 0;
}
