#include <bits/stdc++.h>
using namespace std;

// ������1��ʼ
int N;  // ����������
int M; // �û�����
int a, b; // �Դ���batchsize֮��Ĺ�ϵ��Mem = a * bs + b

class Request {
public:
    int requestTime; // �����ʱ��
    int userId;         // �û���Id
    int batchSize;     // batchsize

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

    bool operator<(const Event& other) const { // ����С�ں�����
       return this->time < other.time;
    }; 
};

// ��������¼�
class RequestCompletedEvent: public Event {
public:
    int serverId; 
    int npuId;
    Request request;
};

// �������㷢�������¼�
class RequestReadyEvent: public Event {
public:
    int userId;
};

// �����յ��¼�
class RequestArrivedEvent: public Event {
    int serverId;
    int npuId;
    Request request;
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

    int batchCount = 0;

    int calculateInferenceTime (int batchSize) {
        return ceil(sqrtl(batchSize) / inferenceSpeed);
    }

    void handleRequest(int currentTime) {
        // 1. �Ƴ���������������
        // 2. ���ӵ�ǰʱ�̽��յ�������    
        // 3. ����������
        // 4. �Ӷ�������β����ɨ�����������ϸ����������Դ��δ�����������Դ棬����Ϊ������Ը�
        // �������������Դ��

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
    vector<NPU> npus; // NPUʵ��
    
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
    int id; // �û�id
    int s; // ����ʼʱ��
    int e; // �������ʱ��
    int cnt; // ��������
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

