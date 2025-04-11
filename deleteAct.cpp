#include "header/deleteAct.h"

/**
 * 删除对象在disk_id这块磁盘上的数据（obj_id 与 block_id）。
 * @param object_unit 对象的存储单元下标
 * @param disk_id 磁盘编号
 * @param size 对象大小
 */
void do_object_delete(const int *object_unit, const int disk_id, int size) {
    for (int i = 1; i <= size; i++) {
        disk_obj_id[disk_id][object_unit[i]] = 0;   // 清空磁盘obj_id
        disk_block_id[disk_id][object_unit[i]] = 0; // 清空磁盘 block_id
        
    }
}

/**
 * 删除操作
 */
void delete_action() {
    int n_delete;                   // 要删除的请求数量
    int abort_num = 0;              // 记录未完成请求的数量
    static int _id[MAX_OBJECT_NUM]; // 存储待删除对象的 ID

    scanf("%d", &n_delete); // 读取删除请求数量
    for (int i = 1; i <= n_delete; i++) {
        scanf("%d", &_id[i]); // 读取每个删除请求的 ID
    }

    // 检查每个请求是否有未完成的请求
    //    for (int i = 1; i <= n_delete; i++) {
    //        int id = _id[i];
    //        int current_id = object[id].last_request_point; //
    //        获取对象的最后请求指针 while (current_id != 0) {
    //            if (request[current_id].is_done == false) {
    //                abort_num++; // 如果请求未完成，增加未完成请求计数
    //            }
    //            current_id = request[current_id].prev_id; // 移动到前一个请求
    //        }
    //    }
    for (int i = 1; i <= n_delete; i++) {
        int id = _id[i];
        abort_num += object[id].cnt_request;
        // abort_num += object[id].deleted_phases.size();
    }

//    std::set<int> object_id_set;

    printf("%d\n", abort_num); // 打印未完成请求的数量
    for (int i = 1; i <= n_delete; i++) {
        int id = _id[i];

//        if (!object[id].active_phases.empty()) {
//            object_id_set.insert(id);
//        }
//        int *replica = object[id].replica;
        while (!object[id].active_phases.empty()) {
            int current_id = object[id].active_phases.front();
            object[id].active_phases.pop_front();
            // if (!request[current_id].is_done) { // 这里应该总是可以删除的
                printf("%d\n", current_id);     // 打印未完成请求的 ID
                request[current_id].is_done = true; // 标记请求为已完成，方便从global_requests里删除
            // }
            for (int j = 1; j <= REP_NUM; j++) {
                di[object[id].replica[j]].cnt_request --;
            }
        }

        // while (!object[id].deleted_phases.empty()) {
        //     int current_id = object[id].deleted_phases.front();
        //     object[id].deleted_phases.pop();
        //     request[current_id].is_done = true; //出于一致性考虑，这里也标记为已完成
        //     // printf("%d\n", current_id);     // 打印未完成请求的 ID
        // }

        // 删除对象的副本
        for (int j = 1; j <= REP_NUM; j++) {
            // do_object_delete(object[id].unit[j],
            // disk_obj_id[object[id].replica[j]], disk_block_id[]
            // ,object[id].size);
            do_object_delete(object[id].unit[j], object[id].replica[j],
                             object[id].size);
            di[object[id].replica[j]].tag_num[object[id].tag]--;
        }
        object[id].cnt_request = 0;
        object[id].is_delete = true; // 标记对象为已删除
    }

    fflush(stdout);                 // 刷新输出缓冲区
}

