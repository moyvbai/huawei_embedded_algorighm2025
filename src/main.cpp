#include <bits/stdc++.h>
using namespace std;

// 计数从1开始
int N;  // 服务器数量
int M; // 用户数量
int a, b; // 显存与batchsize之间的关系，Mem = a * bs + b

class Request {
public:
    int requestTime; // 请求的时间
    int userId;         // 用户的Id
    int batchSize;     // batchsize

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

    bool operator<(const Event& other) const { // 声明小于号重载
       return this->time < other.time;
    }; 
};

// 请求完成事件
class RequestCompletedEvent: public Event {
public:
    int serverId; 
    int npuId;
    Request request;
};

// 请求满足发送条件事件
class RequestReadyEvent: public Event {
public:
    int userId;
};

// 请求收到事件
class RequestArrivedEvent: public Event {
    int serverId;
    int npuId;
    Request request;
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

    int batchCount = 0;

    int calculateInferenceTime (int batchSize) {
        return ceil(sqrtl(batchSize) / inferenceSpeed);
    }

    void handleRequest(int currentTime) {
        // 1. 移除已完成推理的请求。
        // 2. 增加当前时刻接收到的请求。    
        // 3. 将序列排序
        // 4. 从队首至队尾依次扫描请求，若加上该请求所需显存后未超过服务器显存，则认为本毫秒对该
        // 请求分配推理资源。

        sort(requests.begin(), requests.end());
        for (int i = 0; i < (int)requests.size(); i ++) {
            if (requests[i].batchSize <= this->freeMemory) {
                RequestCompletedEvent reqcomp;
                reqcomp.time = currentTime + calculateInferenceTime(requests[i].batchSize);
                reqcomp.serverId = severId;
                reqcomp.npuId = id;
                reqcomp.request = requests[i];

                eventQueue.push(reqcomp);
                this->freeMemory -= requests[i].batchSize;
            }
        }
    }

    // 构造函数
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
    int id; // 服务器id
    int g; // NPU个数
    int k; // NPU推理速度
    int m; // NPU显存大小；
    vector<NPU> npus; // NPU实例
    
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
vector<Server> servers;


class User {
public:
    int id; // 用户id
    int s; // 请求开始时间
    int e; // 请求结束时间
    int cnt; // 请求数量
    User(){};
    User(int id, int s, int e, int cnt) {
        this->id = id;
        this->s = s;
        this->e = e;
        this->cnt = cnt;
    }
};
vector<User> users;
vector<vector<int> > latency;


void solve() {
    cin >> N;
    clog << N << endl;
    servers.resize(N + 1);
    for (int i = 1; i <= N; i ++) {
        int gi, ki, mi; cin >> gi >> ki >> mi;
        servers[i] = Server(i, gi, ki, mi);
    }

    cin >> M;
    users.resize(M + 1);
    clog << M << endl;
    for (int i = 1; i <= M; i ++) {
        // clog << i << endl;
        int si, ei, cnti; cin >> si >> ei >> cnti;
        // clog << si << ei << cnti << endl;
        users[i] = User(i, si, ei, cnti);
        // clog << i << endl;
    }


    latency.resize(N + 1, vector<int>(M + 1));
    for (int i = 1; i <= N; i ++) {
        for (int j = 1; j <= M; j ++) {
            cin >> latency[i][j];
        }
    }
    // clog << "over!" << endl;

    int a, b; cin >> a >> b;
    clog << a << b << endl;
    int serverId = 1, npuId = 1;
    for (int i = 1; i <= M; i ++) {
        vector<vector<int> > ans;
        int total = users[i].cnt;
        int startTime = users[i].s;
        while (total) {
            int bi = (servers[serverId].m - b) / a;
            bi = min(total, bi);
            ans.push_back({startTime, serverId, npuId, bi});
            startTime += latency[serverId][i] + 1;
            total -= bi;
        }

        cout << ans.size() << endl;
        for (int j = 0; j < ans.size(); j ++) {
            cout << ans[j][0] << " " << ans[j][1] << " "
                << ans[j][2] << " " << ans[j][3] << endl;
        }

        npuId ++;
        if (npuId > servers[serverId].g) {
            // cout << npuId << endl;
            serverId += 1;
            npuId = 1;

            if (serverId > N) serverId = 1;
        }

    }

}
 

int main() {
    // ios::sync_with_stdio(false); cin.tie(0); clog.tie(0);
    solve();
    return 0;
}

