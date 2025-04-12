#ifndef DEFINITION_H
#define DEFINITION_H

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
#include <deque>
#include <queue>
#include <set>
#include <utility>

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
#define HEAD_NUM (2 + 1)         //磁头数量

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

/**
 * @brief 热标签分配结构体
 * @param disk 磁盘 ID
 * @param start 开始位置
 * @param is_hot 是否为热标签
 * @param remain_alloc_num 剩余分配数量
 */
typedef struct HotTagAlloc_ {
    int disk[REP_NUM + 1];
    int start[REP_NUM + 1];
    int is_hot;

    int remain_alloc_num;
} HotTagAlloc;

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



// 全局变量
extern Request request[MAX_REQUEST_NUM]; // 请求数组
extern Object object[MAX_OBJECT_NUM];    // 对象数组
extern int total_object_num;

extern int T, M, N, V, G, K; // 时间片、对象标签、硬盘数量、存储单元、令牌数量
extern int disk_obj_id[MAX_DISK_NUM][MAX_DISK_SIZE];   // 磁盘上存储的obj的id
extern int disk_block_id[MAX_DISK_NUM][MAX_DISK_SIZE]; // 磁盘上存储的obj的block的编号
extern int timestamp;                                  // 当前时间戳

extern DiskHead disk_head[MAX_DISK_NUM][HEAD_NUM]; // 磁头状态数组
extern DiskInfo di[MAX_DISK_NUM];
extern HotTagAlloc hot_tag_alloc[MAX_TAG_NUM];

extern int phase_G[TAG_PHASE + 1];

#endif

