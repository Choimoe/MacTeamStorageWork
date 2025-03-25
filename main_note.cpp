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

#define MAX_DISK_NUM (10 + 1) // 最大磁盘数量
#define MAX_DISK_SIZE (16384 + 1) // 最大磁盘大小
#define MAX_REQUEST_NUM (30000000 + 1) // 最大请求数量
#define MAX_OBJECT_NUM (100000 + 1) // 最大对象数量
#define REP_NUM (3) // 每个对象的副本数量
#define FRE_PER_SLICING (1800) // 每个时间片的最大请求数
#define EXTRA_TIME (105) // 额外时间片
#define TAG_PHASE (48 + 1)
#define MAX_TAG_NUM (16 + 1)
#define MAX_OBJECT_SIZE (5 + 1) //最大对象大小
#define MAX_TOKEN_NUM (1000 + 1) //最大令牌数量

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
    int tag;
    std::queue<int> request_queue;
} Object;

// 新增磁头状态结构
typedef struct DiskHead_ {
    int pos;            // 当前磁头位置（存储单元编号）
    int last_action;    // 上一次动作类型：0-Jump,1-Pass,2-Read
    int last_token;     // 上一次消耗的令牌数
    std::multiset<std::tuple<int, int, int>> token_queue;
} DiskHead;

typedef struct HotTagAlloc_ {
    int disk[REP_NUM + 1];
    int start[REP_NUM + 1];
    int is_hot;
} HotTagAlloc;

HotTagAlloc hot_tag_alloc[MAX_TAG_NUM];

Request request[MAX_REQUEST_NUM];
Object object[MAX_OBJECT_NUM];

int T, M, N, V, G;
int disk[MAX_DISK_NUM][MAX_DISK_SIZE];
int disk_block_id[MAX_DISK_NUM][MAX_DISK_SIZE];
int disk_point[MAX_DISK_NUM];

DiskHead disk_head[MAX_DISK_NUM];
int timestamp;

int fre_del[MAX_TAG_NUM][TAG_PHASE]; // 每个标签的每个阶段删除的对象大小
int fre_write[MAX_TAG_NUM][TAG_PHASE]; // 每个标签的每个阶段写入的对象大小
int fre_read[MAX_TAG_NUM][TAG_PHASE]; // 每个标签的每个阶段读取的对象大小

int tag_alloc_length[MAX_TAG_NUM]; // 每个标签的分配长度

int disk_tag_num[MAX_DISK_NUM][MAX_TAG_NUM]; // 当前磁盘的标签个数
int disk_distribute[MAX_DISK_NUM][MAX_TAG_NUM]; // 当前磁盘标签分布
int disk_distribute_length[MAX_DISK_NUM][MAX_TAG_NUM]; // 当前磁盘标签分布每个占用的长度
int disk_tag_distinct_number[MAX_DISK_NUM]; // 当前磁盘标签数量
int disk_subhot_read_tag[MAX_DISK_NUM][TAG_PHASE]; // 当前磁盘当前阶段最热门的读取标签
int disk_subhot_delete_tag[MAX_DISK_NUM][TAG_PHASE]; // 当前磁盘当前阶段最热门的删除标签
int disk_end_point[MAX_DISK_NUM]; // 当前磁盘的结束位置
int disk_belong_tag[MAX_DISK_NUM][MAX_DISK_SIZE]; // 当前磁盘每个位置属于哪个标签

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

/**
 * 计算磁盘disk_id的最大连续空闲块长度
 * @param disk_id 磁盘编号
 * @return 最大连续空闲块长度
 */
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

/**
 * 选择三个不同磁盘（优先选择连续空间大的）
 * @param id 对象编号
 * @return 选择的磁盘编号列表 std::vector<int>
 */
std::vector<int> select_disks_for_object(int id) {
    std::vector<std::pair<int, int> > disk_scores;
    std::vector<int> vis(N + 1);
    // 遍历所有磁盘，计算得分（连续空间 >= size的磁盘才有资格）
    int tag = object[id].tag;
    for (int i = 1; i <= REP_NUM; i++) {
        int target_hot_disk = hot_tag_alloc[tag].disk[i];
        vis[target_hot_disk] = 1;
        int contiguous = calculate_max_contiguous(target_hot_disk);
        int fixed_score = V * N;
        if (disk_subhot_delete_tag[target_hot_disk][(timestamp - 1) / FRE_PER_SLICING + 1] == tag) {
            if (disk_distribute_length[target_hot_disk][tag] * 2 < tag_alloc_length[tag] / MAX_OBJECT_SIZE) {
                fixed_score = 0;
            }
        }
        if (contiguous >= object[id].size) {
            disk_scores.emplace_back(fixed_score + contiguous, target_hot_disk);
        } else {
            // std::cerr << "[ERROR] select_disks_for_object: disk_id: " << target_hot_disk << " contiguous: " << contiguous << " size: " << object[id].size << std::endl;
        }
    }
    for (int i = 1; i <= N; i++) {
        if (vis[i]) continue;
        int contiguous = calculate_max_contiguous(i);
        if (contiguous >= object[id].size) {
            disk_scores.emplace_back(contiguous, i);
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

/**
 * 在磁盘disk_id上分配size个连续块
 * @param disk_id 磁盘编号
 * @param size 对象大小
 * @param object_id 对象编号
 * @param reverse_blocks 是否翻转对象块
 * @return 返回分配的存储单元编号列表
 */
std::vector<int> allocate_contiguous_blocks(int disk_id, int size, int object_id, bool reverse_blocks) {
    int tag = object[object_id].tag;
    int start = -1, rep_id = 0;
    for (int i = 1; i <= REP_NUM; i++) {
        if (hot_tag_alloc[tag].disk[i] == disk_id) {
            start = hot_tag_alloc[tag].start[i];
            rep_id = i;
            break;
        }
    }
    if (start == -1) {
        start = disk_head[disk_id].pos;
        // std::cerr << "[ERROR] allocate_contiguous_blocks: disk_id: " << disk_id << " size: " << size << " object_id: " << object_id << "(" << object[object_id].tag << ")" << " reverse_blocks: " << reverse_blocks << " rep_id: " << rep_id << " start: " << start <<  std::endl;
        // for (int i = 1; i <= REP_NUM; i++) {
        //     std::cerr << "[ERROR]      rep #" << i << ": (" << hot_tag_alloc[tag].disk[i] << ")" << " start: " << hot_tag_alloc[tag].start[i] << std::endl;
        // }
    }

    /**
     * @brief 保存block到磁盘
     * @param pos block开始的位置
     * @return 保存的block序号
     */
    auto save_block = [&](int pos) {
        std::vector<int> blocks;
        for (int j = 0; j < size; j++) {
            int block_pos = (pos + j) % V;
            if (block_pos == 0) block_pos = V;
            blocks.push_back(block_pos);
            disk[disk_id][block_pos] = object_id; // 填充对象编号
            disk_block_id[disk_id][block_pos] = j + 1; //填充对象块编号
            disk_distribute_length[disk_id][disk_belong_tag[disk_id][block_pos]]--;
        }
        return blocks;
    };

    auto check_valid = [&](int pos) {
        for (int j = 0; j < size; j++) {
            int check_pos = (pos + j) % V;
            if (check_pos == 0) check_pos = V;
            if (disk[disk_id][check_pos] != 0) {
                return false;
            }
        }
        return true;
    };
        
    if (rep_id != 0) {
        for (int i = 0; i < tag_alloc_length[tag]; i++) {
            int pos = (start + i) % V;
            if (pos == 0) pos = V;
            if (disk[disk_id][pos] == 0) {
                if (check_valid(pos)) {
                    return save_block(pos);
                }
            }
        }
    }

    if (tag == disk_subhot_read_tag[disk_id][(timestamp - 1) / FRE_PER_SLICING + 1]) {
        start = disk_end_point[disk_id];
        for (int i = 0; i < tag_alloc_length[tag]; i++) {
            int pos = (start + i) % V;
            if (pos == 0) pos = V;
            if (disk[disk_id][pos] == 0) {
                if (check_valid(pos)) {
                    return save_block(pos);
                }
            }
        }
    } 
    
    start = 1;

    for (int i = 0; i < V; i++) {
        int pos = (start - i + V) % V;
        if (pos == 0) pos = V;
        if (disk[disk_id][pos] == 0) {
            if (check_valid(pos)) {
                return save_block(pos);
            }
        }
    }
    return {}; // 空间不足
}


/**
 * 写入对象的函数
 * @param object_unit 对象单元
 * @param disk_unit 磁盘单元
 * @param size 对象大小
 * @param object_id 对象编号
 */
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

/**
 * 写操作
 */
void write_action()
{
    int n_write; // 写请求数量
    scanf("%d", &n_write); // 读取写请求的数量
    for (int i = 1; i <= n_write; i++) {
        int id, size, tag;
        scanf("%d%d%d", &id, &size, &tag); // 读取对象 ID 和大小
        // std::cerr << "[DEBUG] " << " write_action: " << id << " size: " << size << " tag: " << tag << std::endl;
        object[id].tag = tag; // 设置对象标签
        object[id].last_request_point = 0; // 初始化对象的最后请求指针
        object[id].size = size; // 设置对象的大小
        object[id].is_delete = false; // 标记对象为未删除
        std::vector<int> selected_disks = select_disks_for_object(id);
        for (int j = 1; j <= REP_NUM; j++) {
            int disk_id = selected_disks[j - 1];
            std::vector<int> blocks = allocate_contiguous_blocks(disk_id, size, id, j & 1); //奇数翻转，偶数不变
            object[id].replica[j] = disk_id; // 计算副本的 ID
            object[id].unit[j] = static_cast<int*>(malloc(sizeof(int) * (size + 1))); // 分配内存以存储对象数据
            for (int _ = 0; _ < size; _++) {
                object[id].unit[j][_+1] = blocks[_];
            }
            disk_tag_num[disk_id][object[id].tag]++;
            // do_object_write(object[id].unit[j], disk[object[id].replica[j]], size, id); // 将对象数据写入磁盘
        }

        printf("%d\n", id); // 打印对象 ID
        for (int j = 1; j <= REP_NUM; j++) {
            printf("%d", object[id].replica[j]); // 打印副本 ID
            // std::cerr << "[DEBUG] replica" <<j<< " in disk: " << object[id].replica[j] << " ";
            for (int k = 1; k <= size; k++) {
                printf(" %d", object[id].unit[j][k]); // 打印对象数据
                // std::cerr << "block" << k << ":" << object[id].unit[j][k] << " ";
            }
            // std::cerr << std::endl;
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

/**
 * 预处理标签
 */
void preprocess_tag() {
    // 读取每个标签的删除请求数量
    for (int i = 1; i <= M; i++) {
        for (int j = 1; j <= (T - 1) / FRE_PER_SLICING + 1; j++) {
            scanf("%d", &fre_del[i][j]);
            fre_del[i][0] += fre_del[i][j];
        }
    }

    // 读取每个标签的写请求数量
    for (int i = 1; i <= M; i++) {
        for (int j = 1; j <= (T - 1) / FRE_PER_SLICING + 1; j++) {
            scanf("%d", &fre_write[i][j]);
            fre_write[i][0] += fre_write[i][j];
        }
    }

    // 读取每个标签的读请求数量
    for (int i = 1; i <= M; i++) {
        for (int j = 1; j <= (T - 1) / FRE_PER_SLICING + 1; j++) {
            scanf("%d", &fre_read[i][j]);
            fre_read[i][0] += fre_read[i][j];
        }
    }

    std::vector<int> tag_id;
    for (int i = 1; i <= M; i++) {
        tag_id.push_back(i);
    }

    std::sort(tag_id.begin(), tag_id.end(), [](int a, int b) {
        return fre_write[a][0] * 2 + fre_read[a][0] * 3 + fre_del[a][0] > fre_write[b][0] * 2 + fre_read[b][0] * 3 + fre_del[b][0];
    });

    std::vector<int> hot_tag;

    for (int i = 0; i < N / REP_NUM; i++) {
        hot_tag.push_back(tag_id[i]);
        hot_tag_alloc[tag_id[i]].is_hot = 1;
        tag_id.erase(tag_id.begin() + i);
    }

    std::sort(tag_id.begin(), tag_id.end(), [](int a, int b) {
        return fre_write[a][0] - fre_del[a][0] > fre_write[b][0] - fre_del[b][0];
    });

    std::vector<int> start_point(N + 1);
    for (int i = 1; i <= N; i++) {
        start_point[i] = 1;
    }

    /**
     * 设置标签信息
     * @param tag 标签编号
     * @param disk_id 磁盘编号
     * @param rep_id 副本编号
     * @param size 标签大小
     */
    auto set_tag_info = [&](int tag, int disk_id, int rep_id, int size) {
        hot_tag_alloc[tag].disk[rep_id] = disk_id;
        hot_tag_alloc[tag].start[rep_id] = start_point[disk_id];
        tag_alloc_length[tag] = size;
        disk_distribute_length[disk_id][tag] = size;
        start_point[disk_id] = (start_point[disk_id] + size + V - 1) % V + 1;
        for (int i = 0; i < size; i++) {
            disk_belong_tag[disk_id][(start_point[disk_id] + i - 1) % V + 1] = tag;
        }
    };

    int disk_id = 1;
    for (auto tag : hot_tag) {
        for (int i = 1; i <= REP_NUM; i++) {
            set_tag_info(tag, disk_id, i, (fre_write[tag][0] - fre_del[tag][0]) * 1.1);
            disk_id = disk_id % N + 1;
        }
    }

    std::priority_queue<std::pair<int,int> > current_space;
    for (int i = 1; i <= N; i++)
        current_space.emplace(V - start_point[i] + 1, i);

    for (auto i : tag_id) { 
        int size = fre_write[i][0] - fre_del[i][0];
        std::vector<std::pair<int, int> > selected_disk(REP_NUM + 1);
        for (int j = 1; j <= REP_NUM; j++) {
            auto it = current_space.top();
            selected_disk[j] = std::make_pair(it.first, it.second);
            current_space.pop();
        }
        for (int j = 1; j <= REP_NUM; j++) {
            current_space.emplace(selected_disk[j].first - size, selected_disk[j].second);
            set_tag_info(i, selected_disk[j].second, j, size);
        }
    }

    for (int i = 1; i <= N; i++) {
        disk_end_point[i] = start_point[i];
    }

    std::vector<std::vector<std::pair<int, int> > > disk_distribute_vector(N + 1, std::vector<std::pair<int, int> >());

    for (auto i : tag_id) {
    //    std::cerr << "[DEBUG] tag: " << i << " is_hot: " << hot_tag_alloc[i].is_hot << std::endl;
        for (int j = 1; j <= REP_NUM; j++) {
        //    std::cerr << "[DEBUG]      rep #" << j << ": disk: " << hot_tag_alloc[i].disk[j] << " start: " << hot_tag_alloc[i].start[j] << std::endl;
            disk_distribute_vector[hot_tag_alloc[i].disk[j]].emplace_back(hot_tag_alloc[i].start[j], i);
        }
    }

    // std::cerr << "[DEBUG] N = " << N << ", V = " << V << std::endl;

    for (int i = 1; i <= N; i++) std::sort(disk_distribute_vector[i].begin(), disk_distribute_vector[i].end());

    for (int i = 1; i <= N; i++) {
        // std::cerr << "[DEBUG] disk: " << i << ":" << std::endl;
        int cnt = 0;
        for (auto [fi, se]: disk_distribute_vector[i]) {
            disk_distribute[i][++cnt] = se;
            // std::cerr << "[DEBUG]      start: " << fi << "(" << se << ")" << std::endl;
        }
        disk_tag_distinct_number[i] = cnt;
    }

    for (int i = 1; i <= N; i++) {
        for (int j = 1; j <= (T - 1) / FRE_PER_SLICING + 1; j++) {
            int hot_read_tag_size = 0, hot_read_tag_id = 0;
            int hot_delete_tag_size = 0, hot_delete_tag_id = 0;
            int tag_cnt = disk_tag_distinct_number[i];
            for (int k = 1; k <= tag_cnt; k++) {
                if (disk_distribute[i][k] == 0) continue;
                if (fre_read[disk_distribute[i][k]][j] > hot_read_tag_size) {
                    hot_read_tag_size = fre_read[disk_distribute[i][k]][j];
                    hot_read_tag_id = disk_distribute[i][k];
                }
                if (fre_del[disk_distribute[i][k]][j] > hot_delete_tag_size) {  
                    hot_delete_tag_size = fre_del[disk_distribute[i][k]][j];
                    hot_delete_tag_id = disk_distribute[i][k];
                }
            }
            disk_subhot_read_tag[i][j] = hot_read_tag_id;
            disk_subhot_delete_tag[i][j] = hot_delete_tag_id;
        }
    }
}

/**
 * 基于这个思路实现https://rocky-robin-46d.notion.site/1bb3b75a16b7803d8457c86b01881322?pvs=4
 */
int main()
{
    freopen("log.txt", "w", stderr); // 将调试输出重定向到 log.txt

    scanf("%d%d%d%d%d", &T, &M, &N, &V, &G); // 读取参数
    for (int i = 1; i <= N; i++) { // 初始化磁头位置和当前阶段
        disk_head[i].pos = 1; 
    }

    preprocess_tag();

    printf("OK\n"); // 输出 OK
    fflush(stdout); // 刷新输出缓冲区

    // 主循环，处理时间片
    for (int t = 1; t <= T + EXTRA_TIME; t++) {
//        std::cerr << "[DEBUG] " << "------- t: " << t <<"-------"<< std::endl;
        timestamp_action(); // 处理时间戳
        delete_action(); // 处理删除请求
        write_action(); // 处理写请求
        read_action(); // 处理读请求
    }
    clean(); // 清理资源

    return 0; // 返回 0，表示程序正常结束
}