#include "header/util.h"

/**
 * 计算从x走到y需要的时间
 * @param x 起始位置
 * @param y 终止位置
 * @return 需要移动的距离
 */
int get_distance(int x, int y) {
    if (x <= y) {
        return y - x;
    } else {
        return V - x + y;
    }
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