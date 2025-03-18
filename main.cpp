#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <queue>
#include <stack>
#include <string>
#include <unordered_map>
#include <vector>

#define MAX_DISK_NUM (10 + 1)
#define MAX_DISK_SIZE (16384 + 1)
#define MAX_REQUEST_NUM (30000000 + 1)
#define MAX_OBJECT_NUM (100000 + 1)
#define REP_NUM (3)
#define FRE_PER_SLICING (1800)
#define EXTRA_TIME (105)
#define TAG_PHASE (48 + 1)
#define MAX_TAG_NUM (16 + 1)

// 请求结构体，包含对象ID、前驱请求ID、完成状态和时间戳
typedef struct Request_ {
    int object_id;
    int prev_id;
    bool is_done;
    int time;
} Request;

// 对象结构体，管理副本信息、存储位置、请求状态等
typedef struct Object_ {
    int replica[REP_NUM + 1];
    int *unit[REP_NUM + 1];
    int size;
    int last_request_point;
    bool is_delete;
    int tag;

    std::deque<int> active_phases;
    int current_phase;
    bool is_request;
    int disk_id;
    int process_request;
} Object;

// 磁头状态结构，记录位置、操作历史和当前处理对象
typedef struct DiskHead_ {
    int pos;
    int last_action;
    int last_token;

    int current_object;
} DiskHead;

// 全局变量
Request request[MAX_REQUEST_NUM];
Object object[MAX_OBJECT_NUM];

int T, M, N, V, G;
int disk[MAX_DISK_NUM][MAX_DISK_SIZE];
int disk_point[MAX_DISK_NUM];
int timestamp;

DiskHead disk_head[MAX_DISK_NUM];
std::priority_queue<std::pair<int, int>> disk_requests[MAX_DISK_NUM];

int fre_del[MAX_TAG_NUM][TAG_PHASE];
int fre_write[MAX_TAG_NUM][TAG_PHASE];
int fre_read[MAX_TAG_NUM][TAG_PHASE];

int disk_tag_num[MAX_DISK_NUM][MAX_TAG_NUM];

// 处理时间戳更新操作
void timestamp_action() {
    int cur_time;

    scanf("%*s%d", &cur_time);
    printf("TIMESTAMP %d\n", cur_time);
    timestamp = cur_time;
    fflush(stdout);
}

// 执行对象删除操作，清空磁盘单元
void do_object_delete(const int *object_unit, int *disk_unit, int size) {
    for (int i = 1; i <= size; i++) {
        disk_unit[object_unit[i]] = 0;
    }
}

// 处理删除请求，标记已删除对象并释放空间
void delete_action() {
    int n_delete;
    int abort_num = 0;
    static int _id[MAX_OBJECT_NUM];

    scanf("%d", &n_delete);
    for (int i = 1; i <= n_delete; i++) {
        scanf("%d", &_id[i]);
    }

    // 检查每个请求是否有未完成的请求
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
        // 删除对象的副本
        for (int j = 1; j <= REP_NUM; j++) {
            do_object_delete(object[id].unit[j], disk[object[id].replica[j]],
                             object[id].size);
            disk_tag_num[object[id].replica[j]][object[id].tag]--;
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
        while (disk[disk_id][head] == 0 && head <= V)
            head++;
        while (disk[disk_id][tail] == 0 && tail >= 1)
            tail--;
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
    std::vector<std::pair<int, int>> disk_scores;
    // 遍历所有磁盘，计算得分（连续空间 >= size的磁盘才有资格）
    for (int i = 1; i <= N; i++) {
        int contiguous = calculate_max_contiguous(i);
        int max_space = calculate_max_space(i);

        if (contiguous >= object[id].size) {
            disk_scores.emplace_back(contiguous, i);
        }
    }
    // 按连续空间降序排序
    std::sort(disk_scores.rbegin(), disk_scores.rend());

    std::vector<int> selected;
    for (auto &[score, disk_id] : disk_scores) {
        if (selected.size() >= 3)
            break;
        if (std::find(selected.begin(), selected.end(), disk_id) ==
            selected.end()) {
            selected.push_back(disk_id);
        }
    }
    return selected;
}

// 在磁盘disk_id上分配size个连续块（返回分配的存储单元编号列表）
std::vector<int> allocate_contiguous_blocks(int disk_id, int size,
                                            int object_id) {
    // 从磁头当前位置开始搜索（减少未来读取时的移动距离）
    int start = disk_head[disk_id].pos;
    for (int i = 0; i < V; i++) {
        int pos = (start + i) % V;
        if (pos == 0)
            pos = V;
        if (disk[disk_id][pos] == 0) {
            bool found = true;
            std::vector<int> blocks;
            // 检查后续size个单元是否都空闲
            for (int j = 0; j < size; j++) {
                int check_pos = (pos + j) % V;
                if (check_pos == 0)
                    check_pos = V;
                if (disk[disk_id][check_pos] != 0) {
                    found = false;
                    break;
                }
            }
            if (found) {
                for (int j = 0; j < size; j++) {
                    int block_pos = (pos + j) % V;
                    if (block_pos == 0)
                        block_pos = V;
                    blocks.push_back(block_pos);
                    disk[disk_id][block_pos] = object_id;
                }
                return blocks;
            }
        }
    }
    return {};
}

// 写入对象的函数
void do_object_write(int *object_unit, int *disk_unit, int size,
                     int object_id) {
    int current_write_point = 0;
    for (int i = 1; i <= V; i++) {
        if (disk_unit[i] == 0) { // 如果磁盘单元为空
            disk_unit[i] = object_id;
            object_unit[++current_write_point] = i;
            if (current_write_point == size) {
                break;
            }
        }
    }

    assert(current_write_point == size);
}

// 写操作
void write_action() {
    int n_write;
    scanf("%d", &n_write);
    for (int i = 1; i <= n_write; i++) {
        int id, size, tag;
        scanf("%d%d%d", &id, &size, &tag);

        object[id].tag = tag;
        object[id].last_request_point = 0;
        object[id].size = size;
        object[id].is_delete = false;
        object[id].disk_id = -1;
        object[id].is_request = false;
        std::vector<int> selected_disks = select_disks_for_object(id);
        for (int j = 1; j <= REP_NUM; j++) {
            int disk_id = selected_disks[j - 1];
            std::vector<int> blocks =
                allocate_contiguous_blocks(disk_id, size, id);
            object[id].replica[j] = disk_id;
            object[id].unit[j] =
                static_cast<int *>(malloc(sizeof(int) * (size + 1)));
            for (int _ = 0; _ < size; _++) {
                object[id].unit[j][_ + 1] = blocks[_];
            }
            disk_tag_num[disk_id][object[id].tag]++;
            // do_object_write(object[id].unit[j], disk[object[id].replica[j]],
            // size, id);
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
int calculate_compactness(const int *blocks, int size) {
    if (size <= 1)
        return 0;

    int sorted[MAX_DISK_SIZE];
    memcpy(sorted, blocks + 1, sizeof(int) * size);
    std::sort(sorted, sorted + size);

    int score = 0, lst = 64, pass = 0;
    for (int i = 1; i < size; i++) {
        lst = (pass ? 64 : std::max(16, (int)ceil(lst * 0.8)));
        pass = sorted[i] - sorted[i - 1] - 1;
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
int evaluate_replica(int rep_id, const Object *obj, int current_time) {
    const int disk_id = obj->replica[rep_id];
    const int head_pos = disk_head[disk_id].pos;
    const int *blocks = obj->unit[rep_id];

    // 紧凑性评分
    int compactness = G - std::min(G, calculate_compactness(blocks, obj->size));

    // 移动成本评分
    int move_cost = G - std::min(G, calculate_move_cost(head_pos, blocks[1]));

    // 时间局部性评分（需预存标签访问模式）
    // 此处需要接入预处理数据，暂用固定值
    int time_score = 0;

    return compactness * 1 + move_cost * 1 + time_score * 1;
}

// 选择最佳副本
int select_best_replica(int object_id) {
    const Object *obj = &object[object_id];
    int best_rep = -1;
    int max_score = -1;

    for (int rep = 1; rep <= REP_NUM; rep++) {
        int score = evaluate_replica(rep, obj, 0);
        if (score > max_score) { // 如果评分更高
            max_score = score;
            best_rep = rep;
        }
    }

    return best_rep;
}

int select_best_replica_available(int object_id, bool *available_disks) {
    const Object *obj = &object[object_id];
    int best_rep = -1;
    int max_score = -1;
    for (int rep = 1; rep <= REP_NUM; rep++) {
        int disk_id = obj->replica[rep];
        if (available_disks[disk_id]) {
            int score = evaluate_replica(rep, obj, 0);
            if (score > max_score) { // 如果评分更高
                max_score = score;
                best_rep = rep;
            }
        }
    }

    return best_rep;
}

// 执行对象读取操作，处理磁头移动和IO请求
void do_object_read(int object_id, int target_disk,
                    std::string &head_movement) {
    int is_read = 0;
    int &current_phase = object[object_id].current_phase;
    int best_rep = -1;
    for (int i = 1; i <= REP_NUM; i++) {
        if (object[object_id].replica[i] == target_disk) {
            best_rep = i;
        }
    }
    for (int i = 1; i <= N; i++) {
        if (i == target_disk) { // 如果是目标磁盘
            int target_pos =
                object[object_id].unit[best_rep][current_phase + 1];

            if (target_pos == 0) {
                std::cerr << "[Error] " << " target_pos: " << target_pos
                          << " object_id: " << object_id
                          << " object->size: " << object[object_id].size
                          << " current_phase: " << current_phase
                          << " target_disk: " << target_disk << std::endl;
            }
            int remain_token = G;
            if (target_pos !=
                disk_head[target_disk].pos) { // 如果目标位置与当前磁头位置不同
                int pass_cost =
                    calculate_move_cost(disk_head[target_disk].pos, target_pos);
                if (pass_cost > G) { // 如果移动成本大于剩余令牌
                    remain_token -= G;
                    disk_head[target_disk].pos = target_pos;
                    disk_head[target_disk].last_action = 0;
                    disk_head[target_disk].last_token = G;
                    // printf("j %d\n", target_pos);
                    head_movement += "j " + std::to_string(target_pos) + "\n";
                    continue;
                } else {
                    remain_token -= pass_cost;
                    for (int i = 1; i <= pass_cost; i++) {
                        // printf("p");

                        head_movement += "p";
                    }
                    disk_head[target_disk].pos = target_pos;
                    disk_head[target_disk].last_action = 1;
                    disk_head[target_disk].last_token = 1;
                }
            }
            while (true) {

                int move_cost =
                    (disk_head[target_disk].last_action != 2)
                        ? 64
                        : std::max(16,
                                   (int)ceil(disk_head[target_disk].last_token *
                                             0.8));
                target_pos =
                    object[object_id].unit[best_rep][current_phase + 1];
                if (move_cost > remain_token ||
                    current_phase == object[object_id].size ||
                    target_pos != disk_head[target_disk].pos) {
                    if (target_pos != disk_head[target_disk].pos) {
                    }
                    // printf("#\n");
                    head_movement += "#\n";

                    break;
                }
                current_phase++;
                // printf("r");
                head_movement += "r";
                disk_head[target_disk].pos =
                    (disk_head[target_disk].pos % V) + 1;
                disk_head[target_disk].last_action = 2;
                disk_head[target_disk].last_token = move_cost;
                remain_token -= move_cost;
            }
        } else {
            // printf("#\n");
        }
    }
}

void reset_disk_head(int disk_id) // 重置磁头，等待下一个任务
{
    disk_head[disk_id].current_object = -1;
}

bool check_disk_head(int disk_id) // 检查当前盘是否空闲
{
    if (disk_head[disk_id].current_object == -1) {
        return true;
    }
    return false;
}

int evaluate_request(int object_id) {
    return timestamp * 105 +
           object[object_id].active_phases.size() * object[object_id].size;
}

// 读取操作
/*
Origin:
直接取出最近的一个请求。
然后找到一个最佳的副本，进行输出。
Update:
1. 给空闲的磁盘分配任务
2. 每个磁盘进行工作
UpUpdate:
1. 将Object拆分
2. 尽量减少jump
3. current_phase 绑定到Object上
4. 尽量减少移动
5. 分配Object而不是分配request，request仅仅用作输出
*/
void read_action() {
    int n_read;
    int request_id = -1, object_id;
    scanf("%d", &n_read);
    static std::priority_queue<std::pair<int, int>> new_requests;
    for (int i = 1; i <= n_read; i++) {
        scanf("%d%d", &request_id, &object_id);
        request[request_id].object_id = object_id;
        request[request_id].prev_id = object[object_id].last_request_point;
        request[request_id].is_done = false;
        request[request_id].time = timestamp;

        object[object_id].last_request_point = request_id;
        object[object_id].active_phases.push_back(request_id);
        object[object_id].is_request = true;

        new_requests.push(
            std::make_pair(evaluate_request(object_id), object_id));
        for (int rep = 1; rep <= REP_NUM; rep++) {
            int disk_id = object[object_id].replica[rep];
            disk_requests[disk_id].push(
                std::make_pair(evaluate_request(object_id), object_id));
        }
    }
    std::cerr << "[DEBUG] " << " n_read: " << n_read << std::endl;
    // if (!n_read)  // 如果没有当前请求
    // {
    //     for (int i = 1; i <= N; i++) {
    //         printf("#\n");
    //
    //     }
    //     printf("0\n");
    //
    //     fflush(stdout);
    //     return;
    // }

    // 移除已完成的请求
    // while(!new_requests.empty() && request[new_requests.top()].is_done ==
    // true) new_requests.pop();

    std::vector<int> finished_requests;
    std::string head_movement[MAX_DISK_NUM];

    bool available_disks[MAX_DISK_NUM]; // 储存可用磁头的数组
    for (int i = 1; i <= N; i++) {
        available_disks[i] = check_disk_head(i);
    }

    while (
        !new_requests
             .empty()) // 分配请求到磁盘，直到磁盘上都有任务，但可能存在有的磁盘未被使用。
    {
        int current_object = 0;
        int rank_value = new_requests.top().first;
        current_object = new_requests.top().second;
        new_requests.pop();

        if (!object[current_object].is_request)
            continue;
        if (object[current_object].disk_id != -1)
            continue;

        int best_rep =
            select_best_replica_available(current_object, available_disks);
        if (best_rep == -1) // 如果没有可用的磁盘，就退出。
        {
            new_requests.push(std::make_pair(rank_value, current_object));
            break;
        }
        int target_disk = object[current_object].replica[best_rep];
        object[current_object].disk_id = target_disk;
        object[current_object].process_request =
            object[current_object].active_phases.back();

        disk_head[target_disk].current_object = current_object;
        available_disks[target_disk] = false;
    }
    for (int i = 1; i <= N; i++) {
        if (available_disks[i]) {

            while (!disk_requests[i].empty()) {

                int current_object = disk_requests[i].top().second;
                disk_requests[i].pop();
                if (!object[current_object].is_request)
                    continue;
                if (object[current_object].disk_id != -1)
                    continue;
                disk_head[i].current_object = current_object;
                object[current_object].disk_id = i;
                object[current_object].process_request =
                    object[current_object].active_phases.back();
                available_disks[i] = false;

                break;
            }
        }
    }
    int not_work_disk = 0;
    for (int i = 1; i <= N; i++) {
        if (available_disks[i]) {
            not_work_disk++;
        }
    }
    std::cerr << "[DEBUG] " << " request_num: " << new_requests.size()
              << std::endl;
    std::cerr << "[DEBUG] " << " not_work_disk: " << not_work_disk << std::endl;

    for (int i = 1; i <= N; i++) {
        if (available_disks[i])
            continue;        // 说明磁盘空闲，不进行读取
        int target_disk = i; // 获取当前磁盘
        int object_id = disk_head[target_disk].current_object;

        do_object_read(object_id, target_disk, head_movement[target_disk]);
        if (object[object_id].current_phase !=
            object[object_id].size) { // 如果当前阶段未达到对象大小

            continue;
        }

        if (object[object_id].is_delete) { // 如果对象被删除

            reset_disk_head(target_disk);
            continue;
        }

        auto *active_phases = &object[object_id].active_phases;

        while (!active_phases->empty() &&
               active_phases->front() <=
                   object[object_id].process_request) { // 移除已完成的请求
            finished_requests.push_back(active_phases->front());
            request[active_phases->front()].is_done = true;

            object[object_id].active_phases.pop_front();
        }

        if (object[object_id].active_phases.empty()) {
            object[object_id].is_request = false;
            object[object_id].disk_id = -1;
            object[object_id].current_phase = 0;
        } else {

            new_requests.push(
                std::make_pair(evaluate_request(object_id), object_id));
            object[object_id].is_request = true;
            object[object_id].disk_id = -1;
            object[object_id].current_phase = 0;

            for (int rep = 1; rep <= REP_NUM; rep++) {
                int disk_id = object[object_id].replica[rep];
                disk_requests[disk_id].push(
                    std::make_pair(evaluate_request(object_id), object_id));
            }
        }

        reset_disk_head(target_disk);
    }
    for (int i = 1; i <= N; i++) {
        if (head_movement[i].empty()) {
            head_movement[i] = "#\n";
        }
    }

    for (int i = 1; i <= N; i++) {
        printf("%s", head_movement[i].c_str());
    }
    int fsize = finished_requests.size();

    printf("%d\n", fsize);

    for (int i = 0; i < fsize; i++) {
        std::cerr << "[DEBUG] " << " finished_requests[" << i
                  << "]: " << finished_requests[i] << std::endl;
        printf("%d\n", finished_requests[i]);
    }
    fflush(stdout);
}

// 清理函数，释放动态分配的内存
void clean() {
    for (auto &obj : object) {
        for (int i = 1; i <= REP_NUM; i++) {
            if (obj.unit[i] == nullptr) // 如果指针为空，跳过
                continue;
            free(obj.unit[i]);
            obj.unit[i] = nullptr;
        }
    }
}

// 主函数
int main() {
    freopen("log.txt", "w", stderr);

    scanf("%d%d%d%d%d", &T, &M, &N, &V, &G);
    std::cerr << "[DEBUG] " << " T: " << T << " M: " << M << " N: " << N
              << " V: " << V << " G: " << G << std::endl;
    for (int i = 1; i <= N; i++) { // 初始化磁头位置和当前阶段
        disk_head[i].pos = 1;
    }

    // 读取每个标签的删除请求数量
    for (int i = 1; i <= M; i++) {
        for (int j = 1; j <= (T - 1) / FRE_PER_SLICING + 1; j++) {
            scanf("%d", &fre_del[i][j]);
        }
    }

    // 读取每个标签的写请求数量
    for (int i = 1; i <= M; i++) {
        for (int j = 1; j <= (T - 1) / FRE_PER_SLICING + 1; j++) {
            scanf("%d", &fre_write[i][j]);
        }
    }

    // 读取每个标签的读请求数量
    for (int i = 1; i <= M; i++) {
        for (int j = 1; j <= (T - 1) / FRE_PER_SLICING + 1; j++) {
            scanf("%d", &fre_read[i][j]);
        }
    }

    printf("OK\n");
    fflush(stdout);

    for (int i = 1; i <= N; i++) {
        disk_point[i] = 1;
        reset_disk_head(i);
    }

    // 主循环，处理时间片
    for (int t = 1; t <= T + EXTRA_TIME; t++) {
        std::cerr << "[DEBUG] " << "------- t: " << t << "-------" << std::endl;
        timestamp_action();
        delete_action();
        write_action();
        read_action();
    }
    clean();

    return 0;
}