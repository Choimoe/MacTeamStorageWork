#include <cstdio>
#include <cassert>
#include <cstdlib>
#include <algorithm>
#include <cstring>
#include <iostream>
#include <cmath>
#include <vector>
#include <unordered_map>
#include <string>
#include <queue>
#include <stack>

#define MAX_DISK_NUM (10 + 1)
#define MAX_DISK_SIZE (16384 + 1)
#define MAX_REQUEST_NUM (30000000 + 1)
#define MAX_OBJECT_NUM (100000 + 1)
#define REP_NUM (3)
#define FRE_PER_SLICING (1800)
#define EXTRA_TIME (105)

typedef struct Request_ {
    int object_id;
    int prev_id;
    bool is_done;
    int time;

    std::string head_movement; 
} Request;

typedef struct Object_ {
    int replica[REP_NUM + 1];
    int* unit[REP_NUM + 1];
    int size;
    int last_request_point;
    bool is_delete;

    std::queue<int> active_phases;
} Object;

// 新增磁头状态结构
typedef struct DiskHead_ {
    int pos;            // 当前磁头位置（存储单元编号）
    int last_action;    // 上一次动作类型：0-Jump,1-Pass,2-Read
    int last_token;     // 上一次消耗的令牌数
} DiskHead;

Request request[MAX_REQUEST_NUM];
Object object[MAX_OBJECT_NUM];

int T, M, N, V, G;
int disk[MAX_DISK_NUM][MAX_DISK_SIZE];
int disk_point[MAX_DISK_NUM];
int timestamp;

DiskHead disk_head[MAX_DISK_NUM];

void timestamp_action()
{
    int cur_time;
    scanf("%*s%d", &cur_time);
    printf("TIMESTAMP %d\n", cur_time);
    timestamp = cur_time;

    fflush(stdout);
}

void do_object_delete(const int* object_unit, int* disk_unit, int size)
{
    for (int i = 1; i <= size; i++) {
        disk_unit[object_unit[i]] = 0;
    }
}

void delete_action()
{
    int n_delete;
    int abort_num = 0;
    static int _id[MAX_OBJECT_NUM];

    scanf("%d", &n_delete);
    for (int i = 1; i <= n_delete; i++) {
        scanf("%d", &_id[i]);
    }

    for (int i = 1; i <= n_delete; i++) {
        int id = _id[i];
        int current_id = object[id].last_request_point;
        while (current_id != 0) {
            if (request[current_id].is_done == false) {
                abort_num++;
            }
            current_id = request[current_id].prev_id;
        }
    }

    printf("%d\n", abort_num);
    for (int i = 1; i <= n_delete; i++) {
        int id = _id[i];
        int current_id = object[id].last_request_point;
        while (current_id != 0) {
            if (request[current_id].is_done == false) {
                printf("%d\n", current_id);
            }
            current_id = request[current_id].prev_id;
        }
        for (int j = 1; j <= REP_NUM; j++) {
            do_object_delete(object[id].unit[j], disk[object[id].replica[j]], object[id].size);
        }
        object[id].is_delete = true;
    }

    fflush(stdout);
}

void do_object_write(int* object_unit, int* disk_unit, int size, int object_id)
{
    int current_write_point = 0;
    for (int i = 1; i <= V; i++) {
        if (disk_unit[i] == 0) {
            disk_unit[i] = object_id;
            object_unit[++current_write_point] = i;
            if (current_write_point == size) {
                break;
            }
        }
    }

    assert(current_write_point == size);
}

void write_action()
{
    int n_write;
    scanf("%d", &n_write);
    for (int i = 1; i <= n_write; i++) {
        int id, size;
        scanf("%d%d%*d", &id, &size);
        object[id].last_request_point = 0;
        for (int j = 1; j <= REP_NUM; j++) {
            object[id].replica[j] = (id + j) % N + 1;
            object[id].unit[j] = static_cast<int*>(malloc(sizeof(int) * (size + 1)));
            object[id].size = size;
            object[id].is_delete = false;
            do_object_write(object[id].unit[j], disk[object[id].replica[j]], size, id);
        }

        printf("%d\n", id);
        for (int j = 1; j <= REP_NUM; j++) {
            printf("%d", object[id].replica[j]);
            for (int k = 1; k <= size; k++) {
                printf(" %d", object[id].unit[j][k]);
            }
            printf("\n");
        }
    }

    fflush(stdout);
}

// 紧凑性计算函数（最大间隙越小越好）
int calculate_compactness(const int* blocks, int size) {
    if(size <= 1) return 0;
    
    int sorted[MAX_DISK_SIZE];
    memcpy(sorted, blocks+1, sizeof(int)*size);
    std::sort(sorted, sorted+size);
    
    int score = 0, lst = 64, pass = 0;
    for(int i=1; i<size; i++){
        lst = (pass ? 64 : std::max(16, (int)ceil(lst * 0.8)));
        pass = sorted[i] - sorted[i-1] - 1;
        score += pass + lst;
    }
    
    return score;
}

// 计算移动到第一个块的最短距离（环形）
int calculate_move_cost(int current_pos, int first_block) {
    int clockwise = (first_block - current_pos + V) % V;
    return clockwise;
}

// 副本评分函数
int evaluate_replica(int rep_id, const Object* obj, int current_time) {
    const int disk_id = obj->replica[rep_id];
    const int head_pos = disk_head[disk_id].pos;
    const int* blocks = obj->unit[rep_id];
    
    // 紧凑性评分
    int compactness = G - std::min(G, calculate_compactness(blocks, obj->size));
    
    // 移动成本评分
    int move_cost = G - std::min(G, calculate_move_cost(head_pos, blocks[1]));
    
    // 时间局部性评分（需预存标签访问模式）
    // 此处需要接入预处理数据，暂用固定值
    int time_score = 0;
    
    return compactness*1 + move_cost*1 + time_score*1;
}

// 选择最佳副本
int select_best_replica(int object_id) {
    const Object* obj = &object[object_id];
    int best_rep = -1;
    int max_score = -1;
    
    for(int rep=1; rep<=REP_NUM; rep++){
        int score = evaluate_replica(rep, obj, 0);
        if(score > max_score){
            max_score = score;
            best_rep = rep;
        }
        std::cerr << "[DEBUG] " << " rep: " << rep << " score: " << score << std::endl;
    }
    std::cerr << "[DEBUG] " << " best_rep: " << best_rep << std::endl;
    return best_rep;
}

void do_object_read(int object_id, int &current_phase){
    int best_rep = select_best_replica(object_id);
    int target_disk = object[object_id].replica[best_rep];
    int is_read = 0;
    for (int i = 1; i <= N; i++) {
        if (i == target_disk) {
            int target_pos = object[object_id].unit[best_rep][current_phase + 1];
            int remain_token = G;
            std::cerr << "[DEBUG] " << " current_phase: " << current_phase << " object_id: " << object_id << " target_pos: " << target_pos << " disk_head[target_disk].pos: " << disk_head[target_disk].pos << std::endl;
            if (target_pos != disk_head[target_disk].pos) {
                int pass_cost = calculate_move_cost(disk_head[target_disk].pos, target_pos);
                if (pass_cost > G) {
                    std::cerr << "[OUTPUT] " << " jump" << std::endl;
                    remain_token -= G;
                    disk_head[target_disk].pos = target_pos;
                    disk_head[target_disk].last_action = 0;
                    disk_head[target_disk].last_token = G;
                    printf("j %d\n", target_pos);
                    continue;
                } else {
                    remain_token -= pass_cost;
                    for (int i = 1; i <= pass_cost; i++) {
                        printf("p");
                        std::cerr << "[OUTPUT] " << " pass" << std::endl;
                    }
                    disk_head[target_disk].pos = target_pos;
                    disk_head[target_disk].last_action = 1;
                    disk_head[target_disk].last_token = 1;
                }
            }
            while(true) {
                int move_cost = (disk_head[target_disk].last_action != 2) ? 64 : 
                    std::max(16, (int)ceil(disk_head[target_disk].last_token * 0.8));
                target_pos = object[object_id].unit[best_rep][current_phase + 1];
                if(move_cost > remain_token || current_phase == object[object_id].size || target_pos != disk_head[target_disk].pos) {
                    if(target_pos != disk_head[target_disk].pos) {
                        std::cerr << "[DEBUG] " << " move_cost: " << move_cost << " remain_token: " << remain_token << " target_disk: " << target_disk << std::endl;
                    }
                    printf("#\n");
                    std::cerr << "[OUTPUT] " << " end" << std::endl;
                    break;
                }
                current_phase++;
                printf("r");
                std::cerr << "[OUTPUT] " << " read" << std::endl;
                disk_head[target_disk].pos = (disk_head[target_disk].pos % V) + 1;
                disk_head[target_disk].last_action = 2;
                disk_head[target_disk].last_token = move_cost;
                remain_token -= move_cost;
            }
        } else {
            printf("#\n");
        }
    }
}

void read_action()
{
    int n_read;
    int request_id = -1, object_id;
    scanf("%d", &n_read);
    static std::stack<int> new_requests;
    for (int i = 1; i <= n_read; i++) {
        scanf("%d%d", &request_id, &object_id);
        request[request_id].object_id = object_id;
        request[request_id].prev_id = object[object_id].last_request_point;
        object[object_id].last_request_point = request_id;
        object[object_id].active_phases.push(request_id);
        request[request_id].is_done = false;
        request[request_id].time = timestamp;
        new_requests.push(request_id);
    }

    static int current_request = 0;
    static int current_phase = 0;
    if (!current_request && n_read > 0) {
        current_request = new_requests.top();
        new_requests.pop();
    }
    if (!current_request) {
        std::cerr << "[DEBUG] skip read" << " current_request: " << current_request << " request_id: " << request_id << std::endl;
        for (int i = 1; i <= N; i++) {
            printf("#\n");
        }
        printf("0\n");
        fflush(stdout);
        return;
    }

    object_id = request[current_request].object_id;

    do_object_read(object_id, current_phase);

    std::vector<int> finished_phases;

    if (current_phase == object[object_id].size) {
        if (object[object_id].is_delete) {
            printf("0\n");
        } else {
            auto *active_phases = &object[object_id].active_phases;
            while (!active_phases->empty() && active_phases->front() <= current_request) {
                finished_phases.push_back(active_phases->front());
                request[active_phases->front()].is_done = true;
                active_phases->pop();
            }
            int fsize = finished_phases.size();
            // printf("1\n%d\n", current_request);
            // std::cerr << "[DEBUG] " << " finished_phases: " << fsize << " current_request: " << current_request << std::endl;
            printf("%d\n", fsize);
            // std::cerr << "[OUTPUT] " << " finished_phases: " << fsize << std::endl;
            for (int i = 0; i < fsize; i++) {
                printf("%d\n", finished_phases[i]);
                // std::cerr << finished_phases[i];
            }
            // std::cerr << "\n";
            
            request[current_request].is_done = true;
        }
        current_request = 0;
        current_phase = 0;
    } else {
        printf("0\n");
    }
    fflush(stdout);
}

void clean()
{
    for (auto& obj : object) {
        for (int i = 1; i <= REP_NUM; i++) {
            if (obj.unit[i] == nullptr)
                continue;
            free(obj.unit[i]);
            obj.unit[i] = nullptr;
        }
    }
}

int main()
{
    freopen("log.txt", "w", stderr);

    scanf("%d%d%d%d%d", &T, &M, &N, &V, &G);

    for(int i=1; i<=N; i++) disk_head[i].pos = 1;

    for (int i = 1; i <= M; i++) {
        for (int j = 1; j <= (T - 1) / FRE_PER_SLICING + 1; j++) {
            scanf("%*d");
        }
    }

    for (int i = 1; i <= M; i++) {
        for (int j = 1; j <= (T - 1) / FRE_PER_SLICING + 1; j++) {
            scanf("%*d");
        }
    }

    for (int i = 1; i <= M; i++) {
        for (int j = 1; j <= (T - 1) / FRE_PER_SLICING + 1; j++) {
            scanf("%*d");
        }
    }

    printf("OK\n");
    fflush(stdout);

    for (int i = 1; i <= N; i++) {
        disk_point[i] = 1;
    }

    for (int t = 1; t <= T + EXTRA_TIME; t++) {
        timestamp_action();
        delete_action();
        write_action();
        read_action();
    }
    clean();

    return 0;
}