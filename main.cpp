#include <cstdio>
#include <cassert>
#include <cstdlib>
#include <algorithm>
#include <cstring>
#include <iostream>
#include <set>
#include <vector>
#include <queue>
#include <cmath>
#include <unordered_map>
#include <tuple>

#define MAX_DISK_NUM (10 + 1)
#define MAX_DISK_SIZE (16384 + 1)
#define MAX_REQUEST_NUM (30000000 + 1)
#define MAX_OBJECT_NUM (100000 + 1)
#define REP_NUM (3)
#define FRE_PER_SLICING (1800)
#define EXTRA_TIME (105)
#define TAG_PHASE (48 + 1)
#define MAX_TAG_NUM (16 + 1)

typedef struct Request_ {
    int object_id;
    int prev_id;
    bool is_done;

    int time;
} Request;

typedef struct Object_ {
    int replica[REP_NUM + 1];
    int* unit[REP_NUM + 1];
    int size;
    int last_request_point;
    bool is_delete;
    std::queue<int> request_queue;
} Object;

// 新增磁头状态结构
typedef struct DiskHead_ {
    int pos;            // 当前磁头位置（存储单元编号）
    int last_action;    // 上一次动作类型：0-Jump,1-Pass,2-Read
    int last_token;     // 上一次消耗的令牌数
    std::multiset<std::tuple<int, int, int>> token_queue;
} DiskHead;

Request request[MAX_REQUEST_NUM];
Object object[MAX_OBJECT_NUM];

int T, M, N, V, G;
int disk[MAX_DISK_NUM][MAX_DISK_SIZE];
int disk_point[MAX_DISK_NUM];

DiskHead disk_head[MAX_DISK_NUM];
int timestamp;

// 处理时间戳更新操作
void timestamp_action() {
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


// 计算磁盘disk_id的最大连续空闲块长度
int calculate_max_contiguous(int disk_id) {
    int max_len = 0, current_len = 0;
    for (int i = 1; i <= V; i++) {
        if (disk[disk_id][i] == 0) {
            current_len++;
            max_len = std::max(max_len, current_len);
        } else {
            current_len = 0;
        }
    }
    // 环形处理：检查首尾连接的情况（例如末尾和开头连续）
    if (disk[disk_id][V] == 0 && disk[disk_id][1] == 0) {
        int head = 1, tail = V;
        while (disk[disk_id][head] == 0 && head <= V) head++;
        while (disk[disk_id][tail] == 0 && tail >= 1) tail--;
        max_len = std::max(max_len, (V - tail) + (head - 1) + 2);
    }
    return max_len;
}

int calculate_max_space(int disk_id) {
    int max_space = 0;
    for (int i = 1; i <= V; i++) {
        if (disk[disk_id][i] == 0) {
            max_space++;
        }
    }
    return max_space;
}

// 选择三个不同磁盘（优先选择连续空间大的）
std::vector<int> select_disks_for_object(int id) {
    std::vector<std::pair<int, int> > disk_scores;
    // 遍历所有磁盘，计算得分（连续空间 >= size的磁盘才有资格）
    for (int i = 1; i <= N; i++) {
        int contiguous = calculate_max_contiguous(i);
        int max_space = calculate_max_space(i);
        // std::cerr << "[DEBUG] disk_id: " << i << " contiguous: " << contiguous << " max_space: " << max_space << " tag_num: " << disk_tag_num[i][object[id].tag] << std::endl;
        if (contiguous >= object[id].size) {
            disk_scores.emplace_back(contiguous, i);
            // std::cerr << "[DEBUG] disk_id: " << i << " score: " << disk_tag_num[i][object[id].tag] << std::endl;
        }
    }
    // 按连续空间降序排序
    std::sort(disk_scores.rbegin(), disk_scores.rend());
    
    std::vector<int> selected;
    for (auto& [score, disk_id] : disk_scores) {
        if (selected.size() >= 3) break;
        if (std::find(selected.begin(), selected.end(), disk_id) == selected.end()) {
            selected.push_back(disk_id);
        }
    }
    return selected;
}

// 在磁盘disk_id上分配size个连续块（返回分配的存储单元编号列表）
std::vector<int> allocate_contiguous_blocks(int disk_id, int size, int object_id) {
    // 从磁头当前位置开始搜索（减少未来读取时的移动距离）
    int start = disk_head[disk_id].pos;
    for (int i = 0; i < V; i++) {
        int pos = (start + i) % V;
        if (pos == 0) pos = V; // 存储单元编号从1开始
        if (disk[disk_id][pos] == 0) {
            bool found = true;
            std::vector<int> blocks;
            // 检查后续size个单元是否都空闲
            for (int j = 0; j < size; j++) {
                int check_pos = (pos + j) % V;
                if (check_pos == 0) check_pos = V;
                if (disk[disk_id][check_pos] != 0) {
                    found = false;
                    break;
                }
            }
            if (found) {
                for (int j = 0; j < size; j++) {
                    int block_pos = (pos + j) % V;
                    if (block_pos == 0) block_pos = V;
                    blocks.push_back(block_pos);
                    disk[disk_id][block_pos] = object_id; // 标记为已占用
                }
                return blocks;
            }
        }
    }
    return {}; // 空间不足
}


// 写入对象的函数
void do_object_write(int* object_unit, int* disk_unit, int size, int object_id)
{
    int current_write_point = 0; // 当前写入位置
    for (int i = 1; i <= V; i++) {
        if (disk_unit[i] == 0) { // 如果磁盘单元为空
            disk_unit[i] = object_id; // 写入对象 ID
            object_unit[++current_write_point] = i; // 记录写入位置
            if (current_write_point == size) {
                break; // 如果写入完成，退出
            }
        }
    }

    assert(current_write_point == size); // 确保写入的大小正确
}

// 写操作
void write_action()
{
    int n_write; // 写请求数量
    scanf("%d", &n_write); // 读取写请求的数量
    for (int i = 1; i <= n_write; i++) {
        int id, size, tag;
        scanf("%d%d%d", &id, &size, &tag); // 读取对象 ID 和大小
        object[id].last_request_point = 0; // 初始化对象的最后请求指针
        object[id].size = size; // 设置对象的大小
        object[id].is_delete = false; // 标记对象为未删除
        std::vector<int> selected_disks = select_disks_for_object(id);
        for (int j = 1; j <= REP_NUM; j++) {
            int disk_id = selected_disks[j - 1];
            std::vector<int> blocks = allocate_contiguous_blocks(disk_id, size, id);
            object[id].replica[j] = disk_id;
            object[id].unit[j] = static_cast<int*>(malloc(sizeof(int) * (size + 1))); // 分配内存以存储对象数据
            for (int _ = 0; _ < size; _++) {
                object[id].unit[j][_+1] = blocks[_];
            }
        }

        printf("%d\n", id); // 打印对象 ID
        for (int j = 1; j <= REP_NUM; j++) {
            printf("%d", object[id].replica[j]); // 打印副本 ID
            for (int k = 1; k <= size; k++) {
                printf(" %d", object[id].unit[j][k]); // 打印对象数据
            }
            printf("\n");
        }
    }

    fflush(stdout); // 刷新输出缓冲区
}

// 紧凑性计算函数（最大间隙越小越好）
int calculate_compactness(const int* blocks, int size) {
    if(size <= 1) return 0;
    
    int sorted[MAX_DISK_SIZE];
    memcpy(sorted, blocks+1, sizeof(int)*size);
    std::sort(sorted, sorted+size);
    
    int max_gap = 0;
    for(int i=1; i<size; i++){
        max_gap = std::max(max_gap, sorted[i]-sorted[i-1]);
    }
    // 处理环形间隙
    max_gap = std::max(max_gap, (V - sorted[size-1]) + sorted[0]);
    return max_gap;
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
    
    // 紧凑性评分（权重40%）
    int compactness = V - calculate_compactness(blocks, obj->size);
    
    // 移动成本评分（权重50%）
    int move_cost = V - calculate_move_cost(head_pos, blocks[1]);
    
    // 时间局部性评分（权重10%，需预存标签访问模式）
    // 此处需要接入预处理数据，暂用固定值
    int time_score = 0;
    
    return compactness*4 + move_cost*5 + time_score*1;
}

// 选择最佳副本
int select_best_replica(int object_id) {
    const Object* obj = &object[object_id];
    int best_rep = 1;
    int max_score = -1;
    
    for(int rep=1; rep<=REP_NUM; rep++){
        int score = evaluate_replica(rep, obj, 0);
        if(score > max_score){
            max_score = score;
            best_rep = rep;
        }
    }
    return best_rep;
}

double current_score(int request_id) {
    int object_id = request[request_id].object_id;
    int timex = timestamp - request[request_id].time;
    double fx = 0;
    if (timex <= 10) fx = 1.0 - 0.005 * timex;
    else if (timex <= 105) fx = 1.05 - 0.01 * timex;
    return fx * (object[object_id].size + 1.0) * 0.5;
}

void read_action()
{
    int n_read;
    int request_id, object_id;
    scanf("%d", &n_read);
    for (int i = 1; i <= n_read; i++) {
        scanf("%d%d", &request_id, &object_id);
        request[request_id].object_id = object_id;
        request[request_id].prev_id = object[object_id].last_request_point;
        object[object_id].last_request_point = request_id;
        object[object_id].request_queue.push(request_id);
        request[request_id].is_done = false;
        request[request_id].time = timestamp;
        for (int j = 1; j <= REP_NUM; j++) {
            auto it = disk_head[object[object_id].replica[j]].token_queue.lower_bound(std::make_tuple(object[object_id].unit[j][1], request_id, -1));
            if (it != disk_head[object[object_id].replica[j]].token_queue.end()  && std::get<0>(*it) == object[object_id].unit[j][1]) {
                disk_head[object[object_id].replica[j]].token_queue.erase(it);
            }
            disk_head[object[object_id].replica[j]].token_queue.insert(std::make_tuple(object[object_id].unit[j][1], -request_id, 0));
        }
    }

    static int current_request = 0;
    static int current_phase = 0;
    double total_score = 0, score_cnt = 0;

    if (!current_request && n_read > 0) {
        current_request = request_id;
    }

    std::vector<int> finished_request;
    std::vector<std::string> movement(N+1);

    for (int i = 1; i <= N; i++) {
        int head_pos = disk_head[i].pos;
        int remain_token = G;
        int last_request_id = 0, cnt = 0;
        auto next_request = disk_head[i].token_queue.begin();
        // std::cerr << "[DEBUG] disk " << i << ": head_pos: " << head_pos << std::endl;
        while (remain_token > 0) {
            if (disk_head[i].token_queue.empty()) break;
            next_request = disk_head[i].token_queue.upper_bound(std::make_tuple(head_pos, -MAX_REQUEST_NUM, -1));
            if (next_request == disk_head[i].token_queue.end()) {
                next_request = disk_head[i].token_queue.begin();
            }
            int object_id = request[-std::get<1>(*next_request)].object_id;
            if (object[object_id].is_delete || request[-std::get<1>(*next_request)].is_done) {
                disk_head[i].token_queue.erase(next_request);
                continue;
            }
            double next_score = current_score(-std::get<1>(*next_request));
            total_score += next_score; score_cnt++;
            if (next_score < total_score / score_cnt * 0.85) {
                disk_head[i].token_queue.erase(next_request);
                continue;
            }
            // if (std::get<2>(*next_request) != 0) {
            //     std::cerr << "[DEBUG] request_id: [" << -std::get<1>(*next_request) << "] object_id: " << object_id << " (remain " << object[object_id].size - std::get<2>(*next_request) << " blocks)" << " is_delete: " << object[object_id].is_delete << " is_done: " << request[-std::get<1>(*next_request)].is_done << std::endl;
            // }
            last_request_id = -std::get<1>(*next_request);
            // std::cerr << "[DEBUG]         next_request: [" << next_request->first << "] request_id: " << -next_request->second << " is_done: " << request[-next_request->second].is_done << std::endl;
            //jump to next request
            int pass_cost = calculate_move_cost(disk_head[i].pos, std::get<0>(*next_request));
            if (remain_token < std::min(G, pass_cost)) break;
            if (pass_cost > G) {
                movement[i] = "j " + std::to_string(std::get<0>(*next_request)) + "\n";
                disk_head[i].pos = std::get<0>(*next_request);
                disk_head[i].last_action = 0;
                disk_head[i].last_token = G;
                remain_token -= G;
            } else {
                for (int _ = 1; _ <= pass_cost; _++) {
                    movement[i] += "p";
                    disk_head[i].pos = (disk_head[i].pos % V) + 1;
                }
                disk_head[i].last_action = 1;
                disk_head[i].last_token = 1;
                remain_token -= pass_cost;
            }
            cnt = std::get<2>(*next_request);
            while (true) {
                int move_cost = (disk_head[i].last_action != 2) ? 64 : 
                    std::max(16, (int)ceil(disk_head[i].last_token * 0.8));
                int cur_object_id = request[-std::get<1>(*next_request)].object_id;
                // std::cerr << "[DEBUG]         read current_pos: [" << disk_head[i].pos << "](" << disk[i][disk_head[i].pos] << ") move_cost: " << move_cost << " remain_token: " << remain_token << std::endl;
                if (cnt == object[cur_object_id].size) {
                    // finished_request.push_back(-next_request->second);
                    request[-std::get<1>(*next_request)].is_done = true;
                    // std::cerr << "[DEBUG]         finished_request: " << -next_request->second << std::endl;
                    while (!object[cur_object_id].request_queue.empty()) {
                        int req = object[cur_object_id].request_queue.front();
                        if (req > -std::get<1>(*next_request)) break;
                        finished_request.push_back(req);
                        object[cur_object_id].request_queue.pop();
                        request[req].is_done = true;
                    }
                    disk_head[i].token_queue.erase(next_request);
                    break;
                }
                if (move_cost > remain_token) {
                    remain_token = -1;
                    break;
                }
                cnt++;
                movement[i] += "r";
                // std::cerr << "[DEBUG]         read current_pos: [" << disk_head[i].pos << "](" << disk[i][disk_head[i].pos] << ") move_cost: " << move_cost << " remain_token: " << remain_token << std::endl;
                disk_head[i].pos = (disk_head[i].pos % V) + 1;
                disk_head[i].last_action = 2;
                disk_head[i].last_token = move_cost;
                remain_token -= move_cost;
            }
        }
        if (remain_token == -1 && !disk_head[i].token_queue.empty() && next_request != disk_head[i].token_queue.end()) {
            disk_head[i].token_queue.erase(next_request);
            disk_head[i].token_queue.insert(std::make_tuple(disk_head[i].pos, -last_request_id, cnt));
        }
        // std::cerr << "[DEBUG] remain_token: " << remain_token << "/" << G << " (" << (double)remain_token/G*100 << "%)" << std::endl;
    }

    // std::cerr << "[DEBUG] movement: " << std::endl;
    for (int i = 1; i <= N; i++) {
        if (movement[i].empty() || movement[i][0] != 'j') movement[i] += "#\n";
        printf("%s", movement[i].c_str());
        // std::cerr << "[DEBUG] #" << i << ": " << movement[i];
    }
    

    int size = finished_request.size();
    printf("%d\n", size);
    // std::cerr << "[DEBUG] finished_request: " << std::endl;
    for (int i = 0; i < size; i++) {
        printf("%d\n", finished_request[i]);
        // std::cerr << "[DEBUG]         finished_request[" << i << "]: " << finished_request[i] << std::endl;
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