#include "header/garbageAct.h"

// 判断是否为有效的交换对
bool valid_swap(int disk_id, int unit_a, int unit_b) {
    int is_same = disk_obj_id[disk_id][unit_a] != disk_obj_id[disk_id][unit_b];
    int is_null = disk_obj_id[disk_id][unit_a] == 0 || disk_obj_id[disk_id][unit_b] == 0;
    return is_same && !is_null;
}

std::vector<std::pair<int, int> > find_swap(int disk_id, int&remain_swap_num) {
    std::vector<std::pair<int, int> > swap_list;
    std::vector<int> visited(V + 1, 0);
    // for (int from = 1; from <= V; from++) {
    //     int belong_to_tag = di[disk_id].disk_belong_tag[from];
    //     for (int to = 1; to <= V; to++) {
    //         int to_belong_to_tag = di[disk_id].disk_belong_tag[to];
    //         if (!valid_swap(disk_id, from, to)) {
    //             continue;
    //         }
    //         if (visited[from] || visited[to]) {
    //             continue;
    //         }
    //         if (remain_swap_num <= 0) {
    //             return swap_list;
    //         }
    //         auto make_swap = [&](int a, int b) {
    //             swap_list.push_back(std::make_pair(a, b));
    //             remain_swap_num--;
    //             visited[a] = 1;
    //             visited[b] = 1;
    //         };
    //         if (disk_obj_id[disk_id][from] == 0) {
    //             if (to_belong_to_tag == belong_to_tag) {
    //                 make_swap(from, to);
    //                 break;
    //             }
    //         }
    //         if (disk_obj_id[disk_id][to] != 0) {
    //             int obj_b = disk_obj_id[disk_id][to];
    //             if (object[obj_b].tag == belong_to_tag) {
    //                 make_swap(from, to);
    //                 break;
    //             }
    //         }
    //     }
    // }
    return swap_list;
}

void garbage_collection_action() {
    scanf("%*s%*s");
    // std::cerr << "[DEBUG] garbage_collection_action: " << s << " " << t << std::endl;
    printf("GARBAGE COLLECTION\n");
    int remain_swap_num = K;
    for (int i = 1; i <= N; i++) {
        int disk_id = i;
        std::vector<std::pair<int, int> > swaps = find_swap(disk_id, remain_swap_num);
        printf("%lu\n", swaps.size());
        for (auto &swap : swaps) {
            int a = swap.first;
            int b = swap.second;

            int obj_a = disk_obj_id[disk_id][a], block_a = disk_block_id[disk_id][a];
            int obj_b = disk_obj_id[disk_id][b], block_b = disk_block_id[disk_id][b];

            int replica_a = -1, replica_b = -1;
            for (int j = 1; j <= REP_NUM; j++) {
                if (obj_a && disk_id == object[obj_a].replica[j]) replica_a = j;
                if (obj_b && disk_id == object[obj_b].replica[j]) replica_b = j;
            }

            std::swap(disk_obj_id[disk_id][a], disk_obj_id[disk_id][b]);
            std::swap(disk_block_id[disk_id][a], disk_block_id[disk_id][b]);

            if (obj_a && obj_b)
                std::swap(object[obj_a].unit[replica_a][block_a], object[obj_b].unit[replica_b][block_b]);
            else if (!obj_a && obj_b)
                object[obj_b].unit[replica_b][block_b] = a;
            else if (obj_a && !obj_b)
                object[obj_a].unit[replica_a][block_a] = b;

            printf("%d %d\n", a, b);
        }
    }

    fflush(stdout);
}