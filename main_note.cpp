#include <cstdio>
#include <cassert>
#include <cstdlib>
#include <algorithm>
#include <cstring>
#include <iostream>
#include <cmath>
#include <vector>
#include <string>
#include <queue>
#include <stack>
#include <set>

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

// 请求结构体
typedef struct Request_ {
    int object_id; // 对象 ID
    int prev_id; // 前一个请求 ID
    bool is_done; // 请求是否完成
    int time; // 请求时间
} Request;

// 对象结构体
typedef struct Object_ {
    int replica[REP_NUM + 1]; // 副本 ID 数组
    int* unit[REP_NUM + 1]; // 存储对象数据的指针数组
    int size; // 对象大小
    int last_request_point; // 最后一个请求的指针
    bool is_delete; // 对象是否被标记为删除
    int tag; // 对象标签
    int cnt_request; // 对象请求计数
    int last_finish_time; // 最近一次请求完成时间

    std::deque<int> active_phases; // 活动阶段队列
} Object;

// 磁头状态结构
typedef struct DiskHead_ {
    int pos; // 当前磁头位置（存储单元编号）
    int last_action; // 上一次动作类型：0-Jump, 1-Pass, 2-Read
    int last_token; // 上一次消耗的令牌数在cost中的下标
} DiskHead;

// 全局变量
Request request[MAX_REQUEST_NUM]; // 请求数组
Object object[MAX_OBJECT_NUM]; // 对象数组

int T, M, N, V, G; // 时间片、对象标签、硬盘数量、存储单元、令牌数量
int disk_obj_id[MAX_DISK_NUM][MAX_DISK_SIZE]; // 磁盘上存储的obj的id
int disk_block_id[MAX_DISK_NUM][MAX_DISK_SIZE]; // 磁盘上存储的obj的block的编号
int disk_point[MAX_DISK_NUM]; // 磁盘指针
int timestamp; // 当前时间戳
int time_vis[MAX_OBJECT_NUM][MAX_OBJECT_SIZE]; //表示每个对象块最后一次被read的时间

DiskHead disk_head[MAX_DISK_NUM]; // 磁头状态数组
std::priority_queue<std::pair<int, int> >disk_requests[MAX_DISK_NUM]; // 存储新请求的栈


int fre_del[MAX_TAG_NUM][TAG_PHASE]; // 每个标签的每个阶段删除的对象大小
int fre_write[MAX_TAG_NUM][TAG_PHASE]; // 每个标签的每个阶段写入的对象大小
int fre_read[MAX_TAG_NUM][TAG_PHASE]; // 每个标签的每个阶段读取的对象大小

int disk_tag_num[MAX_DISK_NUM][MAX_TAG_NUM]; // 当前磁盘的标签个数
std::set<std::pair<int, int > > disk_set[MAX_DISK_SIZE]; //存储磁盘每个位置的对象块对应的对象仍有多少查询未完成，只保留第二维非0的元素。
const int cost[] = {0, 64, 52, 42, 34, 28, 23, 19, 16};



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

// 时间戳操作
void timestamp_action()
{
    int cur_time;

    scanf("%*s%d", &cur_time); // 读取当前时间戳
    printf("TIMESTAMP %d\n", cur_time); // 输出时间戳
    timestamp = cur_time; // 更新全局时间戳
    fflush(stdout); // 刷新输出缓冲区
}

/**
 * 删除对象在disk_id这块磁盘上的数据（obj_id 与 block_id）。
 * @param object_unit 对象的存储单元下标
 * @param disk_id 磁盘编号
 * @param size 对象大小
 */
void do_object_delete(const int* object_unit, const int disk_id, int size)
{
    for (int i = 1; i <= size; i++) {
        // disk_obj_unit[object_unit[i]] = 0; // 将磁盘单元标记为 0（删除）
        disk_obj_id[disk_id][object_unit[i]] = 0; //清空磁盘obj_id
        disk_block_id[disk_id][object_unit[i]] = 0; //清空磁盘 block_id
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
                // disk_set[disk_id].erase(disk_set[disk_id].lower_bound(std::make_pair(index, 0)));
//                std::cerr << "[DEBUG] object_id: " << object_id << " disk_id: " << disk_id << " index: " << index << " cnt_request: " << object[object_id].cnt_request << std::endl;
                auto p = disk_set[disk_id].lower_bound(std::make_pair(index, 0));



                if (p != disk_set[disk_id].end() && p->first == index) { //删除原来的
                    disk_set[disk_id].erase(p);
                }

                if (object[object_id].cnt_request > 0)  //增加新的
                    disk_set[disk_id].insert(std::make_pair(index, object[object_id].cnt_request));
            }
        }
    }
}

// 删除操作
void delete_action()
{
    int n_delete; // 要删除的请求数量
    int abort_num = 0; // 记录未完成请求的数量
    static int _id[MAX_OBJECT_NUM]; // 存储待删除对象的 ID

    scanf("%d", &n_delete); // 读取删除请求数量
    for (int i = 1; i <= n_delete; i++) {
        scanf("%d", &_id[i]); // 读取每个删除请求的 ID
    }

    // 检查每个请求是否有未完成的请求
//    for (int i = 1; i <= n_delete; i++) {
//        int id = _id[i];
//        int current_id = object[id].last_request_point; // 获取对象的最后请求指针
//        while (current_id != 0) {
//            if (request[current_id].is_done == false) {
//                abort_num++; // 如果请求未完成，增加未完成请求计数
//            }
//            current_id = request[current_id].prev_id; // 移动到前一个请求
//        }
//    }
    for (int i = 1; i <= n_delete; i++) {
        int id = _id[i];
        abort_num += object[id].cnt_request;
    }

    std::set<int> object_id_set;

    printf("%d\n", abort_num); // 打印未完成请求的数量
    for (int i = 1; i <= n_delete; i++) {
        int id = _id[i];

        if (!object[id].active_phases.empty()) {
            object_id_set.insert(id);
        }

        while (!object[id].active_phases.empty()) {
            int current_id = object[id].active_phases.front();
            object[id].active_phases.pop_front();
            if (!request[current_id].is_done) { // 这里应该总是可以删除的
                printf("%d\n", current_id); // 打印未完成请求的 ID
            }
        }
        // 删除对象的副本
        for (int j = 1; j <= REP_NUM; j++) {
            // do_object_delete(object[id].unit[j], disk_obj_id[object[id].replica[j]], disk_block_id[] ,object[id].size);
            do_object_delete(object[id].unit[j], object[id].replica[j], object[id].size);
            disk_tag_num[object[id].replica[j]][object[id].tag]--;
        }
        object[id].cnt_request = 0;
        object[id].is_delete = true; // 标记对象为已删除
    }

    update_disk_cnt(object_id_set); //增加请求数量后需要更新磁盘上的set
    fflush(stdout); // 刷新输出缓冲区
}

// 计算磁盘disk_id的最大连续空闲块长度
int calculate_max_contiguous(int disk_id) {
    int max_len = 0, current_len = 0;
    for (int i = 1; i <= V; i++) {
        if (disk_obj_id[disk_id][i] == 0) {
            current_len++;
            max_len = std::max(max_len, current_len);
        } else {
            current_len = 0;
        }
    }
    // 环形处理：检查首尾连接的情况（例如末尾和开头连续）
    if (disk_obj_id[disk_id][V] == 0 && disk_obj_id[disk_id][1] == 0) {
        int head = 1, tail = V;
        while (disk_obj_id[disk_id][head] == 0 && head <= V) head++;
        while (disk_obj_id[disk_id][tail] == 0 && tail >= 1) tail--;
        max_len = std::max(max_len, (V - tail) + (head - 1) + 2);
    }
    return max_len;
}

// 选择三个不同磁盘（优先选择连续空间大的）
std::vector<int> select_disks_for_object(int id) {
    std::vector<std::pair<int, int> > disk_scores;
    // 遍历所有磁盘，计算得分（连续空间 >= size的磁盘才有资格）
    for (int i = 1; i <= N; i++) {
        int contiguous = calculate_max_contiguous(i);
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

/**
 * 在磁盘disk_id上分配size个连续块
 * @param disk_id 磁盘编号
 * @param size 对象大小
 * @param object_id 对象编号
 * @param reverse_blocks 是否翻转对象块
 * @return 返回分配的存储单元编号列表
 */
std::vector<int> allocate_contiguous_blocks(int disk_id, int size, int object_id, bool reverse_blocks) {
    // 从磁头当前位置开始搜索（减少未来读取时的移动距离）
    int start = disk_head[disk_id].pos;
    for (int i = 0; i < V; i++) {
        int pos = (start + i) % V;
        if (pos == 0) pos = V; // 存储单元编号从1开始
        if (disk_obj_id[disk_id][pos] == 0) {
            bool found = true;
            std::vector<int> blocks;
            // 检查后续size个单元是否都空闲
            for (int j = 0; j < size; j++) {
                int check_pos = (pos + j) % V;
                if (check_pos == 0) check_pos = V;
                if (disk_obj_id[disk_id][check_pos] != 0) {
                    found = false;
                    break;
                }
            }
            if (found) {
                for (int j = 0; j < size; j++) {
                    int block_pos = (pos + j) % V;
                    if (block_pos == 0) block_pos = V;
                    blocks.push_back(block_pos);
                    disk_obj_id[disk_id][block_pos] = object_id; // 填充对象编号
                    disk_block_id[disk_id][block_pos] = reverse_blocks ? size - j : j + 1; //填充对象块编号
                }
                if (reverse_blocks) std::reverse(blocks.begin(), blocks.end()); // 翻转块
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
        // std::cerr << "[DEBUG] " << " write_action: " << id << " size: " << size << " tag: " << tag << std::endl;
        object[id].tag = tag; // 设置对象标签
        object[id].last_request_point = 0; // 初始化对象的最后请求指针
        object[id].size = size; // 设置对象的大小
        object[id].is_delete = false; // 标记对象为未删除
        object[id].cnt_request = 0;
        object[id].last_finish_time = -1;
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
            // std::cerr << "[DEBUG] 副本" <<j<< " in disk: " << object[id].replica[j] << " ";
            for (int k = 1; k <= size; k++) {
                printf(" %d", object[id].unit[j][k]); // 打印对象数据
                // std::cerr << "block"<<k<< ":" << object[id].unit[j][k] << " ";
            }
            // std::cerr << std::endl;
            printf("\n");
        }
    }

    fflush(stdout); // 刷新输出缓冲区
}

/**
 *  决策disk_id这块硬盘是否需要进行jump，以及决策首地址。
 * @param disk_id 磁盘编号
 * @return first:表示是否jump;
 * second表示要移动到的位置。特别的，-1表示该磁头无任何操作。
 */
std::pair<int, int> jump_decision(int disk_id) {
    //TODO:如果没有有效的对象块该如何决策？
    //TODO:超前搜索一个时间片
    //当前的策略是保持不动

    int head = disk_head[disk_id].pos;
    auto ptr = disk_set[disk_id].lower_bound(std::make_pair(head, 0));

    if (ptr == disk_set[disk_id].end()) {
        ptr = disk_set[disk_id].lower_bound(std::make_pair(1, 0));
        if (ptr == disk_set[disk_id].end()) {
            return std::make_pair(0, head); // 这个磁头不进行任何操作
        }

        int dist = get_distance(head, ptr->first);

        if (dist >= G) {
            return std::make_pair(1, ptr->first);// 如果距离大于等于G，那么只能jump
        } else {
            return std::make_pair(0, ptr->first);// 反之使用pass即可。
        }
    }

    int dist = get_distance(head, ptr->first);
    if (dist >= G) {
        return std::make_pair(1, ptr->first);// 如果距离大于等于G，那么只能jump
    } else {
        return std::make_pair(0, ptr->first);// 反之使用pass即可。
    }
}


int dp[MAX_TOKEN_NUM][10];
int dp_path[MAX_TOKEN_NUM][10];

/**
 * 使用动态规划求磁盘disk_id在tokens个令牌内的最优行动序列，优化目标是尽可能走得远
 * @param disk_id 磁盘编号
 * @param tokens 剩余的tokens数量
 * @return 最优的行动序列
 */
std::string dp_plan(int disk_id, int tokens) {
    for (int i = 0; i <= tokens; i++) { //初始化，清空dp和dp_path
        for (int j = 0; j <= 9; j++) {
            dp[i][j] = 1e6; //代价设为无穷大
            dp_path[i][j] = -1; //path都设为-1
        }
    }

    int head = disk_head[disk_id].pos;

    if (disk_head[disk_id].last_action == 2) { //初始化
        dp[0][disk_head[disk_id].last_token] = 0;
    } else {
        dp[0][0] = 0;
    }

    for (int i = 1; i <= tokens; i++) {
        int request_cnt = 0; // 磁盘位置head + i - 1上的请求数量
//        int real_pos = (head + i - 1 - 1) % V + 1;
//        assert(real_pos >= 1 && real_pos <= V);

        if (disk_obj_id[disk_id][head] != 0) {
            request_cnt = object[disk_obj_id[disk_id][head]].cnt_request;
        }

        for (int j = 0; j <= 8; j++) {
            if (j == 0) {
                if (request_cnt) continue; // 如果有请求，那么必须使用read而不是pass
                for (int k = 0; k <= 8; k++) {
                    if (dp[i - 1][k] == 1000000) continue; //无穷大则不合法
                    if (dp[i - 1][k] + 1 > tokens) continue;//不能超过tokens

                    if (dp[i][j] > dp[i - 1][k] + 1) {
                        dp[i][j] = dp[i - 1][k] + 1;
                        dp_path[i][j] = k;
                    }
                }
            } else if (j != 8){
                //无论是否有请求都可以使用read或者pass
                if (dp[i - 1][j - 1] == 1000000) continue; //无穷大则不合法
                if (dp[i - 1][j - 1] + cost[j] > tokens) continue; //不能超过tokens

                dp[i][j] = dp[i - 1][j - 1] + cost[j];
                dp_path[i][j] = j - 1;
            } else { // j = 8
                if (dp[i - 1][j - 1] != 1000000 && dp[i - 1][j - 1] + cost[j] <= tokens) {
                    dp[i][j] = dp[i - 1][j - 1] + cost[j];
                    dp_path[i][j] = j - 1;
                }
                if (dp[i - 1][j] != 1000000 && dp[i - 1][j] + cost[j] <= tokens) {
                    if (dp[i][j] > dp[i - 1][j] + cost[j]) {
                        dp[i][j] = dp[i - 1][j] + cost[j];
                        dp_path[i][j] = j;
                    }
                }
            }
        }

        head++;
        if (head > V) head = 1;
    }

    std::string result = "";

    for (int i = tokens; i >= 1; i--) {// 找到最远的能走到的位置。
        for (int j = 8; j >= 0; j--) {//相同的位置，认为尽可能的读会更好
            if (dp[i][j] != 1000000 && dp[i][j] <= tokens) {
                std::vector<int> path;
                int cur_i = i;
                int cur_j = j;
                while (cur_i) {
                    path.push_back(cur_j);
                    cur_j = dp_path[cur_i][cur_j];
                    cur_i--;
                }
                if (!path.empty()) {// 更新磁头的最后一次操作和最后一次操作消耗的token
                    if (path[0] == 0) {
                        disk_head[disk_id].last_action = 1;
                        disk_head[disk_id].last_token = 0;
                    } else {
                        disk_head[disk_id].last_action = 2;
                        disk_head[disk_id].last_token = path[0];
                    }
                }

                std::reverse(path.begin(), path.end());
                for (auto v : path) {
                    if (v == 0)
                        result += "p";
                    else
                        result += "r";
                }

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
void judge_request_on_objects(const std::set<int> & set, std::vector<int> &finished_request, std::set<int> & changed_objects) {
    for (auto id : set) {
        auto *deque = &object[id].active_phases;
        bool flag = false;
        while (!deque->empty()) {
            int front = deque->front();// 取出时间最早的请求
            if (request[front].time <= object[id].last_finish_time) {
                flag = true;
                request[front].is_done = true;
                finished_request.push_back(front);
                deque->pop_front();
                object[id].cnt_request--; //修改对象的请求数量
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
 * @return 该磁盘在该时间片的操作序列（string）
 */
void solve_disk(int disk_id, std::string &actions, std::vector<int> &finished_request) {
    auto p = jump_decision(disk_id); //决策初始位置，以及是否不得不使用jump

    int distance = get_distance(disk_head[disk_id].pos, p.second);
    disk_head[disk_id].pos = p.second; //更新磁盘头的位置

    std::set<int> obj_indices;

    if (p.first == -1) { // 无操作
        actions = "#\n";
    } else if (p.first == 1) { //jump
        actions = "j " + std::to_string(p.second) + "\n";
        disk_head[disk_id].last_action = 0; // 使用jump
        disk_head[disk_id].last_token = 0;
    } else if (p.first == 0) { //pass
        for (int i = 1; i <= distance; i++) {
            actions += "p";
        }
        disk_head[disk_id].last_action = 1; //使用pass
        disk_head[disk_id].last_token = 0;

        auto s = dp_plan(disk_id, G - distance); //使用dp计算最优操作序列，最优化目标位尽可能走得远
        actions += s;
        actions += "#\n";
        int i = disk_head[disk_id].pos;
        int len = s.length();
//        int end = s.length() + disk_head[disk_id].pos;

        while (len--) {
            int obj_id = disk_obj_id[disk_id][i];

            if (obj_id == 0) {
                i++;
                if (i > V) {
                    i = 1;
                }
                continue;
            } else if (object[obj_id].cnt_request == 0) {
                i++;
                if (i > V) {
                    i = 1;
                }
                continue;
            }

            obj_indices.insert(obj_id);
            int block_id = disk_block_id[disk_id][i];
            time_vis[obj_id][block_id] = timestamp;

            int mintime = 1000000; //该对象的对象块的最晚被访问时间。
            for (int j = 1; j <= object[obj_id].size; j++) {
                mintime = std::min(mintime, time_vis[obj_id][j]);
            }
//            object[obj_id].last_finish_time = mintime; //修改对象的最后完整访问时间。
            if (mintime != 1000000) {
                object[obj_id].last_finish_time = std::max(object[obj_id].last_finish_time, mintime);
            }

            i++;
            if (i > V) {
                i = 1;
            }
        }

        disk_head[disk_id].pos = i;

        std::set<int> changed_objects;

        judge_request_on_objects(obj_indices, finished_request, changed_objects); //处理被修改过的对象上潜在的请求
        update_disk_cnt(changed_objects);//修改request被完成对象的计数，并更新其set
    }
}

/**
 * 处理读入操作
 */
void read_action()
{
    int n_read; //读取请求数量
    int request_id = -1, object_id; //请求 ID 和对象 ID
    scanf("%d", &n_read);// 读取请求数量

    std::set<int> object_id_set;

    for (int i = 1; i <= n_read; i++) {
        scanf("%d%d", &request_id, &object_id);
        object_id_set.insert(object_id);
        request[request_id].object_id = object_id;
        request[request_id].prev_id = object[object_id].last_request_point;
        request[request_id].is_done = false;
        request[request_id].time = timestamp;

        object[object_id].last_request_point = request[request_id].time;
        object[object_id].active_phases.push_back(request_id);
        object[object_id].cnt_request++;
    }
    update_disk_cnt(object_id_set); //增加请求数量后需要更新磁盘上的set


    std::string head_movement[N + 1]; // 存储磁头移动记录
    std::vector<int> finished_request;

    for (int i = 1; i <= N; i++) {
        solve_disk(i, head_movement[i], finished_request);
    }

    for (int i = 1; i <= N; i++) {
        printf("%s", head_movement[i].c_str());
    }

    int finished_request_size = finished_request.size();

    printf("%d\n", finished_request_size);

    for (int i = 0; i < finished_request_size; i++) {
        printf("%d\n", finished_request[i]);
    }

    fflush(stdout);
}

// 清理函数，释放动态分配的内存
void clean()
{
    for (auto& obj : object) {
        for (int i = 1; i <= REP_NUM; i++) {
            if (obj.unit[i] == nullptr) // 如果指针为空，跳过
                continue;
            free(obj.unit[i]); // 释放内存
            obj.unit[i] = nullptr; // 将指针置为 nullptr
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
    for(int i=1; i<=N; i++) { // 初始化磁头位置和当前阶段
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

    printf("OK\n"); // 输出 OK
    fflush(stdout); // 刷新输出缓冲区

    for (int i = 1; i <= N; i++) {
        disk_point[i] = 1; // 初始化每个硬盘的磁头初始位置
    }

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