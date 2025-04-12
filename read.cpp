#include "header/readAct.h"
#include "header/util.h"
#include <random>

const int cost[] = {0,  64, 52, 42, 34,
                    28, 23, 19, 16}; // 从0开始连续Read操作的代价序列

int dp[MAX_TOKEN_NUM][10];
int dp_path[MAX_TOKEN_NUM][10];
int time_vis[MAX_OBJECT_NUM][MAX_OBJECT_SIZE]; // 表示每个对象块最后一次被read的时间

DiskHead disk_head[MAX_DISK_NUM][HEAD_NUM];

std::queue<int> global_requestions; // 全局的请求队列，按照时间戳天然不降序
std::queue<int> timeout_request;

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

std::string dp_plan(int disk_id, int head_id, int tokens) {
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
                request[front].is_done = true; //这里标记为true实际上是告诉clean可以从全局请求队列里删除这个请求了
                finished_request.push_back(front);
                deque->pop_front();
                object[id].cnt_request--; // 修改对象的请求数量
                for (int j = 1; j <= REP_NUM; j++) {
                    di[object[id].replica[j]].cnt_request--;
                }
            } else {
                break;
            }
        }

        // auto *queue = &object[id].deleted_phases;
        // while (!queue->empty()) {
        //     int front = queue->front(); // 取出时间最早的请求
        //     if (request[front].time <= object[id].last_finish_time) {
        //         flag = true;
        //         request[front].is_done = true;
        //         finished_request.push_back(front);
        //         queue->pop();
        //     } else {
        //         break;
        //     }
        // }
        if (flag) {
            changed_objects.insert(id);
        }
    }
}

std::set<int> solve_disk(int disk_id, int head_id, std::string &actions,
                         std::vector<int> &finished_request) {
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

        auto s = dp_plan(disk_id, head_id,G + phase_G[((timestamp + 1799)/ 1800)]); // 使用dp计算最优操作序列，最优化目标位尽可能走得远

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
        judge_request_on_objects(obj_indices, finished_request,changed_objects); // 处理被修改过的对象上潜在的请求
//        std::cerr << "[DEBUG] finish judge_request_on_objects" << std::endl;

//        reset_disk_cnt(changed_objects);
//        update_disk_cnt(
//            changed_objects); // 修改request被完成对象的计数，并更新其set
    }
    return changed_objects;
}


void set_request_info(int request_id, int object_id) {
    request[request_id].object_id = object_id;
    request[request_id].prev_id = object[object_id].last_request_point;
    request[request_id].is_done = false;
    request[request_id].time = timestamp;

    object[object_id].last_request_point = request[request_id].time;
    object[object_id].active_phases.push_back(request_id);
    object[object_id].cnt_request++;

    global_requestions.push(request_id);

    for (int j = 1; j <= REP_NUM; j++) {
        di[object[object_id].replica[j]].cnt_request++;
    }
}

void clean_timeout_request(std::vector<int> &busy_requests) {
    //TODO:这个优化不一定和磁盘按照“有效块数量排序”兼容，没想清楚
    static const int time_limit = 95; //超参数，时间片数
    while (!global_requestions.empty()) {
        int request_id = global_requestions.front();
        if (request[request_id].is_done) {
            global_requestions.pop();
            continue;
        }
//        std::cerr<< "[DEBUG] front request id = " << request_id << " time = " << request[request_id].time << std::endl;
        if (timestamp - request[request_id].time > time_limit) {
            global_requestions.pop();
            int obj_id = request[request_id].object_id;


            int r_id = object[obj_id].active_phases.front();

            if (r_id != request_id) {
                std::cerr << "[DEBUG]" << "there is a bug in clean_timeout_request" << std::endl;
            }

            if (timestamp - request[r_id].time > time_limit) {
                object[obj_id].active_phases.pop_front();
                object[obj_id].cnt_request--;
                for (int j = 1; j <= REP_NUM; j++) {
                    di[object[obj_id].replica[j]].cnt_request--;
                }
                busy_requests.push_back(r_id);
            }
            // object[obj_id].deleted_phases.push(request_id);
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

int header_order = 1; // 1 - 正序，2 - 逆序

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

    std::string head_movement[N + 1][HEAD_NUM]; // 存储磁头移动记录
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

    header_order = 3 - header_order;

    std::set<int> changed_objects;
    for (int i = 1; i <= N; i++) {
        for (int j = 1; j <= 2; j++) {
            int target_head = j;
            if (header_order == 2) {
                target_head = 3 - j;
            }
            std::set<int> t = solve_disk(disk_id[i - 1], target_head, head_movement[disk_id[i - 1]][target_head], finished_request);
            changed_objects.insert(t.begin(), t.end());
        }

//        std::cerr << "[DEBUG] finish disk solve for i = : " << i << std::endl;
    }
//    update_disk_cnt(changed_objects);

    for (int i = 1; i <= N; i++) {
        for (int j = 1; j <= 2; j++)
            printf("%s", head_movement[i][j].c_str());
        // std::cerr << "[DEBUG] head_movement[" << i << "] = " << head_movement[i] << std::endl;
    }

    int finished_request_size = (int)finished_request.size();

    printf("%d\n", finished_request_size);

    for (int i = 0; i < finished_request_size; i++) {
        printf("%d\n", finished_request[i]);
    }

    std::vector<int> busy_requests;

    clean_timeout_request(busy_requests);
    // update_valuable_block_num();//28412380.1172

    int n_busy = (int)busy_requests.size();
    printf("%d\n", n_busy);

    for (int i = 1; i <= n_busy; i++) {
        printf("%d\n", busy_requests[i - 1]);
    }

    fflush(stdout);
}

void init_disk_head() {
    for (int i = 1; i <= N; i++) {           // 初始化磁头位置和当前阶段
        for (int j = 1; j <= 2; j++) {
            disk_head[i][j].pos = 1;
        }
    }
}