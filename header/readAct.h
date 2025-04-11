#ifndef READACT_H
#define READACT_H

#include "definition.h"
#include <utility>
#include <vector>

/**
 * 更新object_id_set中所有对象的磁盘set，需要支持cnt_request增加、减小。
 * @param object_id_set 记录需要修改的object的id的集合（使用set自动去重）
 */
void reset_disk_cnt(const std::set<int> &object_id_set);

/**
 * 更新object_id_set中所有对象的磁盘set，需要支持cnt_request增加、减小。
 * @param object_id_set 记录需要修改的object的id的集合（使用set自动去重）
 */
void update_disk_cnt(const std::set<int> &object_id_set);

//遍历所有object, 找出当前磁盘上，cnt_request最大的object
std::pair<int, int> find_max_cnt_request_object(int disk_id);

/**
 *  决策disk_id这块硬盘是否需要进行jump，以及决策首地址。
 * @param disk_id 磁盘编号
 * @return first:表示是否jump;
 * second表示要移动到的位置。特别的，-1表示该磁头无任何操作。
 */
std::pair<int, int> jump_decision(int disk_id, int head_id);

/**
 * 使用动态规划求磁盘disk_id在tokens个令牌内的最优行动序列，优化目标是尽可能走得远
 * @param disk_id 磁盘编号
 * @param tokens 剩余的tokens数量
 * @return 最优的行动序列
 */
std::string dp_plan(int disk_id, int tokens, int head_id);

/**
 * 判断可能完成的对象上的请求是否完成
 * @param set 可能完成读入的对象编号
 * @param finished_request 完成的对象请求，引用
 * @param changed_objects 存在被完成请求的对象集合
 */
void judge_request_on_objects(const std::set<int> &set, std::vector<int> &finished_request, std::set<int> &changed_objects);

/**
 * 对于给定磁盘编号，处理当前时间片的操作
 * @param disk_id 磁盘编号
 * @param actions 记录磁头移动的字符串
 * @param finished_request 记录已经完成的请求
 */
std::set<int> solve_disk(int disk_id, std::string &actions, std::vector<int> &finished_request, int head_id);

/**
 * 设置请求信息
 * @param request_id 请求编号
 * @param object_id 对象编号
 */
void set_request_info(int request_id, int object_id);

/**
 * 清理超时请求
 */
void clean_timeout_request();

/**
 * 更新有价值的块数
 */
void update_valuable_block_num();

/**
 * 处理读入操作
 */
void read_action();

/**
 * 初始化磁头
 */
void init_disk_head();

#endif

