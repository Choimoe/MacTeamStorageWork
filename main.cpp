#include "header/definition.h"
#include "header/readAct.h"
#include "header/writeAct.h"
#include "header/deleteAct.h"
#include "header/garbageAct.h"

Request request[MAX_REQUEST_NUM]; // 请求数组
Object object[MAX_OBJECT_NUM];    // 对象数组
DiskInfo di[MAX_DISK_NUM];
HotTagAlloc hot_tag_alloc[MAX_TAG_NUM];
DiskHead disk_head[MAX_DISK_NUM][MAX_DISK_HEAD_NUM + 1];


int T, M, N, V, G, K; // 时间片、对象标签、硬盘数量、存储单元、令牌数量、gc次数
int total_object_num;                           // 当前对象数量
int timestamp;                                  // 当前时间戳

int disk_obj_id[MAX_DISK_NUM][MAX_DISK_SIZE];   // 磁盘上存储的obj的id
int disk_block_id[MAX_DISK_NUM][MAX_DISK_SIZE]; // 磁盘上存储的obj的block的编号

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