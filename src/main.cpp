#include <bits/stdc++.h>

#define READY "ready" 
#define ARRIVED "arrived"
#define COMPLETED "completed"


// 计数从1开始
int N;  // 服务器数量
int M; // 用户数量
int A, B; // 显存与batchsize之间的关系，Mem = a * bs + b

class Request {
public:
    int requestTime; // 请求的时间
    int userId;         // 用户的Id
    int batchSize;     // batchsize
    int serverId;       // 请求的服务器id
    int npuId;          // 请求的npu的id

    Request(){};
    Request(int requestTime, int userId, int batchSize) {
        this->requestTime = requestTime;
        this->userId = userId;
        this->batchSize = batchSize;
    }

    bool operator<(const Request& other) const { // 声明小于号重载
        return this->requestTime < other.requestTime or
            this->userId < other.userId or
            this->batchSize < other.batchSize;
    }; 

};


class Event {
public: 
    int time; // 事件的时间
    int prior; // 事件的优先级, 数值越小优先级越高
    std::string type; // 表示当前的事件类型
    Request request;

    bool operator<(const Event& other) const { // 声明小于号重载
       if (this->time == other.time) return this->prior > other.prior;
       else return this->time > other.time;
    }; 

};

std::priority_queue<Event> eventQueue;


class NPU {
public:
    int severId; // 所属的服务器id
    int id; // 编号
    int totalMemory; // 显存容量（单位：GB）
    int freeMemory;  // 剩余显存（单位：GB）
    int inferenceSpeed; // 推理速度（单位：推理/秒）
    std::vector<Request> requests; // 请求队列


    int calculateInferenceTime (int batchSize) {
        return ceil(sqrtl(batchSize) / inferenceSpeed);
    }

    void handleRequest(int currentTime) {
        // 1. 移除已完成推理的请求。
        // 2. 增加当前时刻接收到的请求。    
        // 3. 将序列排序
        // 4. 从队首至队尾依次扫描请求，若加上该请求所需显存后未超过服务器显存，则认为本毫秒对该
        // 请求分配推理资源。

        // std::cout << "当前NPU" << this->severId << " " << this->id << std::endl;
        sort(requests.begin(), requests.end());
        std::vector<Request> new_requests;
        for (int i = 0; i < (int)requests.size(); i ++) {
            if (requests[i].batchSize * A + B <= this->freeMemory) {
                Event reqcomp;
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
        requests = std::move(new_requests);
    }

    // 构造函数
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
    int id; // 服务器id
    int npuCount; // NPU个数

    std::vector<NPU> npus; // NPU实例
    
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
    int id; // 用户id
    int startTime; // 请求开始时间
    int endTime; // 请求结束时间
    int requestCount; // 请求数量
    int remainCount; // 剩余的未完成的请求数量
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
    // std::cout << a << std::endl;
    std::vector<std::vector<std::array<int, 4> > > ans(M + 1); // 最终输出
    std::vector<std::array<int, 2> > userNpu(M + 1, {0, 0}); // 用户指定的npu;
    std::set<std::array<int, 2> > needUpdateNPU;
    int currentServerId = 1, currentNpuId = 1;

    int cnt = 0;
    while (!eventQueue.empty()) {
        Event e = eventQueue.top(); // 假设你有一个Event指针
        eventQueue.pop();
        int currentTime = e.time;
        Request& request = e.request;
        
        // std::cout << "time: " << currentTime << std::endl;
        // std::cout << e.type << std::endl;


        if (e.type == COMPLETED) {
            // 是RequestCompletedEvent
            
            int serverId = request.serverId;
            int npuId = request.npuId;
            int batchSize = request.batchSize;
            servers[serverId].npus[npuId].freeMemory += A * batchSize + B;
            
            needUpdateNPU.insert({serverId, npuId});

        } else if (e.type == ARRIVED) {
            // 是RequestArrivedEvent
            
            int serverId = request.serverId;
            int npuId = request.npuId;
            int batchSize = request.batchSize;
            servers[serverId].npus[npuId].requests.push_back(request);

            needUpdateNPU.insert({serverId, npuId});
        } else if (e.type == READY) {
            // 是RequestReadyEvent
            int userId = request.userId;
            // 假定整个过程中不更换npu, 如果是第一个batch size, 需要计算分配到那个npu上，
            if (userNpu[userId][0] == 0) {
                userNpu[userId] = {currentServerId, currentNpuId};
                
                currentNpuId += 1;
                if (currentNpuId > servers[currentServerId].npuCount) {
                    currentServerId += 1;
                    currentNpuId = 1;
                }

                if (currentServerId > N) currentServerId = 1;
            }

            int serverId = userNpu[userId][0];
            int npuId = userNpu[userId][1];
            
            
            
            Event reqArrive, reqReady;
            reqArrive.time = e.time + latency[serverId][userId];
            reqArrive.prior = 2;
            reqArrive.type = ARRIVED;
            

            reqReady.time = e.time + latency[serverId][userId] + 1;
            reqReady.prior = 3;
            reqReady.type = READY;

            Request req;
            req.requestTime = e.time + latency[serverId][userId];
            req.serverId = serverId;
            req.npuId = npuId;
            req.userId = userId;
            req.batchSize = (servers[serverId].npus[npuId].totalMemory - B) / A;
            req.batchSize = std::min(req.batchSize, users[userId].remainCount);
            users[userId].remainCount -= req.batchSize;
            reqArrive.request = req;
            reqReady.request = req;


            if (req.batchSize > 0) eventQueue.push(reqArrive);
            if (users[userId].remainCount > 0) eventQueue.push(reqReady);
            ans[userId].push_back({e.time, serverId, npuId, req.batchSize});

        } 

        if (eventQueue.empty() or currentTime != eventQueue.top().time) {
            for (auto it = needUpdateNPU.begin(); it != needUpdateNPU.end(); it ++) {
                int serverId = it->at(0), npuId = it->at(1);
            }
            needUpdateNPU.clear();
        }

    }

    for (int i = 1; i <= M; i ++) {
        std::cout << ans[i].size() << std::endl;
        for (int j = 0; j < (int)ans[i].size(); j ++) {
            std::cout << ans[i][j][0] << " " << ans[i][j][1] << " "
                << ans[i][j][2] << " " << ans[i][j][3] << std::endl;
        }
    }
}
 

int main() {
    // ios::sync_with_stdio(false); cin.tie(0); clog.tie(0);
    solve();
    return 0;
}
