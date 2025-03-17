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

#define MAX_DISK_NUM (10 + 1) // 最大磁盘数量
#define MAX_DISK_SIZE (16384 + 1) // 最大磁盘大小
#define MAX_REQUEST_NUM (30000000 + 1) // 最大请求数量
#define MAX_OBJECT_NUM (100000 + 1) // 最大对象数量
#define REP_NUM (3) // 每个对象的副本数量
#define FRE_PER_SLICING (1800) // 每个时间片的最大请求数
#define EXTRA_TIME (105) // 额外时间片

// 请求结构体
typedef struct Request_ {
    int object_id; // 对象 ID
    int prev_id; // 前一个请求 ID
    bool is_done; // 请求是否完成
    int time; // 请求时间
    int disk_id; // 正在处理请求的磁盘 ID
    int rep; // 选择的副本

    std::string head_movement; // 磁头移动记录
} Request;

// 对象结构体
typedef struct Object_ {
    int replica[REP_NUM + 1]; // 副本 ID 数组
    int* unit[REP_NUM + 1]; // 存储对象数据的指针数组
    int size; // 对象大小
    int last_request_point; // 最后一个请求的指针
    bool is_delete; // 对象是否被标记为删除

    std::queue<int> active_phases; // 活动阶段队列
} Object;

// 磁头状态结构
typedef struct DiskHead_ {
    int pos; // 当前磁头位置（存储单元编号）
    int last_action; // 上一次动作类型：0-Jump, 1-Pass, 2-Read
    int last_token; // 上一次消耗的令牌数

    int current_phase; // 当前阶段
    int current_request; // 当前请求
} DiskHead;

// 全局变量
Request request[MAX_REQUEST_NUM]; // 请求数组
Object object[MAX_OBJECT_NUM]; // 对象数组

int T, M, N, V, G; // 时间片、对象标签、硬盘数量、存储单元、令牌数量
int disk[MAX_DISK_NUM][MAX_DISK_SIZE]; // 磁盘数据
int disk_point[MAX_DISK_NUM]; // 磁盘指针
int timestamp; // 当前时间戳

DiskHead disk_head[MAX_DISK_NUM]; // 磁头状态数组
std::stack<int> disk_requests[MAX_DISK_NUM]; // 存储新请求的栈

// 时间戳操作
void timestamp_action()
{
    int cur_time;

    scanf("%*s%d", &cur_time); // 读取当前时间戳
    printf("TIMESTAMP %d\n", cur_time); // 输出时间戳
    timestamp = cur_time; // 更新全局时间戳
    fflush(stdout); // 刷新输出缓冲区
}

// 删除对象的函数
void do_object_delete(const int* object_unit, int* disk_unit, int size)
{
    for (int i = 1; i <= size; i++) {
        disk_unit[object_unit[i]] = 0; // 将磁盘单元标记为 0（删除）
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
    for (int i = 1; i <= n_delete; i++) {
        int id = _id[i];
        int current_id = object[id].last_request_point; // 获取对象的最后请求指针
        while (current_id != 0) {
            if (request[current_id].is_done == false) {
                abort_num++; // 如果请求未完成，增加未完成请求计数
            }
            current_id = request[current_id].prev_id; // 移动到前一个请求
        }
    }

    printf("%d\n", abort_num); // 打印未完成请求的数量
    for (int i = 1; i <= n_delete; i++) {
        int id = _id[i];
        int current_id = object[id].last_request_point; // 获取对象的最后请求指针
        while (current_id != 0) {
            if (request[current_id].is_done == false) {
                printf("%d\n", current_id); // 打印未完成请求的 ID
            }
            current_id = request[current_id].prev_id; // 移动到前一个请求
        }
        // 删除对象的副本
        for (int j = 1; j <= REP_NUM; j++) {
            do_object_delete(object[id].unit[j], disk[object[id].replica[j]], object[id].size);
        }
        object[id].is_delete = true; // 标记对象为已删除
    }

    fflush(stdout); // 刷新输出缓冲区
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
        int id, size;
        scanf("%d%d%*d", &id, &size); // 读取对象 ID 和大小
        object[id].last_request_point = 0; // 初始化对象的最后请求指针
        for (int j = 1; j <= REP_NUM; j++) {
            object[id].replica[j] = (id + j) % N + 1; // 计算副本的 ID
            object[id].unit[j] = static_cast<int*>(malloc(sizeof(int) * (size + 1))); // 分配内存以存储对象数据
            object[id].size = size; // 设置对象的大小
            object[id].is_delete = false; // 标记对象为未删除
            do_object_write(object[id].unit[j], disk[object[id].replica[j]], size, id); // 将对象数据写入磁盘
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
    if(size <= 1) return 0; // 如果只有一个块，返回 0
    
    int sorted[MAX_DISK_SIZE]; // 存储排序后的块
    memcpy(sorted, blocks+1, sizeof(int)*size); // 复制块数据
    std::sort(sorted, sorted+size); // 排序块数据
    
    int score = 0, lst = 64, pass = 0; // 初始化评分、上一次消耗的令牌数和通过的块数
    for(int i=1; i<size; i++){
        lst = (pass ? 64 : std::max(16, (int)ceil(lst * 0.8))); // 计算当前块的消耗
        pass = sorted[i] - sorted[i-1] - 1; // 计算当前块与前一个块之间的间隙
        score += pass + lst; // 更新评分
    }
    
    return score; // 返回评分
}

// 计算移动到第一个块的最短距离（环形）
int calculate_move_cost(int current_pos, int first_block) {
    int clockwise = (first_block - current_pos + V) % V; // 计算顺时针移动的距离
    return clockwise; // 返回移动成本
}

// 副本评分函数
int evaluate_replica(int rep_id, const Object* obj, int current_time) {
    const int disk_id = obj->replica[rep_id]; // 获取副本所在的磁盘 ID
    const int head_pos = disk_head[disk_id].pos; // 获取当前磁头位置
    const int* blocks = obj->unit[rep_id]; // 获取副本的数据块
    
    // 紧凑性评分
    int compactness = G - std::min(G, calculate_compactness(blocks, obj->size));
    
    // 移动成本评分
    int move_cost = G - std::min(G, calculate_move_cost(head_pos, blocks[1]));
    
    // 时间局部性评分（需预存标签访问模式）
    // 此处需要接入预处理数据，暂用固定值
    int time_score = 0;
    
    return compactness*1 + move_cost*1 + time_score*1; // 返回总评分
}

// 选择最佳副本
int select_best_replica(int object_id) {
    const Object* obj = &object[object_id]; // 获取对象
    int best_rep = -1; // 最佳副本索引
    int max_score = -1; // 最大评分
    
    for(int rep=1; rep<=REP_NUM; rep++){
        int score = evaluate_replica(rep, obj, 0); // 评估副本
        if(score > max_score){ // 如果评分更高
            max_score = score; // 更新最大评分
            best_rep = rep; // 更新最佳副本
        }
        // std::cerr << "[DEBUG] " << " rep: " << rep << " score: " << score << std::endl; // 调试输出
    }
    // std::cerr << "[DEBUG] " << " best_rep: " << best_rep << std::endl; // 调试输出最佳副本
    return best_rep; // 返回最佳副本
}

int select_best_replica_available(int object_id, bool* available_disks) {
    const Object* obj = &object[object_id]; // 获取对象
    int best_rep = -1; // 最佳副本索引
    int max_score = -1; // 最大评分
    for(int rep=1; rep<=REP_NUM; rep++){
        int disk_id = obj->replica[rep];
        if(available_disks[disk_id])
        {
            int score = evaluate_replica(rep, obj, 0); // 评估副本
            if(score > max_score){ // 如果评分更高
                max_score = score; // 更新最大评分
                best_rep = rep; // 更新最佳副本
            }
            // std::cerr << "[DEBUG] " << " rep: " << rep << " score: " << score << std::endl; // 调试输出
        }
    }
    // std::cerr << "[DEBUG] " << " best_rep: " << best_rep << std::endl; // 调试输出最佳副本
    return best_rep; // 返回最佳副本
}



// 执行对象读取操作
void do_object_read(int object_id, int target_disk, std::string &head_movement){
    int is_read = 0; // 读取标志
    int &current_phase = disk_head[target_disk].current_phase; // 当前阶段
    int best_rep = -1; // 获取最佳副本
    for (int i = 1; i <= REP_NUM; i++)
    {
        if (object[object_id].replica[i] == target_disk)
        {
            best_rep = i;
        }
    }
    for (int i = 1; i <= N; i++) 
    {
        if (i == target_disk) 
        { // 如果是目标磁盘
            int target_pos = object[object_id].unit[best_rep][current_phase + 1]; // 获取目标位置
            if (target_pos == 0)
            {
                std::cerr << "[Error] " << " target_pos: " << target_pos << " object_id: " << object_id << " object->size: " << object[object_id].size<< " current_phase: " << current_phase << " target_disk: " << target_disk << std::endl;
            }
            int remain_token = G; // 剩余令牌数
            if (target_pos != disk_head[target_disk].pos) { // 如果目标位置与当前磁头位置不同
                int pass_cost = calculate_move_cost(disk_head[target_disk].pos, target_pos); // 计算移动成本
                if (pass_cost > G) { // 如果移动成本大于剩余令牌
                    remain_token -= G; // 消耗所有令牌
                    disk_head[target_disk].pos = target_pos; // 更新磁头位置
                    disk_head[target_disk].last_action = 0; // 更新上一次动作类型
                    disk_head[target_disk].last_token = G; // 更新上一次消耗的令牌数
                    // printf("j %d\n", target_pos); // 输出跳转命令
                    head_movement += "j " + std::to_string(target_pos) + "\n"; // 记录磁头移动
                    continue; // 继续下一个循环
                } else {
                    remain_token -= pass_cost; // 消耗移动成本
                    for (int i = 1; i <= pass_cost; i++) {
                        // printf("p"); // 输出通过命令
                        // std::cerr << "[OUTPUT] " << " pass" << std::endl;
                        head_movement += "p"; // 记录磁头移动
                    }
                    disk_head[target_disk].pos = target_pos; // 更新磁头位置
                    disk_head[target_disk].last_action = 1; // 更新上一次动作类型
                    disk_head[target_disk].last_token = 1; // 更新上一次消耗的令牌数
                }
            }
            while(true) {
                // std::cerr << remain_token << " " << current_phase << " " << object[object_id].size << std::endl;
                int move_cost = (disk_head[target_disk].last_action != 2) ? 64 : 
                    std::max(16, (int)ceil(disk_head[target_disk].last_token * 0.8)); // 计算移动成本
                target_pos = object[object_id].unit[best_rep][current_phase + 1]; // 获取目标位置
                if(move_cost > remain_token || current_phase == object[object_id].size || target_pos != disk_head[target_disk].pos) {
                    if(target_pos != disk_head[target_disk].pos) {
                        // std::cerr << "[DEBUG] " << " move_cost: " << move_cost << " remain_token: " << remain_token << " target_disk: " << target_disk << std::endl;
                    }
                    // printf("#\n"); // 输出 #
                    head_movement += "#\n"; // 记录磁头移动
                    // std::cerr << "---break---" <<disk_head[target_disk].current_phase<<":"<< disk_head[target_disk].current_request<<std::endl;
                    break; // 结束循环
                }
                current_phase++; // 进入下一个阶段
                // printf("r"); // 输出读取命令
                head_movement += "r"; // 记录磁头移动
                disk_head[target_disk].pos = (disk_head[target_disk].pos % V) + 1; // 更新磁头位置
                disk_head[target_disk].last_action = 2; // 更新上一次动作类型
                disk_head[target_disk].last_token = move_cost; // 更新上一次消耗的令牌数
                remain_token -= move_cost; // 消耗移动成本
            }
        } else {
            // printf("#\n"); // 如果不是目标磁盘，输出 #
        }
    }
}
void reset_disk_head(int disk_id)//重置磁头，等待下一个任务
{
    disk_head[disk_id].current_phase = 0; // 重置磁头当前阶段
    disk_head[disk_id].current_request = -1; // 重置磁头当前请求
}
bool check_disk_head(int disk_id)//检查当前盘是否空闲
{
    if(disk_head[disk_id].current_request == -1)
    {
        return true;
    }
    return false;
}

// 读取操作
/*
Origin:
直接取出最近的一个请求。
然后找到一个最佳的副本，进行输出。
Update:
1. 给空闲的磁盘分配任务
2. 每个磁盘进行工作
*/
void read_action()
{
    int n_read; // 读取请求数量
    int request_id = -1, object_id; // 请求 ID 和对象 ID
    scanf("%d", &n_read); // 读取请求数量
    static std::stack<int> new_requests; // 存储新请求的栈
    for (int i = 1; i <= n_read; i++) {
        scanf("%d%d", &request_id, &object_id); // 读取请求 ID 和对象 ID
        request[request_id].object_id = object_id; // 记录请求的对象 ID
        request[request_id].prev_id = object[object_id].last_request_point; // 记录前一个请求 ID
        object[object_id].last_request_point = request_id; // 更新对象的最后请求指针
        object[object_id].active_phases.push(request_id); // 将请求 ID 添加到活动阶段队列
        request[request_id].is_done = false; // 标记请求为未完成
        request[request_id].time = timestamp; // 记录请求时间
        request[request_id].rep = -1; // 初始化副本索引
        request[request_id].disk_id = -1;
        new_requests.push(request_id); // 将请求 ID 压入栈中
        for(int rep=1; rep<=REP_NUM; rep++){
            int disk_id = object[object_id].replica[rep];
            disk_requests[disk_id].push(request_id);
            // std::cerr << "[DEBUG] " << " disk_id: " << disk_id << " request_id: " << request_id << std::endl;
        }
    }
    // std::cerr << "[DEBUG] " << " n_read: " << n_read << std::endl;
    if (!n_read)  // 如果没有当前请求
    {
        for (int i = 1; i <= N; i++) {
            printf("#\n"); // 如果没有请求，输出 #
            // std::cerr << "[OUTPUT] " << "#" << std::endl;
        }
        printf("0\n"); // 输出 0
        // std::cerr << "[OUTPUT] " << "0" << std::endl;
        fflush(stdout); // 刷新输出缓冲区
        return; // 结束函数
    }
    // 移除已完成的请求
    // while(!new_requests.empty() && request[new_requests.top()].is_done == true) new_requests.pop();

    std::vector<int> finished_requests; // 存储已完成的阶段
    std::string head_movement[MAX_DISK_NUM]; // 存储磁头移动记录

    bool available_disks[MAX_DISK_NUM];//储存可用磁头的数组
    for (int i = 1; i <= N; i++) {
        available_disks[i] = check_disk_head(i);
    }
    while(!new_requests.empty())//分配请求到磁盘，直到磁盘上都有任务，但可能存在有的磁盘未被使用。
    {
        int current_request = 0; // 当前请求 ID
        current_request = new_requests.top(); // 记录当前请求
        new_requests.pop(); // 从栈中移除当前请求
        if (request[current_request].is_done) continue;
        if (request[current_request].disk_id != -1) continue;
        
        object_id = request[current_request].object_id; // 获取当前请求的对象 ID
        int best_rep = select_best_replica_available(object_id, available_disks); // 选择最佳磁盘
        if (best_rep == -1) // 如果没有可用的磁盘，就退出。
        {
            new_requests.push(current_request);
            break;
        }
        request[current_request].rep = request[current_request].rep == -1 ? best_rep : request[current_request].rep; // 更新副本索引
        
        int target_disk = object[object_id].replica[request[current_request].rep]; // 获取目标磁盘
        request[current_request].disk_id = target_disk;
        // std::cerr << "[DEBUG] " << " target_disk: " << target_disk << " object_id: " << object_id << " request_id: " << current_request << std::endl;
        disk_head[target_disk].current_request = current_request; //分配任务
        available_disks[target_disk] = false; //标记磁盘为忙碌
    }
    for (int i = 1; i <= N; i++) {
        if (available_disks[i])
        {
            // std::cerr << "[DEBUG] " << " available disk_id: " << i << std::endl;
            while (!disk_requests[i].empty())
            {
                // std::cerr << "[DEBUG] " << " disk_requests[i].size(): " << disk_requests[i].size() << std::endl;
                int current_request = disk_requests[i].top();
                disk_requests[i].pop();
                if (request[current_request].is_done) continue;
                if (request[current_request].disk_id != -1) continue;
                disk_head[i].current_request = current_request;
                request[current_request].disk_id = i;
                available_disks[i] = false; //标记磁盘为忙碌
                // std::cerr << "[DEBUG] " <<request[current_request].disk_id << " ok current_request: " << current_request << std::endl;
                break;
            }
        }
    }

    for (int i = 1; i <= N; i++) 
    {
        if (available_disks[i]) continue;//说明磁盘空闲，不进行读取
        int target_disk = i;//获取当前磁盘
        int current_request = disk_head[target_disk].current_request;//获取当前磁盘要处理的请求
        int object_id = request[current_request].object_id;
        // std::cerr << " target_disk: " << target_disk <<" is processing " <<current_request<< " " <<disk_head[target_disk].current_phase<< "/" << object[object_id].size << " " << request[current_request].disk_id << std::endl;
        do_object_read(object_id, target_disk, head_movement[target_disk]); // 执行对象读取操作
        if(disk_head[target_disk].current_phase != object[object_id].size) { // 如果当前阶段未达到对象大小
            // std::cerr << " target_disk: " << target_disk <<" is not finished" << std::endl;
            continue;
        }

        if (object[object_id].is_delete) { // 如果对象被删除
            // std::cerr << " target_disk: " << target_disk << " object_id: " << object_id << " is deleted" << std::endl;
            reset_disk_head(target_disk); // 重置磁头
            continue;
        }

        auto *active_phases = &object[object_id].active_phases; // 获取活动阶段队列
        // std::cerr <<" object_id: " << object_id << " active_phases: " << active_phases->empty() <<std::endl;
        while (!active_phases->empty() && active_phases->front() <= current_request) { // 移除已完成的请求
            finished_requests.push_back(active_phases->front()); // 记录已完成的请求
            request[active_phases->front()].is_done = true; // 标记请求为完成
            active_phases->pop(); // 从队列中移除请求
        }
        request[current_request].is_done = true; // 标记当前请求为完成
        // std::cerr << " target_disk: " << target_disk << " object_id: " << object_id << " is finished" << std::endl;

        current_request = 0; // 重置当前请求
        reset_disk_head(target_disk); // 重置磁头
    }
    for (int i=1; i<=N; i++) {
        if (head_movement[i].empty()) {
            head_movement[i] = "#\n";
        }
    }
    for (int i=1; i<=N; i++) {
            // std::cerr << "[DEBUG] " << " head_movement[" << i << "]: " << head_movement[i];
    }
    
    for (int i = 1; i <= N; i++) {
            printf("%s", head_movement[i].c_str()); // 输出磁头移动记录
            // std::cerr << "[OUTPUT] " << head_movement[i];
    }
    int fsize = finished_requests.size(); // 获取已完成请求的数量

    // std::cerr << "[DEBUG] " << " ******* "<< fsize << std::endl;


    printf("%d\n", fsize); // 输出已完成请求的数量
    // std::cerr << "[OUTPUT] " << fsize << std::endl;
    for (int i = 0; i < fsize; i++) {
        // std::cerr << "[DEBUG] " << " finished_requests[" << i << "]: " << finished_requests[i] << std::endl;
        printf("%d\n", finished_requests[i]); // 输出已完成请求的 ID
        // std::cerr << "[OUTPUT] " << finished_requests[i] << std::endl;
    }
    fflush(stdout); // 刷新输出缓冲区
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

// 主函数
int main()
{
    freopen("log.txt", "w", stderr); // 将调试输出重定向到 log.txt

    scanf("%d%d%d%d%d", &T, &M, &N, &V, &G); // 读取参数

    for(int i=1; i<=N; i++) { // 初始化磁头位置和当前阶段
        disk_head[i].pos = 1; 
        disk_head[i].current_phase = 0;
    }

    // 读取每个标签的删除请求数量
    for (int i = 1; i <= M; i++) {
        for (int j = 1; j <= (T - 1) / FRE_PER_SLICING + 1; j++) {
            scanf("%*d");
        }
    }

    // 读取每个标签的写请求数量
    for (int i = 1; i <= M; i++) {
        for (int j = 1; j <= (T - 1) / FRE_PER_SLICING + 1; j++) {
            scanf("%*d");
        }
    }

    // 读取每个标签的读请求数量
    for (int i = 1; i <= M; i++) {
        for (int j = 1; j <= (T - 1) / FRE_PER_SLICING + 1; j++) {
            scanf("%*d");
        }
    }

    printf("OK\n"); // 输出 OK
    // std::cerr << "[OUTPUT] " << "OK" << std::endl;
    fflush(stdout); // 刷新输出缓冲区

    for (int i = 1; i <= N; i++) {
        disk_point[i] = 1; // 初始化每个硬盘的磁头初始位置
        reset_disk_head(i); // 重置磁头
    }

    // 主循环，处理时间片
    for (int t = 1; t <= T + EXTRA_TIME; t++) {
        timestamp_action(); // 处理时间戳
        delete_action(); // 处理删除请求
        write_action(); // 处理写请求
        read_action(); // 处理读请求
    }
    clean(); // 清理资源

    return 0; // 返回 0，表示程序正常结束
}