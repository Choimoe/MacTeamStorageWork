#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <queue>
#include <set>
#include <stack>
#include <string>
#include <vector>
#include <random>

#define MAX_DISK_NUM (10 + 1)          // 最大磁盘数量
#define MAX_DISK_SIZE (16384 + 1)      // 最大磁盘大小
#define MAX_REQUEST_NUM (30000000 + 1) // 最大请求数量
#define MAX_OBJECT_NUM (100000 + 1)    // 最大对象数量
#define REP_NUM (3)                    // 每个对象的副本数量
#define FRE_PER_SLICING (1800)         // 每个时间片的最大请求数
#define EXTRA_TIME (105)               // 额外时间片
#define TAG_PHASE (48 + 1)
#define MAX_TAG_NUM (16 + 1)
#define MAX_OBJECT_SIZE (5 + 1)  // 最大对象大小
#define MAX_TOKEN_NUM (1000 + 1) // 最大令牌数量
#define OUT_DATE_TIME (105)
#define MAX_DISK_HEAD_NUM (2)

const int cost[] = {0,  64, 52, 42, 34,
                    28, 23, 19, 16}; // 从0开始连续Read操作的代价序列

/**
 * @brief 请求结构体
 * @param object_id 对象 ID
 * @param prev_id 前一个请求 ID
 * @param is_done 请求是否完成
 * @param time 请求时间
 */
typedef struct Request_ {
    int object_id;
    int prev_id;
    bool is_done;
    bool is_timeout;
    int time;
} Request;

/**
 * @brief 对象结构体
 * @param replica 副本 ID 数组
 * @param unit 存储对象数据的指针数组
 * @param size 对象大小
 * @param last_request_point 最后一个请求的指针
 * @param is_delete 对象是否被标记为删除
 * @param tag 对象标签
 * @param cnt_request 对象请求计数
 * @param last_finish_time 最近一次请求完成时间
 */
typedef struct Object_ {
    int replica[REP_NUM + 1]{};
    int *unit[REP_NUM + 1]{};
    int size{};
    int last_request_point{};
    bool is_delete{};
    int tag{};
    int cnt_request{};
    int last_finish_time{};

    std::deque<int> active_phases;
    std::queue<int> deleted_phases; //被认为删除掉的请求
} Object;

/**
 * @brief 磁头状态结构
 * @param pos 当前磁头位置（存储单元编号）
 * @param last_action 上一次动作类型：0-Jump, 1-Pass, 2-Read
 * @param last_token 上一次消耗的令牌数在cost中的下标
 */
typedef struct DiskHead_ {
    int pos;
    int last_action;
    int last_token;
} DiskHead;

// 全局变量
Request request[MAX_REQUEST_NUM]; // 请求数组
std::queue<int> global_requestions; // 全局的请求队列，按照时间戳天然不降序
Object object[MAX_OBJECT_NUM];    // 对象数组
int total_request_num = 0;
int total_object_num = 0;

int T, M, N, V, G, K; // 时间片、对象标签、硬盘数量、存储单元、令牌数量
int disk_obj_id[MAX_DISK_NUM][MAX_DISK_SIZE];   // 磁盘上存储的obj的id
int disk_block_id[MAX_DISK_NUM][MAX_DISK_SIZE]; // 磁盘上存储的obj的block的编号
int timestamp;                                  // 当前时间戳
int time_vis[MAX_OBJECT_NUM]
[MAX_OBJECT_SIZE]; // 表示每个对象块最后一次被read的时间

DiskHead disk_head[MAX_DISK_NUM][MAX_DISK_HEAD_NUM + 1]; // 磁头状态数组
std::priority_queue<std::pair<int, int>>
        disk_requests[MAX_DISK_NUM]; // 存储新请求的栈

int fre_del[MAX_TAG_NUM][TAG_PHASE];   // 每个标签的每个阶段删除的对象大小
int fre_write[MAX_TAG_NUM][TAG_PHASE]; // 每个标签的每个阶段写入的对象大小
int fre_read[MAX_TAG_NUM][TAG_PHASE];  // 每个标签的每个阶段读取的对象大小

int tag_alloc_length[MAX_TAG_NUM]; // 每个标签的分配长度

typedef struct DiskInfo_ {
    int tag_num[MAX_TAG_NUM];           // 每个标签的对象数量
    int distribute[MAX_TAG_NUM];        // 每个标签的分配长度
    int distribute_length[MAX_TAG_NUM]; // 每个标签的分配长度
    int tag_distinct_number;            // 每个标签的distinct数量
    int subhot_read_tag[TAG_PHASE];     // 每个时间片的读取标签
    int subhot_delete_tag[TAG_PHASE];   // 每个时间片的删除标签
    int end_point;                      // 每个磁盘的结束点
    int disk_belong_tag[MAX_DISK_SIZE]; // 每个存储单元的标签
    int cnt_request;                    // 每个磁盘的请求数量
    int valuable_block_num;             // 每个磁盘的有价值的块数量

    std::set<std::pair<int, int>>
            required; // 存储磁盘每个位置的对象块对应的对象仍有多少查询未完成，只保留第二维非0的元素。
} DiskInfo;

DiskInfo di[MAX_DISK_NUM];

typedef struct HotTagAlloc_ {
    int disk[REP_NUM + 1];
    int start[REP_NUM + 1];
    int is_hot;

    int remain_alloc_num;
} HotTagAlloc;

HotTagAlloc hot_tag_alloc[MAX_TAG_NUM];

std::queue<int> timeout_request;

/**
 * 计算从x走到y需要的时间
 * @param x 起始位置
 * @param y 终止位置
 * @return 需要移动的距离
 */
inline int get_distance(int x, int y) {
    if (x <= y) {
        return y - x;
    } else {
        return V - x + y;
    }
}

/**
 * 时间戳操作
 */
void timestamp_action() {
    int cur_time;

    scanf("%*s%d", &cur_time);          // 读取当前时间戳
    printf("TIMESTAMP %d\n", cur_time); // 输出时间戳
    timestamp = cur_time;               // 更新全局时间戳
    fflush(stdout);                     // 刷新输出缓冲区
}

/**
 * 计算请求的时间得分 f(x)
 * @param request_id 请求编号
 * @return double 得分
 */
double calculate_request_time_score(int request_id) {
    double x = timestamp - request[request_id].time;
    if (x <= 10) return 1 - 0.005 * x;
    if (x <= 105) return 1.05 - 0.01 * x;
    return 0;
}

/**
 * 计算请求的大小得分 g(size)
 * @param request_id 请求编号
 * @return double 得分
 */
double calculate_request_size_score(int request_id) {
    int object_id = request[request_id].object_id;
    int size = object[object_id].size;
    return (size + 1) * 0.5;
}

/**
 * 计算请求的得分 SCORES = f(x) * g(size)
 * @param request_id 请求编号
 * @return double 得分
 */
double calculate_request_score(int request_id) {
    return calculate_request_time_score(request_id) * calculate_request_size_score(request_id);
}

/**
 * 删除对象在disk_id这块磁盘上的数据（obj_id 与 block_id）。
 * @param object_unit 对象的存储单元下标
 * @param disk_id 磁盘编号
 * @param size 对象大小
 */
void do_object_delete(const int *object_unit, const int disk_id, int size) {
    for (int i = 1; i <= size; i++) {
        disk_obj_id[disk_id][object_unit[i]] = 0;   // 清空磁盘obj_id
        disk_block_id[disk_id][object_unit[i]] = 0; // 清空磁盘 block_id
        
    }
}

/**
 * 更新object_id_set中所有对象的磁盘set，需要支持cnt_request增加、减小。
 * @param object_id_set 记录需要修改的object的id的集合（使用set自动去重）
 */
void update_disk_cnt(const std::set<int> &object_id_set) {
    for (int object_id : object_id_set) {
        for (int rep = 1; rep <= REP_NUM; rep++) {
            int disk_id = object[object_id].replica[rep];
            for (int i = 1; i <= object[object_id].size; i++) {
                int index = object[object_id].unit[rep][i];
                auto p =
                        di[disk_id].required.lower_bound(std::make_pair(index, 0));

                if (p != di[disk_id].required.end() &&
                    p->first == index) { // 删除原来的
                    di[disk_id].required.erase(p);
                }

                if (object[object_id].cnt_request > 0) // 增加新的
                    di[disk_id].required.insert(
                            std::make_pair(index, object[object_id].cnt_request));
            }
        }
    }
}

/**
 * 更新object_id_set中所有对象的磁盘set，需要支持cnt_request增加、减小。
 * @param object_id_set 记录需要修改的object的id的集合（使用set自动去重）
 */
void reset_disk_cnt(const std::set<int> &object_id_set) {
    for (int object_id : object_id_set) {
        for (int rep = 1; rep <= REP_NUM; rep++) {
            int disk_id = object[object_id].replica[rep];
            for (int i = 1; i <= object[object_id].size; i++) {
                int index = object[object_id].unit[rep][i];
                auto p =
                        di[disk_id].required.lower_bound(std::make_pair(index, 0));

                if (p != di[disk_id].required.end() &&
                    p->first == index) { // 删除原来的
                    di[disk_id].required.erase(p);
                }

                // if (object[object_id].cnt_request > 0) // 增加新的
                //     di[disk_id].required.insert(
                //         std::make_pair(index, object[object_id].cnt_request));
            }
        }
    }
}

/**
 * 删除操作
 */
void delete_action() {
    int n_delete;                   // 要删除的请求数量
    int abort_num = 0;              // 记录未完成请求的数量
    static int _id[MAX_OBJECT_NUM]; // 存储待删除对象的 ID

    scanf("%d", &n_delete); // 读取删除请求数量
    for (int i = 1; i <= n_delete; i++) {
        scanf("%d", &_id[i]); // 读取每个删除请求的 ID
    }

    // 检查每个请求是否有未完成的请求
    //    for (int i = 1; i <= n_delete; i++) {
    //        int id = _id[i];
    //        int current_id = object[id].last_request_point; //
    //        获取对象的最后请求指针 while (current_id != 0) {
    //            if (request[current_id].is_done == false) {
    //                abort_num++; // 如果请求未完成，增加未完成请求计数
    //            }
    //            current_id = request[current_id].prev_id; // 移动到前一个请求
    //        }
    //    }
    for (int i = 1; i <= n_delete; i++) {
        int id = _id[i];
        abort_num += object[id].cnt_request;
        // abort_num += object[id].deleted_phases.size();
    }

//    std::set<int> object_id_set;

    printf("%d\n", abort_num); // 打印未完成请求的数量
    for (int i = 1; i <= n_delete; i++) {
        int id = _id[i];

//        if (!object[id].active_phases.empty()) {
//            object_id_set.insert(id);
//        }
//        int *replica = object[id].replica;
        while (!object[id].active_phases.empty()) {
            int current_id = object[id].active_phases.front();
            object[id].active_phases.pop_front();
            if (!request[current_id].is_done) { // 这里应该总是可以删除的
                printf("%d\n", current_id);     // 打印未完成请求的 ID
                request[current_id].is_done = true; // 标记请求为已完成，方便从global_requests里删除
            }
            for (int j = 1; j <= REP_NUM; j++) {
                di[object[id].replica[j]].cnt_request --;
            }
        }

        while (!object[id].deleted_phases.empty()) {
            int current_id = object[id].deleted_phases.front();
            object[id].deleted_phases.pop();
            request[current_id].is_done = true; //出于一致性考虑，这里也标记为已完成
            // printf("%d\n", current_id);     // 打印未完成请求的 ID
        }

        // 删除对象的副本
        for (int j = 1; j <= REP_NUM; j++) {
            // do_object_delete(object[id].unit[j],
            // disk_obj_id[object[id].replica[j]], disk_block_id[]
            // ,object[id].size);
            do_object_delete(object[id].unit[j], object[id].replica[j],
                             object[id].size);
            di[object[id].replica[j]].tag_num[object[id].tag]--;
        }
        object[id].cnt_request = 0;
        object[id].is_delete = true; // 标记对象为已删除
    }

//    update_disk_cnt(object_id_set); // 增加请求数量后需要更新磁盘上的set
    fflush(stdout);                 // 刷新输出缓冲区
}

/**
 * 计算磁盘disk_id的最大连续空闲块长度
 * @param disk_id 磁盘编号
 * @return 最大连续空闲块长度
 */
int calculate_max_contiguous(int disk_id, int start, int length) {
    int max_len = 0, current_len = 0;
    for (int i = start; i != (start + length + V - 2) % V + 1; i = i % V + 1) {
        if (disk_obj_id[disk_id][i] == 0) {
            current_len++;
            max_len = std::max(max_len, current_len);
        } else {
            current_len = 0;
        }
    }
    // 环形处理：检查首尾连接的情况（例如末尾和开头连续）
    if (length == V && disk_obj_id[disk_id][start] == 0 && disk_obj_id[disk_id][(start + V - 2) % V + 1] == 0) {
        int head = start, tail = (start + V - 2) % V + 1;
        while (disk_obj_id[disk_id][head] == 0 && head != (start + V - 2) % V + 1)
            head = head % V + 1;
        while (disk_obj_id[disk_id][tail] == 0 && tail != start)
            tail = (tail + V - 2) % V + 1;
        max_len = std::max(max_len, ((start + V - 2) % V + 1 - tail + V) % V + (head - start + V) % V);
    }
    return max_len;
}

/**
 * 选择三个不同磁盘（优先选择连续空间大的）
 * @param id 对象编号
 * @return 选择的磁盘编号列表 std::vector<int>
 */
std::vector<int> select_disks_for_object(int id) {
    std::vector<std::pair<int, int>> disk_scores;
    std::vector<int> vis(N + 1);
    // 遍历所有磁盘，计算得分（连续空间 >= size的磁盘才有资格）
    int tag = object[id].tag;
    for (int i = 1; i <= REP_NUM; i++) {
        int target_hot_disk = hot_tag_alloc[tag].disk[i];
        vis[target_hot_disk] = 1;
        int contiguous = calculate_max_contiguous(target_hot_disk, hot_tag_alloc[tag].start[i], tag_alloc_length[tag]);
        int fixed_score = V * N;
        if (di[target_hot_disk]
                    .subhot_delete_tag[(timestamp - 1) / FRE_PER_SLICING + 1] ==
            tag) {
            if (di[target_hot_disk].distribute_length[tag] * 2 <
                tag_alloc_length[tag] / MAX_OBJECT_SIZE) {
                // fixed_score = 0;
            }
        }
        if (contiguous >= object[id].size) {
            disk_scores.emplace_back(fixed_score + contiguous, target_hot_disk);
        }
    }
    for (int i = 1; i <= N; i++) {
        if (vis[i])
            continue;
        int contiguous = calculate_max_contiguous(i, di[i].end_point, V - di[i].end_point + 1);
        if (contiguous >= object[id].size) {
            disk_scores.emplace_back(contiguous, i);
        }
    }
    for (int i = 1; i <= N; i++) {
        if (vis[i])
            continue;
        int contiguous = calculate_max_contiguous(i, 1, V);
        if (contiguous >= object[id].size) {
            disk_scores.emplace_back(-V * N + contiguous, i);
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

/**
 * 在磁盘disk_id上分配size个连续块
 * @param disk_id 磁盘编号
 * @param size 对象大小
 * @param object_id 对象编号
 * @param reverse_blocks 是否翻转对象块
 * @return 返回分配的存储单元编号列表
 */
std::vector<int> allocate_contiguous_blocks(int disk_id, int size,
                                            int object_id,
                                            bool reverse_blocks) {
//    reverse_blocks = false;
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
        start = 1;
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
            if (block_pos == 0)
                block_pos = V;
            blocks.push_back(block_pos);
            disk_obj_id[disk_id][block_pos] = object_id; // 填充对象编号
            disk_block_id[disk_id][block_pos] =
                    reverse_blocks ? size - j : j + 1; // 填充对象块编号
        }
        if (reverse_blocks)
            std::reverse(blocks.begin(), blocks.end()); // 翻转块
        return blocks;
    };

    /**
     * @brief 检查pos位置是否有效
     * @param pos 位置
     * @return bool 是否有效
     */
    auto check_valid = [&](int pos) {
        for (int j = 0; j < size; j++) {
            int check_pos = (pos + j) % V;
            if (check_pos == 0)
                check_pos = V;
            if (disk_obj_id[disk_id][check_pos] != 0) {
                return false;
            }
        }
        return true;
    };

    if (rep_id != 0) {
        for (int i = 0; i < tag_alloc_length[tag]; i++) {
            int pos = (start + i) % V;
            if (pos == 0)
                pos = V;
            if (disk_obj_id[disk_id][pos] == 0) {
                if (check_valid(pos)) {
                    return save_block(pos);
                }
            }
        }
    }

    if (tag !=
        di[disk_id].subhot_read_tag[(timestamp - 1) / FRE_PER_SLICING + 1]) {
        start = di[disk_id].end_point;
        for (int i = 0; i < tag_alloc_length[tag]; i++) {
            int pos = (start + i) % V;
            if (pos == 0)
                pos = V;
            if (disk_obj_id[disk_id][pos] == 0) {
                if (check_valid(pos)) {
                    return save_block(pos);
                }
            }
        }
    }

    start = V - size + 1;

    for (int i = 0; i < V; i++) {
        int pos = (start - i + V) % V;
        if (pos == 0)
            pos = V;
        if (disk_obj_id[disk_id][pos] == 0) {
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
void do_object_write(int *object_unit, int *disk_unit, int size,
                     int object_id) {
    int current_write_point = 0; // 当前写入位置
    for (int i = 1; i <= V; i++) {
        if (disk_unit[i] == 0) {                    // 如果磁盘单元为空
            disk_unit[i] = object_id;               // 写入对象 ID
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
void write_action() {
    int n_write;           // 写请求数量
    scanf("%d", &n_write); // 读取写请求的数量
    for (int i = 1; i <= n_write; i++) {
        int id, size, tag;
        scanf("%d%d%d", &id, &size, &tag); // 读取对象 ID 和大小
        // std::cerr << "[DEBUG] " << " write_action: " << id << " size: " <<
        // size << " tag: " << tag << std::endl;
        object[id].tag = tag;              // 设置对象标签
        object[id].last_request_point = 0; // 初始化对象的最后请求指针
        object[id].size = size;            // 设置对象的大小
        object[id].is_delete = false;      // 标记对象为未删除
        object[id].cnt_request = 0;
        object[id].last_finish_time = -1;
        total_object_num++;
        std::vector<int> selected_disks = select_disks_for_object(id);
        for (int j = 1; j <= REP_NUM; j++) {
            int disk_id = selected_disks[j - 1];
            std::vector<int> blocks = allocate_contiguous_blocks(
                    disk_id, size, id, j & 1);   // 奇数翻转，偶数不变
            object[id].replica[j] = disk_id; // 计算副本的 ID
            object[id].unit[j] = static_cast<int *>(
                    malloc(sizeof(int) * (size + 1))); // 分配内存以存储对象数据
            for (int _ = 0; _ < size; _++) {
                object[id].unit[j][_ + 1] = blocks[_];
            }
            di[disk_id].tag_num[object[id].tag]++;
            // do_object_write(object[id].unit[j], disk[object[id].replica[j]],
            // size, id); // 将对象数据写入磁盘
        }

        printf("%d\n", id); // 打印对象 ID
        for (int j = 1; j <= REP_NUM; j++) {
            printf("%d", object[id].replica[j]); // 打印副本 ID
            // std::cerr << "[DEBUG] replica" <<j<< " in disk: " <<
            // object[id].replica[j] << " ";
            for (int k = 1; k <= size; k++) {
                printf(" %d", object[id].unit[j][k]); // 打印对象数据
                // std::cerr << "block" << k << ":" << object[id].unit[j][k] <<
                // " ";
            }
            // std::cerr << std::endl;
            printf("\n");
        }
    }

    fflush(stdout); // 刷新输出缓冲区
}
std::pair<int, int> find_max_cnt_request_object(int disk_id) {
    //遍历所有object, 找出当前磁盘上，cnt_request最大的object
    int max_cnt_request = 0;
    int max_cnt_request_object = 0;
    int max_cnt_request_rep = 0;
    for (int obj_id = 1; obj_id <= total_object_num; obj_id++) {
        if (object[obj_id].is_delete) continue;
        if (object[obj_id].cnt_request == 0) continue;
        for (int rep_id = 1; rep_id <= REP_NUM; rep_id++) {
            if (object[obj_id].replica[rep_id] == disk_id) {
                if (object[obj_id].cnt_request > max_cnt_request) {
                    max_cnt_request = object[obj_id].cnt_request;
                    max_cnt_request_object = obj_id;
                    max_cnt_request_rep = rep_id;
                }
            }
        }
    }
    return std::make_pair(max_cnt_request_object, max_cnt_request_rep);
}

inline bool is_valuable(int disk_id, int head) {
    int obj_id = disk_obj_id[disk_id][head];

    if (obj_id == 0) return false;
    int block_id = disk_block_id[disk_id][head];

    if (object[obj_id].cnt_request && request[object[obj_id].active_phases.back()].time >= time_vis[obj_id][block_id]) {
        return true;
    } else {
        return false;
    }
}

std::pair<int, int> get_nearest_valuable_object(int disk_id, int head) {
//    auto ptr = di[disk_id].required.lower_bound(std::make_pair(head, 0));
//    if (ptr != di[disk_id].required.end()) {
//        return *ptr;
//    }
//    return di[disk_id].required.lower_bound(std::make_pair(1, 0));
    int i = head;
    int cnt = G * 2 / 3;//用来调控搜索的范围
    while (cnt--) {
//        if ((obj_id = disk_obj_id[disk_id][i]) != 0) {
//            block_id = disk_block_id[disk_id][i];
//            if (object[obj_id].cnt_request && request[object[obj_id].active_phases.back()].time >= time_vis[obj_id][block_id]) {
//                return std::make_pair(i, object[obj_id].cnt_request);
//            }
//        }
        if (is_valuable(disk_id, i)) {
            return std::make_pair(i, object[disk_obj_id[disk_id][i]].cnt_request);
        }
        i++;
        if (i > V) i = 1;
    }
    return std::make_pair(-1, 0);
}

/**
 *  决策disk_id这块硬盘是否需要进行jump，以及决策首地址。
 * @param disk_id 磁盘编号
 * @return first:表示是否jump;
 * second表示要移动到的位置。特别的，-1表示该磁头无任何操作。
 */
std::pair<int, int> jump_decision(int disk_id, int head_id) {
    // TODO:如果没有有效的对象块该如何决策？
    // TODO:超前搜索一个时间片
    // 当前的策略是保持不动

    int head = disk_head[disk_id][head_id].pos;
//    auto ptr = di[disk_id].required.lower_bound(std::make_pair(head, 0));
    auto ptr = get_nearest_valuable_object(disk_id, head);

    int time_slide_num = timestamp / 1800 + 1; // 下一个时间片
    int tag_id = di[disk_id].subhot_read_tag[time_slide_num];
    int rep = -1;
    for (int r = 1; r <= REP_NUM; r++) {
        if (hot_tag_alloc[tag_id].disk[r] == disk_id) {
            rep = r;
            break;
        }
    }

    if (ptr.first == -1) {//如果磁头后不存在有效对象块
//        ptr = di[disk_id].required.lower_bound(std::make_pair(1, 0));//从头再找一次
        ptr = get_nearest_valuable_object(disk_id, 1);
        if (ptr.first == -1) {//如果从头再找一次，还是不存在有效对象块
            auto max_cnt_request_object = find_max_cnt_request_object(disk_id);//找出当前磁盘上，cnt_request最大的object
            if (max_cnt_request_object.first != 0) {
                int position = max_cnt_request_object.second & 1 ? object[max_cnt_request_object.first].size : 1;
                int dist = get_distance(head, object[max_cnt_request_object.first].unit[max_cnt_request_object.second][position]);
                if (dist >= G * 10 / 10) {
                    return std::make_pair(
                            1, object[max_cnt_request_object.first].unit[max_cnt_request_object.second][position]);
                }
                else{
                    return std::make_pair(0, object[max_cnt_request_object.first].unit[max_cnt_request_object.second][position]);
                }
            }
            // else{
            //     return std::make_pair(1, 1);//直接跳到1号位置
            // }
            // if (rep != -1) {
            //     return std::make_pair(
            //         1, hot_tag_alloc[tag_id]
            //                .start[rep]); // 跳到访问最密集的tag区域的开始
            // } else {
            //     return std::make_pair(1, head); // 这个磁头不进行任何操作
            // }
        }

        int dist = get_distance(head, ptr.first);

        // if (dist >= G) {
        //     return std::make_pair(
        //         1, ptr->first); // 如果距离大于等于G，那么只能jump
        // } else {
        if (dist >= G * 1 / 10)
        {
            auto max_cnt_request_object = find_max_cnt_request_object(disk_id);
            if (max_cnt_request_object.first != 0) {
                int position = max_cnt_request_object.second & 1 ? object[max_cnt_request_object.first].size : 1;
                int dist = get_distance(head, object[max_cnt_request_object.first].unit[max_cnt_request_object.second][position]);
                if (dist >= G * 10 / 10) {
                    return std::make_pair(
                            1, object[max_cnt_request_object.first].unit[max_cnt_request_object.second][position]);
                }
                else{
                    return std::make_pair(0, object[max_cnt_request_object.first].unit[max_cnt_request_object.second][position]);
                }
            }
        }
        return std::make_pair(0, ptr.first); // 反之使用pass即可。
        // return std::make_pair(1, 1);
        // }
    }
    // 2/10: 8857741.6425 (63.9658%)
    // 3/10: 8857741.6425 (63.9658%)
    // 4/10: 8881537.3925 (64.1377%)
    // 5/10: 8842716.1250 (63.8573%)
    // 7/10: 8853003.9975 (63.9316%)
    // 9/10: 8792705.4650 (63.4962%)
    int dist = get_distance(head, ptr.first);
    if (dist >= G * 1 / 20)
    {
        auto max_cnt_request_object = find_max_cnt_request_object(disk_id);
        if (max_cnt_request_object.first != 0) {
            int position = max_cnt_request_object.second & 1 ? object[max_cnt_request_object.first].size : 1;
            int dist = get_distance(head, object[max_cnt_request_object.first].unit[max_cnt_request_object.second][position]);
            if (dist >= G * 10 / 10) {
                return std::make_pair(
                        1, object[max_cnt_request_object.first].unit[max_cnt_request_object.second][position]);
            }
            else{
                return std::make_pair(0, object[max_cnt_request_object.first].unit[max_cnt_request_object.second][position]);
            }
        }
    }
    return std::make_pair(0, ptr.first); // 反之使用pass即可。
    // if (dist >= G) {
    //     auto max_cnt_request_object = find_max_cnt_request_object(disk_id);
    //     if (max_cnt_request_object.first != 0) {
    //         return std::make_pair(
    //             1, object[max_cnt_request_object.first].unit[max_cnt_request_object.second][1]);
    //     }
    //     return std::make_pair(1, ptr->first); // 如果距离大于等于G，那么只能jump
    // } else {
    //     return std::make_pair(0, ptr->first); // 反之使用pass即可。
    // }
}

int dp[MAX_TOKEN_NUM][10];
int dp_path[MAX_TOKEN_NUM][10];

/**
 * 使用动态规划求磁盘disk_id在tokens个令牌内的最优行动序列，优化目标是尽可能走得远
 * @param disk_id 磁盘编号
 * @param tokens 剩余的tokens数量
 * @return 最优的行动序列
 */
std::string dp_plan(int disk_id, int tokens, int head_id) {
    for (int i = 0; i <= tokens; i++) { // 初始化，清空dp和dp_path
        for (int j = 0; j <= 9; j++) {
            dp[i][j] = 1e6;     // 代价设为无穷大
            dp_path[i][j] = -1; // path都设为-1
        }
    }

    int head = disk_head[disk_id][head_id].pos;

    if (disk_head[disk_id][head_id].last_action == 2) { // 初始化
        dp[0][disk_head[disk_id][head_id].last_token] = 0;
    } else {
        dp[0][0] = 0;
    }

    for (int i = 1; i <= tokens; i++) {
        int request_cnt = is_valuable(disk_id, head) ? 1 : 0;

//        if (disk_obj_id[disk_id][head] != 0) {
//            request_cnt = object[disk_obj_id[disk_id][head]].cnt_request;
//        }

        for (int j = 0; j <= 8; j++) {
            if (j == 0) {
                if (request_cnt)
                    continue; // 如果有请求，那么必须使用read而不是pass
                for (int k = 0; k <= 8; k++) {
                    if (dp[i - 1][k] == 1000000)
                        continue; // 无穷大则不合法
                    if (dp[i - 1][k] + 1 > tokens)
                        continue; // 不能超过tokens

                    if (dp[i][j] > dp[i - 1][k] + 1) {
                        dp[i][j] = dp[i - 1][k] + 1;
                        dp_path[i][j] = k;
                    }
                }
            } else if (j != 8) {
                // 无论是否有请求都可以使用read或者pass
                if (dp[i - 1][j - 1] == 1000000)
                    continue; // 无穷大则不合法
                if (dp[i - 1][j - 1] + cost[j] > tokens)
                    continue; // 不能超过tokens

                dp[i][j] = dp[i - 1][j - 1] + cost[j];
                dp_path[i][j] = j - 1;
            } else { // j = 8
                if (dp[i - 1][j - 1] != 1000000 &&
                    dp[i - 1][j - 1] + cost[j] <= tokens) {
                    dp[i][j] = dp[i - 1][j - 1] + cost[j];
                    dp_path[i][j] = j - 1;
                }
                if (dp[i - 1][j] != 1000000 &&
                    dp[i - 1][j] + cost[j] <= tokens) {
                    if (dp[i][j] > dp[i - 1][j] + cost[j]) {
                        dp[i][j] = dp[i - 1][j] + cost[j];
                        dp_path[i][j] = j;
                    }
                }
            }
        }

        head++;
        if (head > V)
            head = 1;
    }

    std::string result;

    for (int i = tokens; i >= 1; i--) { // 找到最远的能走到的位置。
        for (int j = 8; j >= 0; j--) { // 相同的位置，认为尽可能的读会更好
            if (dp[i][j] != 1000000 && dp[i][j] <= tokens) {
                std::vector<int> path;
                int cur_i = i;
                int cur_j = j;
                while (cur_i) {
                    path.push_back(cur_j);
                    cur_j = dp_path[cur_i][cur_j];
                    cur_i--;
                }
//                if (!path.empty()) { // 更新磁头的最后一次操作和最后一次操作消耗的token
//                    if (path[0] == 0) {
//                        disk_head[disk_id].last_action = 1;
//                        disk_head[disk_id].last_token = 0;
//                    } else {
//                        disk_head[disk_id].last_action = 2;
//                        disk_head[disk_id].last_token = path[0];
//                    }
//                }
                std::reverse(path.begin(), path.end());
                //删掉所有的后缀pass操作，也就是path中的0
                while (!path.empty() && *path.rbegin() == 0) {
                    path.pop_back();
                }
                int remain_tokens = tokens;
                for (auto v : path) {
                    if (v == 0) {
                        result += "p";
                        remain_tokens -= 1;
                    }
                    else {
                        result += "r";
                        remain_tokens -= cost[v];
                    }
                }
                //更新磁头
                if (!path.empty()) {
                    if (*path.rbegin()) {
                        disk_head[disk_id][head_id].last_action = 2;
                        disk_head[disk_id][head_id].last_token = *path.rbegin();
                    } else {
                        disk_head[disk_id][head_id].last_action = 1;
                        disk_head[disk_id][head_id].last_token = 0;
                    }
                }

                //尽可能read
                do {
                    int last_token = disk_head[disk_id][head_id].last_token;
                    int c = cost[std::min(last_token + 1, 8)];
                    //计算代价c
                    if (c <= remain_tokens) {
                        result += "r";
                        remain_tokens -= c;
                        disk_head[disk_id][head_id].last_token = std::min(last_token + 1, 8);
                        disk_head[disk_id][head_id].last_action = 2;
                    } else {
                        break;
                    }
                } while (true);

                return result;
            }
        }
    }
    return result;
}

/**
 * 判断可能完成的对象上的请求是否完成
 * @param set 可能完成读入的对象编号
 * @param finished_request 完成的对象请求，引用
 * @param changed_objects 存在被完成请求的对象集合
 */
void judge_request_on_objects(const std::set<int> &set,
                              std::vector<int> &finished_request,
                              std::set<int> &changed_objects) {
    for (auto id : set) {
        auto *deque = &object[id].active_phases;
        bool flag = false;
        while (!deque->empty()) {
            int front = deque->front(); // 取出时间最早的请求
            if (request[front].time <= object[id].last_finish_time) {
                flag = true;
                request[front].is_done = true; //这里标记为true实际上是告诉clean可以删除这个请求了
                if (!request[front].is_timeout) {
                    finished_request.push_back(front);
                }
                deque->pop_front();
                object[id].cnt_request--; // 修改对象的请求数量
                for (int j = 1; j <= REP_NUM; j++) {
                    di[object[id].replica[j]].cnt_request--;
                }
            } else {
                break;
            }
        }

        auto *queue = &object[id].deleted_phases;
        while (!queue->empty()) {
            int front = queue->front(); // 取出时间最早的请求
            if (request[front].time <= object[id].last_finish_time) {
                flag = true;
                request[front].is_done = true;
                if (!request[front].is_timeout) {
                    finished_request.push_back(front);
                }
                queue->pop();
            } else {
                break;
            }
        }
        if (flag) {
            changed_objects.insert(id);
        }
    }
}

/**
 * 对于给定磁盘编号，处理当前时间片的操作
 * @param disk_id 磁盘编号
 * @param actions 记录磁头移动的字符串
 * @param finished_request 记录已经完成的请求
 */
std::set<int> solve_disk(int disk_id, std::string &actions,
                         std::vector<int> &finished_request, int head_id) {
    auto p = jump_decision(disk_id, head_id); // 决策初始位置，以及是否不得不使用jump
    // if(p.first == 1) {
    //     auto max_cnt_request_object = find_max_cnt_request_object(disk_id);
    //     if (max_cnt_request_object.first != 0) {
    //         p.second = object[max_cnt_request_object.first].unit[max_cnt_request_object.second][1];
    //     }
    // }
    int distance = get_distance(disk_head[disk_id][head_id].pos, p.second);
//    disk_head[disk_id].pos = p.second; // 更新磁盘头的位置

    std::set<int> obj_indices;
    std::set<int> changed_objects;

    if (p.first == -1) { // 无操作
        actions = "#\n";
    } else if (p.first == 1) { // jump
        disk_head[disk_id][head_id].pos = p.second; // 更新磁盘头的位置
        actions = "j " + std::to_string(p.second) + "\n";
        disk_head[disk_id][head_id].last_action = 0; // 使用jump
        disk_head[disk_id][head_id].last_token = 0;
    } else if (p.first == 0) { // pass
        //UPDATE:现在去掉了开头的pass操作，整个都是用dp进行决策。
//        for (int i = 1; i <= distance; i++) {
//            actions += "p";
//        }
//        disk_head[disk_id].last_action = 1; // 使用pass
//        disk_head[disk_id].last_token = 0;

        auto s = dp_plan(disk_id, G, head_id); // 使用dp计算最优操作序列，最优化目标位尽可能走得远

        actions += s;
        actions += "#\n";
        int i = disk_head[disk_id][head_id].pos;
        int len = (int)s.length();
        //        int end = s.length() + disk_head[disk_id].pos;

        //更新object的访问时间
        while (len--) {
            int obj_id = disk_obj_id[disk_id][i];
//            std::cerr << "[DEBUG] disk_id: " << disk_id << " disk place: " << i << " obj_id: " << obj_id << std::endl;

            if (!is_valuable(disk_id, i)) {
                i++;
                if (i > V) {
                    i = 1;
                }
                continue;
            }

            obj_indices.insert(obj_id);
            int block_id = disk_block_id[disk_id][i];
            time_vis[obj_id][block_id] = timestamp;

            int min_time = 1000000; // 该对象的对象块的最晚被访问时间。
            for (int j = 1; j <= object[obj_id].size; j++) {
                min_time = std::min(min_time, time_vis[obj_id][j]);
            }
            //            object[obj_id].last_finish_time = min_time;
            //            //修改对象的最后完整访问时间。
            if (min_time != 1000000) {
                object[obj_id].last_finish_time =
                        std::max(object[obj_id].last_finish_time, min_time);
            }

            i++;
            if (i > V) {
                i = 1;
            }
        }

        disk_head[disk_id][head_id].pos = i;

//        std::cerr << "[debug] start judge_request_on_objects" << std::endl;
        judge_request_on_objects(obj_indices, finished_request, changed_objects); // 处理被修改过的对象上潜在的请求
//        std::cerr << "[DEBUG] finish judge_request_on_objects" << std::endl;

//        reset_disk_cnt(changed_objects);
//        update_disk_cnt(
//            changed_objects); // 修改request被完成对象的计数，并更新其set
    }
    return changed_objects;
}

/**
 * 设置请求信息
 * @param request_id 请求编号
 * @param object_id 对象编号
 */
void set_request_info(int request_id, int object_id) {
    request[request_id].object_id = object_id;
    request[request_id].prev_id = object[object_id].last_request_point;
    request[request_id].is_done = false;
    request[request_id].time = timestamp;
    request[request_id].is_timeout = false;

    object[object_id].last_request_point = request[request_id].time;
    object[object_id].active_phases.push_back(request_id);
    object[object_id].cnt_request++;

    global_requestions.push(request_id);

    for (int j = 1; j <= REP_NUM; j++) {
        di[object[object_id].replica[j]].cnt_request++;
    }
}

void clean_timeout_request() {
    //TODO:这个优化不一定和磁盘按照“有效块数量排序”兼容，没想清楚
    static const int time_limit = 105; //超参数，时间片数
    while (!global_requestions.empty()) {
        int request_id = global_requestions.front();
        if (request[request_id].is_done) {
            global_requestions.pop();
            continue;
        }
//        std::cerr<< "[DEBUG] front request id = " << request_id << " time = " << request[request_id].time << std::endl;
        if (timestamp - request[request_id].time >= time_limit) {
            global_requestions.pop();
            int obj_id = request[request_id].object_id;

            object[obj_id].active_phases.pop_front();
            object[obj_id].cnt_request--;
            for (int j = 1; j <= REP_NUM; j++) {
                di[object[obj_id].replica[j]].cnt_request--;
            }
            object[obj_id].deleted_phases.push(request_id);
            timeout_request.push(request_id);
            request[request_id].is_timeout = true;
        } else {
            break;
        }
    }
}

void update_valuable_block_num() {
    for (int i = 1; i <= N; i++) {
        int cnt = 0;
        for (int j = 1; j <= V; j++) {
            if (is_valuable(i, j)) {
                cnt++;
            }
        }
        di[i].valuable_block_num = cnt;
    }
}

/**
 * 处理读入操作
 */
void read_action() {
    int n_read;                     // 读取请求数量
    int request_id = -1, object_id; // 请求 ID 和对象 ID
    scanf("%d", &n_read);           // 读取请求数量

//    std::set<int> object_id_set;

    for (int i = 1; i <= n_read; i++) {
        scanf("%d%d", &request_id, &object_id);
        // std::cerr << "[DEBUG] read_action ["<<i<<"]: request_id = " << request_id << ", object_id = " << object_id << std::endl;
        set_request_info(request_id, object_id);
//        object_id_set.insert(object_id);
    }
    // std::cerr << "[DEBUG] read_action"<<n_read << std::endl;
//    update_disk_cnt(object_id_set); // 增加请求数量后需要更新磁盘上的set

    std::string head_movement[N + 1][MAX_DISK_HEAD_NUM + 1]; // 存储磁头移动记录
    std::vector<int> finished_request;
    std::vector<int> disk_id;

    for (int i = 1; i <= N; i++) {
        disk_id.push_back(i);
    }

    //可能的排序比较函数
    // 0.random
    // 1.此刻磁盘上的有效请求数目
    // 2.此刻磁盘上还可以获得的分数
    // 3.此刻磁盘上需要读取的块的数量
    // 4.last_action读的次数排序降序排

    //方法0
    std::shuffle(disk_id.begin(), disk_id.end(), std::default_random_engine(timestamp));

    //方法1
//    std::sort(disk_id.begin(), disk_id.end(), [](int a, int b) {
//        return di[a].cnt_request > di[b].cnt_request;
//    });

    //方法3
    //需要同时把update_valuable_block_num()注释掉
//    std::sort(disk_id.begin(), disk_id.end(), [](int a, int b) {
//        return di[a].valuable_block_num < di[b].valuable_block_num;
//    });

    //方法4
    /**
     * 优先读，如果都不是读就按照cnt_request排序。
     * 如果都是读，则按照last_token排序。
     */
    // std::sort(disk_id.begin(), disk_id.end(), [](int a, int b) {
    //     if (disk_head[a].last_action == 2 && disk_head[b].last_action == 2) {
    //         return disk_head[a].last_token > disk_head[b].last_token;
    //     } else if (disk_head[a].last_action == 2) {
    //         return true; // 28400043.5997
    //     } else if (disk_head[b].last_action == 2) {
    //         return false;  // 28458840.0522
    //     } else {
    //         return di[a].cnt_request > di[b].cnt_request;//请求数
    //         //    return di[a].valuable_block_num < di[b].valuable_block_num;//有效数，方法3
    //     }
    // });

    std::set<int> changed_objects;
    for (int i = 1; i <= N; i++) {
        for (int j = 1; j <= MAX_DISK_HEAD_NUM; j++) {
            std::set<int> t = solve_disk(disk_id[i - 1], head_movement[disk_id[i - 1]][j], finished_request, j);
            changed_objects.insert(t.begin(), t.end());
        }
//        std::cerr << "[DEBUG] finish disk solve for i = : " << i << std::endl;
    }
//    update_disk_cnt(changed_objects);

    for (int i = 1; i <= N; i++) {
        for (int j = 1; j <= MAX_DISK_HEAD_NUM; j++) {
            printf("%s", head_movement[i][j].c_str());
        }
        // std::cerr << "[DEBUG] head_movement[" << i << "] = " << head_movement[i] << std::endl;
    }

    int finished_request_size = (int)finished_request.size();

    printf("%d\n", finished_request_size);

    for (int i = 0; i < finished_request_size; i++) {
        printf("%d\n", finished_request[i]);
    }

    clean_timeout_request();
    // update_valuable_block_num();//28412380.1172

    int timeout_request_num = (int)timeout_request.size();
    printf("%d\n", timeout_request_num);
    while (!timeout_request.empty()) {
        printf("%d\n", timeout_request.front());
        timeout_request.pop();
    }

    fflush(stdout);
}

// 清理函数，释放动态分配的内存
void clean() {
    for (auto &obj : object) {
        for (int i = 1; i <= REP_NUM; i++) {
            if (obj.unit[i] == nullptr) // 如果指针为空，跳过
                continue;
            free(obj.unit[i]);     // 释放内存
            obj.unit[i] = nullptr; // 将指针置为 nullptr
        }
    }
}

std::vector<std::pair<int, int> > find_swap(int disk_id, int&remain_swap_num) {
    std::vector<std::pair<int, int> > swap_list;
    std::vector<int> visited(V + 1, 0);
    for (int i = 1; i <= V; i++) {
    }
    return swap_list;
}

void garbage_collection_action() {
    char s[10], t[10];
    scanf("%s%s", s, t);
    // std::cerr << "[DEBUG] garbage_collection_action: " << s << " " << t << std::endl;
    printf("GARBAGE COLLECTION\n");
    int remain_swap_num = K;
    for (int i = 1; i <= N; i++) {
        std::vector<std::pair<int, int> > swap_list = find_swap(i, remain_swap_num);
        int swap_list_size = (int)swap_list.size();
        printf("%d\n", swap_list_size);
        for (auto p : swap_list) {
            printf("%d %d\n", p.first, p.second);
        }
    }
    fflush(stdout);
}

/**
 * 预处理标签
 */
void preprocess_tag() {
    // 读取每个标签的删除请求数量
    int slice_num = (T - 1) / FRE_PER_SLICING + 1;
    for (int i = 1; i <= M; i++) {
        for (int j = 1; j <= slice_num; j++) {
            scanf("%d", &fre_del[i][j]);
            fre_del[i][0] += fre_del[i][j];
        }
    }

    // 读取每个标签的写请求数量
    for (int i = 1; i <= M; i++) {
        for (int j = 1; j <= slice_num; j++) {
            scanf("%d", &fre_write[i][j]);
            fre_write[i][0] += fre_write[i][j];
        }
    }

    // 读取每个标签的读请求数量
    for (int i = 1; i <= M; i++) {
        for (int j = 1; j <= slice_num; j++) {
            scanf("%d", &fre_read[i][j]);
            fre_read[i][0] += fre_read[i][j];
        }
    }

    std::vector<int> tag_id;
    for (int i = 1; i <= M; i++) {
        tag_id.push_back(i);
        hot_tag_alloc[i].remain_alloc_num = 3;
    }

    std::sort(tag_id.begin(), tag_id.end(), [](int a, int b) {
        return fre_read[a][0] > fre_read[b][0];
    });

    std::vector<int> hot_tag;
    const int region_first_order = REP_NUM - 1;
    const int hot_tag_cnt = N / region_first_order;

    for (int i = 0; i < hot_tag_cnt; i++) {
        hot_tag.push_back(tag_id[i]);
        hot_tag_alloc[tag_id[i]].is_hot = 1;
    }

    std::sort(tag_id.begin(), tag_id.end(), [](int a, int b) {
        return fre_write[a][0] - fre_del[a][0] > fre_write[b][0] - fre_del[b][0];
    });

    std::vector<int> start_point(N + 1), top(N + 1);
    std::vector<std::set<int> > alloced(N + 1);
    std::vector<int> total_point(N + 1);

    int tot_replica_num = M * REP_NUM;

    for (int i = 1; i <= N; i++) {
        start_point[i] = 1;
    }

    auto update_disk = [&](int tag, int rep, int disk_id, int size) {
        hot_tag_alloc[tag].disk[rep] = disk_id;
        hot_tag_alloc[tag].start[rep] = start_point[disk_id];
        tag_alloc_length[tag] = size;
        di[disk_id].distribute_length[tag] = tag_alloc_length[tag];
        start_point[disk_id] = std::min(V, start_point[disk_id] + tag_alloc_length[tag]);
        total_point[disk_id] += tag_alloc_length[tag];
        top[disk_id] = tag;
        hot_tag_alloc[tag].remain_alloc_num--;
        alloced[disk_id].insert(tag);
        tot_replica_num--;
        for (int i = start_point[disk_id]; i < start_point[disk_id] + tag_alloc_length[tag]; i++) {
            di[disk_id].disk_belong_tag[i] = tag;
        }
    };

    int disk_id = 1;
    for (auto tag : hot_tag) {
        for (int i = 1; i <= region_first_order; i++) {
            int size = fre_write[tag][0] - fre_del[tag][0];
            update_disk(tag, i, disk_id, size);
            disk_id = disk_id % N + 1;
        }
    }


    std::priority_queue<std::pair<int, int>> current_space;
    for (int i = 1; i <= N; i++)
        current_space.emplace(V - start_point[i] + 1, i);

    for (auto i : tag_id) {
        if (!hot_tag_alloc[i].is_hot) {
            int size = fre_write[i][0] - fre_del[i][0];
            tag_alloc_length[i] = size;
            std::vector<std::pair<int, int>> selected_disk(REP_NUM + 1);
            for (int j = 1; j <= REP_NUM; j++) {
                auto it = current_space.top();
                selected_disk[j] = std::make_pair(it.first, it.second);
                current_space.pop();
            }
            for (int j = 1; j <= REP_NUM; j++) {
                int cur_disk_id = selected_disk[j].second;
                int cur_size = size * (1 + 0.05);
                current_space.emplace(selected_disk[j].first - cur_size,
                                      cur_disk_id);
                update_disk(i, j, cur_disk_id, cur_size);
            }
        }
    }

    for (auto i : hot_tag) {
        int size = fre_write[i][0] - fre_del[i][0];
        size = (int)(size * 1.02);
        tag_alloc_length[i] = size;
        std::vector<std::pair<int, int>> selected_disk(REP_NUM + 1);
        for (int j = region_first_order + 1; j <= REP_NUM; j++) {
            auto it = current_space.top();
            selected_disk[j] = std::make_pair(it.first, it.second);
            current_space.pop();
        }
        for (int j = region_first_order + 1; j <= REP_NUM; j++) {
            int cur_disk_id = selected_disk[j].second;
            current_space.emplace(selected_disk[j].first - size,
                                  cur_disk_id);
            update_disk(i, j, cur_disk_id, size);
        }
    }

    for (int i = 1; i <= N; i++) {
        di[i].end_point = start_point[i];
    }

    std::vector<std::vector<std::pair<int, int>>> disk_distribute_vector(
            N + 1, std::vector<std::pair<int, int>>());

    for (auto i : tag_id) {
        //    std::cerr << "[DEBUG] tag: " << i << " is_hot: " <<
        //    hot_tag_alloc[i].is_hot << std::endl;
        for (int j = 1; j <= REP_NUM; j++) {
            //    std::cerr << "[DEBUG]      rep #" << j << ": disk: " <<
            //    hot_tag_alloc[i].disk[j] << " start: " <<
            //    hot_tag_alloc[i].start[j] << std::endl;
            disk_distribute_vector[hot_tag_alloc[i].disk[j]].emplace_back(
                    hot_tag_alloc[i].start[j], i);
        }
    }

    // std::cerr << "[DEBUG] N = " << N << ", V = " << V << std::endl;

    for (int i = 1; i <= N; i++)
        std::sort(disk_distribute_vector.begin(),
                  disk_distribute_vector.end());

    for (int i = 1; i <= N; i++) {
        // std::cerr << "[DEBUG] disk: " << i << ":" << std::endl;
        int cnt = 0;
        for (auto [fi, se] : disk_distribute_vector[i]) {
            di[i].distribute[++cnt] = se;
            // std::cerr << "[DEBUG]      start: " << fi << "(" << se << ")" <<
            // std::endl;
        }
        di[i].tag_distinct_number = cnt;
    }

    for (int i = 1; i <= N; i++) {
        for (int j = 1; j <= (T - 1) / FRE_PER_SLICING + 1; j++) {
            int hot_read_tag_size = 0, hot_read_tag_id = 0;
            int hot_delete_tag_size = 0, hot_delete_tag_id = 0;
            int tag_cnt = di[i].tag_distinct_number;
            for (int k = 1; k <= tag_cnt; k++) {
                if (di[i].distribute[k] == 0)
                    continue;
                if (fre_read[di[i].distribute[k]][j] > hot_read_tag_size) {
                    hot_read_tag_size = fre_read[di[i].distribute[k]][j];
                    hot_read_tag_id = di[i].distribute[k];
                }
                if (fre_del[di[i].distribute[k]][j] > hot_delete_tag_size) {
                    hot_delete_tag_size = fre_del[di[i].distribute[k]][j];
                    hot_delete_tag_id = di[i].distribute[k];
                }
            }
            di[i].subhot_read_tag[j] = hot_read_tag_id;
            di[i].subhot_delete_tag[j] = hot_delete_tag_id;
        }
    }
}

/**
 * 基于这个思路实现https://rocky-robin-46d.notion.site/1bb3b75a16b7803d8457c86b01881322?pvs=4
 */
int main() {
    freopen("log.txt", "w", stderr); // 将调试输出重定向到 log.txt

    scanf("%d%d%d%d%d%d", &T, &M, &N, &V, &G, &K); // 读取参数
    for (int i = 1; i <= N; i++) {           // 初始化磁头位置和当前阶段
        for (int j = 1; j <= MAX_DISK_HEAD_NUM; j++) {
            disk_head[i][j].pos = 1;
        }
    }

    preprocess_tag();

    printf("OK\n"); // 输出 OK
    fflush(stdout); // 刷新输出缓冲区

    // 主循环，处理时间片
    for (int t = 1; t <= T + EXTRA_TIME; t++) {
//         std::cerr << "[DEBUG] " << "------- t: " << t <<"-------"<< std::endl;
        //        std::endl;
        timestamp_action(); // 处理时间戳
//         std::cerr << "[DEBUG] " << "timestamp_action" << std::endl;
        delete_action();    // 处理删除请求
//         std::cerr << "[DEBUG] " << "delete_action" << std::endl;
        write_action();     // 处理写请求
//         std::cerr << "[DEBUG] " << "write_action" << std::endl;
        read_action();      // 处理读请求
//         std::cerr << "[DEBUG] " << "read_action" << std::endl;
        if (t % 1800 == 0) {
            garbage_collection_action();
        }
    }
    clean(); // 清理资源

    return 0; // 返回 0，表示程序正常结束
}
// Origin: 7153134.9875
// 仅仅使用1-2: 7178710.6450: 24760606.92
// 仅仅使用2-3: 7044253.9875: 25552872.98
// 仅仅私用1-3: 7246190.0550:
// object-based: 7176277.4525
// 8881537.3925 (64.1377%)
// 8828032.1025 (63.7513%)
// 8833658.2175 (63.7919%)