#ifndef READACT_H
#define READACT_H

#include "definition.h"
#include <utility>
#include <vector>

void reset_disk_cnt(const std::set<int> &object_id_set);
void update_disk_cnt(const std::set<int> &object_id_set);
std::pair<int, int> find_max_cnt_request_object(int disk_id);
std::vector<int> select_disks_for_object(int id);
std::vector<int> allocate_contiguous_blocks(int disk_id, int size, int object_id, bool reverse_blocks);
std::string dp_plan(int disk_id, int tokens, int head_id);
void judge_request_on_objects(const std::set<int> &set, std::vector<int> &finished_request, std::set<int> &changed_objects);
void set_request_info(int request_id, int object_id);
void clean_timeout_request();
void update_valuable_block_num();
void read_action();
#endif

