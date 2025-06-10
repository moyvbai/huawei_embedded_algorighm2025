#include <bits/stdc++.h>
using namespace std;

// ??????1???
int N;  // ??????????
int M; // ???????
int a, b; // ?????batchsize????????Mem = a * bs + b

class Request {
public:
    int requestTime; // ????????
    int userId;         // ?????Id
    int batchSize;     // batchsize

    Request(){};
    Request(int requestTime, int userId, int batchSize) {
        this->requestTime = requestTime;
        this->userId = userId;
        this->batchSize = batchSize;
    }

    bool operator<(const Request& other) const { // ????§³???????
        return this->requestTime < other.requestTime or
            this->userId < other.userId or
            this->batchSize < other.batchSize;
    }; 

};




class Event {
public: 
    int time; // ????????

    bool operator<(const Event& other) const { // ????§³???????
       return this->time < other.time;
    }; 
};

// ??????????
class RequestCompletedEvent: public Event {
public:
    int serverId; 
    int npuId;
    Request request;
};

// ?????????????????
class RequestReadyEvent: public Event {
public:
    int userId;
};

// ??????????
class RequestArrivedEvent: public Event {
    int serverId;
    int npuId;
    Request request;
};


std::priority_queue<Event> eventQueue;


class NPU {
public:
    int severId; // ???????????id
    int id; // ???
    int totalMemory; // ???????????¦Ë??GB??
    int freeMemory;  // ?????—¤??¦Ë??GB??
    int inferenceSpeed; // ??????????¦Ë??????/??
    std::vector<Request> requests; // ???????

    int batchCount = 0;

    int calculateInferenceTime (int batchSize) {
        return ceil(sqrtl(batchSize) / inferenceSpeed);
    }

    void handleRequest(int currentTime) {
        // 1. ??????????????????
        // 2. ???????????????????    
        // 3. ??????????
        // 4. ?????????¦Â??????????????????????????????¦Ä????????????—¨?????????????
        // ????????????????

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

    // ??????
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
    int id; // ??????id
    int g; // NPU????
    int k; // NPU???????
    int m; // NPU????§³??
    vector<NPU> npus; // NPU???
    
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
    int id; // ???id
    int s; // ????????
    int e; // ??????????
    int cnt; // ????????
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
    vector<int> a1(a, 0);
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

        // npuId ++;
        // if (npuId > servers[serverId].g) {
        //     // cout << npuId << endl;
        //     serverId += 1;
        //     npuId = 1;

        //     if (serverId > N) serverId = 1;
        // }

    }
    vector<Request> allreqs(1000000000);

    // while(true) {
    //     clog << 1 << std::endl;
    // }


}
 

int main() {
    // ios::sync_with_stdio(false); cin.tie(0); clog.tie(0);
    solve();
    return 0;
}
