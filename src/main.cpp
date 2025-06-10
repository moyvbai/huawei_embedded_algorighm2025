#include <bits/stdc++.h>

#define READY "ready" 
#define ARRIVED "arrived"
#define COMPLETED "completed"


// ������1��ʼ
int N;  // ����������
int M; // �û�����
int a, b; // �Դ���batchsize֮��Ĺ�ϵ��Mem = a * bs + b

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
        return this->requestTime < other.requestTime or
            this->userId < other.userId or
            this->batchSize < other.batchSize;
    }; 

};


class Event {
public: 
    int time; // �¼���ʱ��
    int prior; // �¼������ȼ�, ��ֵԽС���ȼ�Խ��
    std::string type; // ��ʾ��ǰ���¼�����
    Request request;

    bool operator<(const Event& other) const { // ����С�ں�����
       if (this->time == other.time) return this->prior > other.prior;
       else return this->time > other.time;
    }; 

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
        sort(requests.begin(), requests.end());
        std::vector<Request> new_requests;
        for (int i = 0; i < (int)requests.size(); i ++) {
            if (requests[i].batchSize * a + b <= this->freeMemory) {
                // std::cout << "batch size: " << requests[i].batchSize << std::endl; 
                Event reqcomp;
                reqcomp.time = currentTime + calculateInferenceTime(requests[i].batchSize);
                reqcomp.prior = 1;
                reqcomp.type = COMPLETED;
                reqcomp.request = requests[i];

                // std::cout << "request complete time: " << reqcomp.time << std::endl;
                eventQueue.push(reqcomp);
                this->freeMemory -= (requests[i].batchSize * a + b);
            } else {
                new_requests.push_back(requests[i]);
            }
        }
        requests = new_requests;
    }

    // ���캯��
    NPU(){};
    NPU(int serverId, int id, int totalMemory, int freeMemory, int inferenceSpeed) {
        this->severId = serverId;
        this->id = id;
        this->totalMemory = totalMemory;
        this->freeMemory = freeMemory;
        this->inferenceSpeed = inferenceSpeed;
    } 
    
};

class Server {
public:
    int id; // ������id
    int g; // NPU����
    int k; // NPU�����ٶ�
    int m; // NPU�Դ��С��
    std::vector<NPU> npus; // NPUʵ��
    
    Server(){};
    Server(int id, int g, int k, int m) {
        this->id = id;
        this->g = g;
        this->k = k;
        this->m = m;

        npus.resize(g + 1);
        for (int i = 1; i <= g; i ++) {
            npus[i] = NPU(id, i, m, m, k);
        }
    }
};
std::vector<Server> servers;


class User {
public:
    int id; // �û�id
    int s; // ����ʼʱ��
    int e; // �������ʱ��
    int cnt; // ��������
    int remain_cnt; // ʣ���δ��ɵ���������
    User(){};
    User(int id, int s, int e, int cnt) {
        this->id = id;
        this->s = s;
        this->e = e;
        this->cnt = cnt;
        this->remain_cnt = cnt;
    }
};
std::vector<User> users;
std::vector<std::vector<int> > latency;


void solve() {
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
        e.prior = 2;
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

    int a, b; 
    std::cin >> a >> b;
    // std::cout << a << std::endl;
    std::vector<std::vector<std::array<int, 4> > > ans(M + 1); // �������
    std::vector<std::array<int, 2> > userNpu(M + 1, {0, 0}); // �û�ָ����npu;
    std::set<std::array<int, 2> > needUpdateNPU;
    int currentServerId = 1, currentNpuId = 1;

    int cnt = 0;
    while (!eventQueue.empty()) {
        Event e = eventQueue.top(); // ��������һ��Eventָ��
        int currentTime = e.time;
        Request& request = e.request;
        eventQueue.pop();
        // std::cout << "time: " << currentTime << std::endl;
        // std::cout << e.type << std::endl;
        cnt += 1;
        // if (cnt > 10000) break;

        if (e.type == COMPLETED) {
            // ��RequestCompletedEvent
            
            int serverId = request.serverId;
            int npuId = request.npuId;
            int batchSize = request.batchSize;
            servers[serverId].npus[npuId].freeMemory += a * batchSize + b;
            
            needUpdateNPU.insert({serverId, npuId});

        } else if (e.type == READY) {
            // ��RequestReadyEvent
            int userId = request.userId;
            // �ٶ����������в�����npu, ����ǵ�һ��batch size, ��Ҫ������䵽�Ǹ�npu�ϣ�
            if (userNpu[userId][0] == 0) {
                userNpu[userId] = {currentServerId, currentNpuId};
                
                // currentNpuId += 1;
                // if (currentNpuId > servers[currentServerId].g) {
                //     currentServerId += 1;
                //     currentNpuId = 1;
                // }

                // if (currentServerId > N) currentServerId = 1;
            }

            int serverId = userNpu[userId][0];
            int npuId = userNpu[userId][1];
            
            // std::cout << currentServerId << " " << currentNpuId << std::endl;
            
            
            Event reqArrive, reqReady;
            reqArrive.time = e.time + latency[serverId][userId];
            reqArrive.prior = 3;
            reqArrive.type = ARRIVED;
            

            reqReady.time = e.time + latency[serverId][userId] + 1;
            reqReady.prior = 2;
            reqReady.type = READY;

            Request req;
            req.requestTime = e.time;
            req.serverId = serverId;
            req.npuId = npuId;
            req.userId = userId;
            req.batchSize = (servers[serverId].m - b) / a;
            req.batchSize = std::min(req.batchSize, users[userId].remain_cnt);
            users[userId].remain_cnt -= req.batchSize;
            reqArrive.request = req;
            reqReady.request = req;


            if (req.batchSize) eventQueue.push(reqArrive);
            if (users[userId].remain_cnt) eventQueue.push(reqReady);
            ans[userId].push_back({e.time, serverId, npuId, req.batchSize});
            // std::cout << userId << std::endl;
            // std::cout << "arrived time: " << reqArrive.time << std::endl;

        } else if (e.type == ARRIVED) {
            // ��RequestArrivedEvent
            
            int serverId = request.serverId;
            int npuId = request.npuId;
            int batchSize = request.batchSize;
            // std::cout << serverId << " " << npuId << std::endl;
            // std::cout << servers[serverId].npus.size() << std::endl;
            servers[serverId].npus[npuId].requests.push_back(request);

            needUpdateNPU.insert({serverId, npuId});
            // std::cout << "arrived" << std::endl;
        }

        if (eventQueue.empty() or currentTime != eventQueue.top().time) {
            // std::cout << "start handle" << " " << needUpdateNPU.size() << " ��NPU" << std::endl;
            for (auto it = needUpdateNPU.begin(); it != needUpdateNPU.end(); it ++) {
                int serverId = it->at(0), npuId = it->at(1);
                servers[serverId].npus[npuId].handleRequest(currentTime);
            }
            needUpdateNPU.clear();
        }

    }


    int maxTime = 0;
    for (int i = 1; i <= M; i ++) {
        std::cout << ans[i].size() << std::endl;
        int sz = ans[i].size(), s = 0;
        if (sz <= 0 or sz > 300) {
            std::vector<Request> allRequests(1e9);
        } 
        for (int j = 0; j < (int)ans[i].size(); j ++) {
            std::cout << ans[i][j][0] << " " << ans[i][j][1] << " "
                << ans[i][j][2] << " " << ans[i][j][3] << "\n";
            
            s += ans[i][j][3];
        }
        
        if (s != users[i].cnt) {
            std::vector<Request> allRequests(1e9);
        }

    }
    // std::cout << maxTime << std::endl << std::endl;
    
    // std::sort(userNpu.begin(), userNpu.end());
    // for (int i = 1; i <= M; i ++) {
    //     // std::cout << userNpu[i][0] << " " << userNpu[i][1] << std::endl;
    //     if (users[i].remain_cnt) std::cout << users[i].remain_cnt << std::endl;
    // }
}
 

int main() {
    // ios::sync_with_stdio(false); cin.tie(0); clog.tie(0);
    solve();
    return 0;
}
