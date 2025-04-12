#include "header/writeAct.h"
#include "header/definition.h"

int fre_del[MAX_TAG_NUM][TAG_PHASE];   // 每个标签的每个阶段删除的对象大小
int fre_write[MAX_TAG_NUM][TAG_PHASE]; // 每个标签的每个阶段写入的对象大小
int fre_read[MAX_TAG_NUM][TAG_PHASE];  // 每个标签的每个阶段读取的对象大小

int tag_alloc_length[MAX_TAG_NUM]; // 每个标签的分配长度

HotTagAlloc hot_tag_alloc[MAX_TAG_NUM];
DiskInfo di[MAX_DISK_NUM];

int phase_G[TAG_PHASE + 1];

/**
 * 计算磁盘disk_id的最大连续空闲块长度
 * @param disk_id 磁盘编号
 * @return 最大连续空闲块长度
 */
int calculate_max_contiguous(int disk_id, int start, int length) {
    int max_len = 0, current_len = 0;
    for (int i = start; i != (start + length + V - 2) % V + 1; i = i % V + 1) {
        if (disk_obj_id[disk_id][i] == 0) {
            current_len++;
            max_len = std::max(max_len, current_len);
        } else {
            current_len = 0;
        }
    }
    // 环形处理：检查首尾连接的情况（例如末尾和开头连续）
    if (length == V && disk_obj_id[disk_id][start] == 0 && disk_obj_id[disk_id][(start + V - 2) % V + 1] == 0) {
        int head = start, tail = (start + V - 2) % V + 1;
        while (disk_obj_id[disk_id][head] == 0 && head != (start + V - 2) % V + 1)
            head = head % V + 1;
        while (disk_obj_id[disk_id][tail] == 0 && tail != start)
            tail = (tail + V - 2) % V + 1;
        max_len = std::max(max_len, ((start + V - 2) % V + 1 - tail + V) % V + (head - start + V) % V);
    }
    return max_len;
}

/**
 * 选择三个不同磁盘（优先选择连续空间大的）
 * @param id 对象编号
 * @return 选择的磁盘编号列表 std::vector<int>
 */
std::vector<int> select_disks_for_object(int id) {
    std::vector<std::pair<int, int>> disk_scores;
    std::vector<int> vis(N + 1);
    // 遍历所有磁盘，计算得分（连续空间 >= size的磁盘才有资格）
    int tag = object[id].tag;
    for (int i = 1; i <= REP_NUM; i++) {
        int target_hot_disk = hot_tag_alloc[tag].disk[i];
        vis[target_hot_disk] = 1;
        int contiguous = calculate_max_contiguous(target_hot_disk, hot_tag_alloc[tag].start[i], tag_alloc_length[tag]);
        int fixed_score = V * N;
        if (di[target_hot_disk]
                    .subhot_delete_tag[(timestamp - 1) / FRE_PER_SLICING + 1] ==
            tag) {
            if (di[target_hot_disk].distribute_length[tag] * 2 <
                tag_alloc_length[tag] / MAX_OBJECT_SIZE) {
                // fixed_score = 0;
            }
        }
        if (contiguous >= object[id].size) {
            disk_scores.emplace_back(fixed_score + contiguous, target_hot_disk);
        }
    }
    for (int i = 1; i <= N; i++) {
        if (vis[i])
            continue;
        int contiguous = calculate_max_contiguous(i, di[i].end_point, V - di[i].end_point + 1);
        if (contiguous >= object[id].size) {
            disk_scores.emplace_back(contiguous, i);
        }
    }
    for (int i = 1; i <= N; i++) {
        if (vis[i])
            continue;
        int contiguous = calculate_max_contiguous(i, 1, V);
        if (contiguous >= object[id].size) {
            disk_scores.emplace_back(-V * N + contiguous, i);
        }
    }
    // 按连续空间降序排序
    std::sort(disk_scores.rbegin(), disk_scores.rend());

    std::vector<int> selected;
    for (auto &[score, disk_id] : disk_scores) {
        if (selected.size() >= 3)
            break;
        if (std::find(selected.begin(), selected.end(), disk_id) ==
            selected.end()) {
            selected.push_back(disk_id);
        }
    }
    return selected;
}

/**
 * 在磁盘disk_id上分配size个连续块
 * @param disk_id 磁盘编号
 * @param size 对象大小
 * @param object_id 对象编号
 * @param reverse_blocks 是否翻转对象块
 * @return 返回分配的存储单元编号列表
 */
std::vector<int> allocate_contiguous_blocks(int disk_id, int size,
                                            int object_id,
                                            bool reverse_blocks) {
//    reverse_blocks = false;
    int tag = object[object_id].tag;
    int start = -1, rep_id = 0;
    for (int i = 1; i <= REP_NUM; i++) {
        if (hot_tag_alloc[tag].disk[i] == disk_id) {
            start = hot_tag_alloc[tag].start[i];
            rep_id = i;
            break;
        }
    }
    if (start == -1) {
        // start = disk_head[disk_id][head_id].pos;
    }

    /**
     * @brief 保存block到磁盘
     * @param pos block开始的位置
     * @return 保存的block序号
     */
    auto save_block = [&](int pos) {
        std::vector<int> blocks;
        for (int j = 0; j < size; j++) {
            int block_pos = (pos + j) % V;
            if (block_pos == 0)
                block_pos = V;
            blocks.push_back(block_pos);
            disk_obj_id[disk_id][block_pos] = object_id; // 填充对象编号
            disk_block_id[disk_id][block_pos] =
                    reverse_blocks ? size - j : j + 1; // 填充对象块编号
            di[disk_id].distribute_length[di[disk_id].disk_belong_tag[block_pos]]--;
        }
        if (reverse_blocks)
            std::reverse(blocks.begin(), blocks.end()); // 翻转块
        return blocks;
    };

    /**
     * @brief 检查pos位置是否有效
     * @param pos 位置
     * @return bool 是否有效
     */
    auto check_valid = [&](int pos) {
        for (int j = 0; j < size; j++) {
            int check_pos = (pos + j) % V;
            if (check_pos == 0)
                check_pos = V;
            if (disk_obj_id[disk_id][check_pos] != 0) {
                return false;
            }
        }
        return true;
    };

    if (rep_id != 0) {
        for (int i = 0; i < tag_alloc_length[tag]; i++) {
            int pos = (start + i) % V;
            if (pos == 0)
                pos = V;
            if (disk_obj_id[disk_id][pos] == 0) {
                if (check_valid(pos)) {
                    return save_block(pos);
                }
            }
        }
    }

    if (tag !=
        di[disk_id].subhot_read_tag[(timestamp - 1) / FRE_PER_SLICING + 1]) {
        start = di[disk_id].end_point;
        for (int i = 0; i < tag_alloc_length[tag]; i++) {
            int pos = (start + i) % V;
            if (pos == 0)
                pos = V;
            if (disk_obj_id[disk_id][pos] == 0) {
                if (check_valid(pos)) {
                    return save_block(pos);
                }
            }
        }
    }

    start = V - size + 1;

    for (int i = 0; i < V; i++) {
        int pos = (start - i + V) % V;
        if (pos == 0)
            pos = V;
        if (disk_obj_id[disk_id][pos] == 0) {
            if (check_valid(pos)) {
                return save_block(pos);
            }
        }
    }
    return {}; // 空间不足
}

/**
 * 写入对象的函数
 * @param object_unit 对象单元
 * @param disk_unit 磁盘单元
 * @param size 对象大小
 * @param object_id 对象编号
 */
void do_object_write(int *object_unit, int *disk_unit, int size,
                     int object_id) {
    int current_write_point = 0; // 当前写入位置
    for (int i = 1; i <= V; i++) {
        if (disk_unit[i] == 0) {                    // 如果磁盘单元为空
            disk_unit[i] = object_id;               // 写入对象 ID
            object_unit[++current_write_point] = i; // 记录写入位置
            if (current_write_point == size) {
                break; // 如果写入完成，退出
            }
        }
    }

    assert(current_write_point == size); // 确保写入的大小正确
}

/**
 * 写操作
 */
void write_action() {
    int n_write;           // 写请求数量
    scanf("%d", &n_write); // 读取写请求的数量
    for (int i = 1; i <= n_write; i++) {
        int id, size, tag;
        scanf("%d%d%d", &id, &size, &tag); // 读取对象 ID 和大小
        // std::cerr << "[DEBUG] " << " write_action: " << id << " size: " <<
        // size << " tag: " << tag << std::endl;
        object[id].tag = tag;              // 设置对象标签
        object[id].last_request_point = 0; // 初始化对象的最后请求指针
        object[id].size = size;            // 设置对象的大小
        object[id].is_delete = false;      // 标记对象为未删除
        object[id].cnt_request = 0;
        object[id].last_finish_time = -1;
        total_object_num++;
        std::vector<int> selected_disks = select_disks_for_object(id);
        for (int j = 1; j <= REP_NUM; j++) {
            int disk_id = selected_disks[j - 1];
            std::vector<int> blocks = allocate_contiguous_blocks(
                    disk_id, size, id, j & 1);   // 奇数翻转，偶数不变
            object[id].replica[j] = disk_id; // 计算副本的 ID
            object[id].unit[j] = static_cast<int *>(
                    malloc(sizeof(int) * (size + 1))); // 分配内存以存储对象数据
            for (int _ = 0; _ < size; _++) {
                object[id].unit[j][_ + 1] = blocks[_];
            }
            di[disk_id].tag_num[object[id].tag]++;
            // do_object_write(object[id].unit[j], disk[object[id].replica[j]],
            // size, id); // 将对象数据写入磁盘
        }

        printf("%d\n", id); // 打印对象 ID
        for (int j = 1; j <= REP_NUM; j++) {
            printf("%d", object[id].replica[j]); // 打印副本 ID
            // std::cerr << "[DEBUG] replica" <<j<< " in disk: " <<
            // object[id].replica[j] << " ";
            for (int k = 1; k <= size; k++) {
                printf(" %d", object[id].unit[j][k]); // 打印对象数据
                // std::cerr << "block" << k << ":" << object[id].unit[j][k] <<
                // " ";
            }
            // std::cerr << std::endl;
            printf("\n");
        }
    }

    fflush(stdout); // 刷新输出缓冲区
}

/**
 * 预处理标签
 */
void preprocess_tag() {
    // 读取每个标签的删除请求数量
    int slice_num = (T - 1) / FRE_PER_SLICING + 1;
    for (int i = 1; i <= M; i++) {
        for (int j = 1; j <= slice_num; j++) {
            scanf("%d", &fre_del[i][j]);
            fre_del[i][0] += fre_del[i][j];
        }
    }

    // 读取每个标签的写请求数量
    for (int i = 1; i <= M; i++) {
        for (int j = 1; j <= slice_num; j++) {
            scanf("%d", &fre_write[i][j]);
            fre_write[i][0] += fre_write[i][j];
        }
    }

    // 读取每个标签的读请求数量
    for (int i = 1; i <= M; i++) {
        for (int j = 1; j <= slice_num; j++) {
            scanf("%d", &fre_read[i][j]);
            fre_read[i][0] += fre_read[i][j];
        }
    }

    for (int i = 1; i <= ceil((T + 105.0) / FRE_PER_SLICING); i++) {
        scanf("%d", &phase_G[i]);
    }

    std::vector<int> tag_id;
    for (int i = 1; i <= M; i++) {
        tag_id.push_back(i);
        hot_tag_alloc[i].remain_alloc_num = 3;
    }

    std::sort(tag_id.begin(), tag_id.end(), [](int a, int b) {
        return fre_read[a][0] > fre_read[b][0];
    });

    std::vector<int> hot_tag;
    const int region_first_order = REP_NUM - 1;
    const int hot_tag_cnt = N / region_first_order;

    for (int i = 0; i < hot_tag_cnt; i++) {
        hot_tag.push_back(tag_id[i]);
        hot_tag_alloc[tag_id[i]].is_hot = 1;
    }

    std::sort(tag_id.begin(), tag_id.end(), [](int a, int b) {
        return fre_read[a][0] > fre_read[b][0];
    });

    std::vector<int> start_point(N + 1), top(N + 1);
    std::vector<std::set<int> > alloced(N + 1);
    std::vector<int> total_point(N + 1);

    int tot_replica_num = M * REP_NUM;

    for (int i = 1; i <= N; i++) {
        start_point[i] = 1;
    }

    auto update_disk = [&](int tag, int rep, int disk_id, int size) {
        for (int i = 1; i <= size; i++) {
            di[disk_id].disk_belong_tag[start_point[disk_id] + i - 1] = tag;
        }
        hot_tag_alloc[tag].disk[rep] = disk_id;
        hot_tag_alloc[tag].start[rep] = start_point[disk_id];
        tag_alloc_length[tag] = size;
        di[disk_id].distribute_length[tag] = tag_alloc_length[tag];
        start_point[disk_id] = std::min(V, start_point[disk_id] + tag_alloc_length[tag]);
        total_point[disk_id] += tag_alloc_length[tag];
        top[disk_id] = tag;
        hot_tag_alloc[tag].remain_alloc_num--;
        alloced[disk_id].insert(tag);
        tot_replica_num--;
    };

    int disk_id = 1;
    for (auto tag : hot_tag) {
        for (int i = 1; i <= region_first_order; i++) {
            int size = fre_write[tag][0] - fre_del[tag][0];
            if (tag == 2 || tag == 8) {
                size = size * 1.1;
            }
            update_disk(tag, i, disk_id, size);
            disk_id = disk_id % N + 1;
        }
    }


    std::priority_queue<std::pair<int, int>> current_space;
    for (int i = 1; i <= N; i++)
        current_space.emplace(V - start_point[i] + 1, i);

    for (auto i : tag_id) {
        if (!hot_tag_alloc[i].is_hot) {
            int size = fre_write[i][0] - fre_del[i][0];
            tag_alloc_length[i] = size;
            std::vector<std::pair<int, int>> selected_disk(REP_NUM + 1);
            for (int j = 1; j <= REP_NUM; j++) {
                auto it = current_space.top();
                selected_disk[j] = std::make_pair(it.first, it.second);
                current_space.pop();
            }
            for (int j = 1; j <= REP_NUM; j++) {
                int cur_disk_id = selected_disk[j].second;
                int cur_size = size * (1 + 0.05);
                current_space.emplace(selected_disk[j].first - cur_size,
                                      cur_disk_id);
                update_disk(i, j, cur_disk_id, cur_size);
            }
        }
    }

    for (auto i : hot_tag) {
        int size = fre_write[i][0] - fre_del[i][0];
        size = (int)(size * 1.02);
        tag_alloc_length[i] = size;
        std::vector<std::pair<int, int>> selected_disk(REP_NUM + 1);
        for (int j = region_first_order + 1; j <= REP_NUM; j++) {
            auto it = current_space.top();
            selected_disk[j] = std::make_pair(it.first, it.second);
            current_space.pop();
        }
        for (int j = region_first_order + 1; j <= REP_NUM; j++) {
            int cur_disk_id = selected_disk[j].second;
            current_space.emplace(selected_disk[j].first - size,
                                  cur_disk_id);
            update_disk(i, j, cur_disk_id, size);
        }
    }

    for (int i = 1; i <= N; i++) {
        di[i].end_point = start_point[i];
    }

    std::vector<std::vector<std::pair<int, int>>> disk_distribute_vector(
            N + 1, std::vector<std::pair<int, int>>());

    for (auto i : tag_id) {
        //    std::cerr << "[DEBUG] tag: " << i << " is_hot: " <<
        //    hot_tag_alloc[i].is_hot << std::endl;
        for (int j = 1; j <= REP_NUM; j++) {
            //    std::cerr << "[DEBUG]      rep #" << j << ": disk: " <<
            //    hot_tag_alloc[i].disk[j] << " start: " <<
            //    hot_tag_alloc[i].start[j] << std::endl;
            disk_distribute_vector[hot_tag_alloc[i].disk[j]].emplace_back(
                    hot_tag_alloc[i].start[j], i);
        }
    }

    // std::cerr << "[DEBUG] N = " << N << ", V = " << V << std::endl;

    for (int i = 1; i <= N; i++)
        std::sort(disk_distribute_vector.begin(),
                  disk_distribute_vector.end());

    for (int i = 1; i <= N; i++) {
        // std::cerr << "[DEBUG] disk: " << i << ":" << std::endl;
        int cnt = 0;
        for (auto [fi, se] : disk_distribute_vector[i]) {
            di[i].distribute[++cnt] = se;
            // std::cerr << "[DEBUG]      start: " << fi << "(" << se << ")" <<
            // std::endl;
        }
        di[i].tag_distinct_number = cnt;
    }

    for (int i = 1; i <= N; i++) {
        for (int j = 1; j <= (T - 1) / FRE_PER_SLICING + 1; j++) {
            int hot_read_tag_size = 0, hot_read_tag_id = 0;
            int hot_delete_tag_size = 0, hot_delete_tag_id = 0;
            int tag_cnt = di[i].tag_distinct_number;
            for (int k = 1; k <= tag_cnt; k++) {
                if (di[i].distribute[k] == 0)
                    continue;
                if (fre_read[di[i].distribute[k]][j] > hot_read_tag_size) {
                    hot_read_tag_size = fre_read[di[i].distribute[k]][j];
                    hot_read_tag_id = di[i].distribute[k];
                }
                if (fre_del[di[i].distribute[k]][j] > hot_delete_tag_size) {
                    hot_delete_tag_size = fre_del[di[i].distribute[k]][j];
                    hot_delete_tag_id = di[i].distribute[k];
                }
            }
            di[i].subhot_read_tag[j] = hot_read_tag_id;
            di[i].subhot_delete_tag[j] = hot_delete_tag_id;
        }
    }
}

