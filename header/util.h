#ifndef UTIL_H
#define UTIL_H

#include "definition.h"

int get_distance(int x, int y);
double calculate_request_time_score(int request_id);
double calculate_request_size_score(int request_id);
double calculate_request_score(int request_id);

#endif
