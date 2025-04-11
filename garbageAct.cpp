#include "header/garbageAct.h"

std::vector<std::pair<int, int> > find_swap(int disk_id, int&remain_swap_num) {
    std::vector<std::pair<int, int> > swap_list;
    std::vector<int> visited(V + 1, 0);
    for (int i = 1; i <= V; i++) {
    }
    return swap_list;
}

void garbage_collection_action() {
    scanf("%*s%*s");
    // std::cerr << "[DEBUG] garbage_collection_action: " << s << " " << t << std::endl;
    printf("GARBAGE COLLECTION\n");
    int remain_swap_num = K;
    for (int i = 1; i <= N; i++) {
        std::vector<std::pair<int, int> > swap_list = find_swap(i, remain_swap_num);
        int swap_list_size = (int)swap_list.size();
        printf("%d\n", swap_list_size);
        for (auto p : swap_list) {
            printf("%d %d\n", p.first, p.second);
        }
    }
    fflush(stdout);
}