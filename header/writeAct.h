#ifndef WRITEACT_H
#define WRITEACT_H

#include "definition.h"

int calculate_max_contiguous(int disk_id, int start, int length);
std::vector<int> select_disks_for_object(int id);
std::vector<int> allocate_contiguous_blocks(int disk_id, int size, int object_id, bool reverse_blocks);
void do_object_write(int *object_unit, int *disk_unit, int size, int object_id);
void write_action();
void preprocess_tag();

#endif
