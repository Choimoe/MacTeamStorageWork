#include <cstdio>
#include <cassert>
#include <cstdlib>

#define MAX_DISK_NUM (10 + 1)
#define MAX_DISK_SIZE (16384 + 1)
#define MAX_REQUEST_NUM (30000000 + 1)
#define MAX_OBJECT_NUM (100000 + 1)
#define REP_NUM (3)
#define FRE_PER_SLICING (1800)
#define EXTRA_TIME (105)

typedef struct Request_ {
    int object_id;  // 对象的 ID
    int prev_id;    // 前一个请求的 ID
    bool is_done;   // 请求是否完成的标志
} Request;

typedef struct Object_ {
    int replica[REP_NUM + 1];  // 对象的副本 ID
    int* unit[REP_NUM + 1];     // 存储对象数据的指针数组
    int size;                   // 对象的大小
    int last_request_point;     // 最后一个请求的指针
    bool is_delete;             // 对象是否被标记为删除
} Object;

Request request[MAX_REQUEST_NUM];
Object object[MAX_OBJECT_NUM];

int T, M, N, V, G;
int disk[MAX_DISK_NUM][MAX_DISK_SIZE];
int disk_point[MAX_DISK_NUM];

void timestamp_action()
{
    int timestamp;
    scanf("%*s%d", &timestamp);
    printf("TIMESTAMP %d\n", timestamp);

    fflush(stdout);
}

void do_object_delete(const int* object_unit, int* disk_unit, int size)
{
    for (int i = 1; i <= size; i++) {
        disk_unit[object_unit[i]] = 0;
    }
}

void delete_action()
{
    int n_delete; // 要删除的请求数量
    int abort_num = 0; // 记录未完成请求的数量
    static int _id[MAX_OBJECT_NUM]; // 存储待删除对象的 ID

    scanf("%d", &n_delete); // 读取删除请求数量
    for (int i = 1; i <= n_delete; i++) {
        scanf("%d", &_id[i]); // 读取每个删除请求的 ID
    }

    // 检查每个请求是否有未完成的请求
    for (int i = 1; i <= n_delete; i++) {
        int id = _id[i];
        int current_id = object[id].last_request_point; // 获取对象的最后请求指针
        while (current_id != 0) {
            if (request[current_id].is_done == false) {
                abort_num++; // 如果请求未完成，增加未完成请求计数
            }
            current_id = request[current_id].prev_id; // 移动到前一个请求
        }
    }

    printf("%d\n", abort_num); // 打印未完成请求的数量
    for (int i = 1; i <= n_delete; i++) {
        int id = _id[i];
        int current_id = object[id].last_request_point; // 获取对象的最后请求指针
        while (current_id != 0) {
            if (request[current_id].is_done == false) {
                printf("%d\n", current_id); // 打印未完成请求的 ID
            }
            current_id = request[current_id].prev_id; // 移动到前一个请求
        }
        // 删除对象的副本
        for (int j = 1; j <= REP_NUM; j++) {
            do_object_delete(object[id].unit[j], disk[object[id].replica[j]], object[id].size);
        }
        object[id].is_delete = true; // 标记对象为已删除
    }

    fflush(stdout); // 刷新输出缓冲区
}

void do_object_write(int* object_unit, int* disk_unit, int size, int object_id)
{
    int current_write_point = 0;
    for (int i = 1; i <= V; i++) {//遍历每个存储单元
        if (disk_unit[i] == 0) {//如果存储单元为空
            disk_unit[i] = object_id;
            object_unit[++current_write_point] = i;//将存储单元的id存储到对象单元中
            if (current_write_point == size) {//如果对象单元满了
                break;
            }
        }
    }

    assert(current_write_point == size);
}

void write_action()
{
    int n_write; // 写请求数量
    scanf("%d", &n_write); // 读取写请求的数量
    for (int i = 1; i <= n_write; i++) {
        int id, size;
        scanf("%d%d%*d", &id, &size); // 读取对象 ID 和大小
        object[id].last_request_point = 0; // 初始化对象的最后请求指针
        for (int j = 1; j <= REP_NUM; j++) {
            object[id].replica[j] = (id + j) % N + 1; // 计算副本的 ID
            object[id].unit[j] = static_cast<int*>(malloc(sizeof(int) * (size + 1))); // 分配内存以存储对象数据
            object[id].size = size; // 设置对象的大小
            object[id].is_delete = false; // 标记对象为未删除
            do_object_write(object[id].unit[j], disk[object[id].replica[j]], size, id); // 将对象数据写入磁盘
        }

        printf("%d\n", id); // 打印对象 ID
        for (int j = 1; j <= REP_NUM; j++) {
            printf("%d", object[id].replica[j]); // 打印副本 ID
            for (int k = 1; k <= size; k++) {
                printf(" %d", object[id].unit[j][k]); // 打印对象数据
            }
            printf("\n");
        }
    }

    fflush(stdout); // 刷新输出缓冲区
}

void read_action()
{
    int n_read;
    int request_id, object_id;
    scanf("%d", &n_read);
    for (int i = 1; i <= n_read; i++) {
        scanf("%d%d", &request_id, &object_id);
        request[request_id].object_id = object_id;
        request[request_id].prev_id = object[object_id].last_request_point;
        object[object_id].last_request_point = request_id;
        request[request_id].is_done = false;
    }

    static int current_request = 0;
    static int current_phase = 0;
    if (!current_request && n_read > 0) {
        current_request = request_id;
    }
    if (!current_request) {
        for (int i = 1; i <= N; i++) {
            printf("#\n");
        }
        printf("0\n");
    } else {
        current_phase++;
        object_id = request[current_request].object_id;
        for (int i = 1; i <= N; i++) {
            if (i == object[object_id].replica[1]) {
                if (current_phase % 2 == 1) {
                    printf("j %d\n", object[object_id].unit[1][current_phase / 2 + 1]);
                } else {
                    printf("r#\n");
                }
            } else {
                printf("#\n");
            }
        }

        if (current_phase == object[object_id].size * 2) {
            if (object[object_id].is_delete) {
                printf("0\n");
            } else {
                printf("1\n%d\n", current_request);
                request[current_request].is_done = true;
            }
            current_request = 0;
            current_phase = 0;
        } else {
            printf("0\n");
        }
    }

    fflush(stdout);
}

void clean()
{
    for (auto& obj : object) {
        for (int i = 1; i <= REP_NUM; i++) {
            if (obj.unit[i] == nullptr)
                continue;
            free(obj.unit[i]);
            obj.unit[i] = nullptr;
        }
    }
}

int main()
{
    scanf("%d%d%d%d%d", &T, &M, &N, &V, &G);
    /*
    T个时间分片, 1<=T<=86400
    M个对象标签, 1<=M<=16
    N个硬盘，3<=N<=10
    V个存储单元，1<=V<=16384
    每个磁头在每个时间片最多消耗G个令牌，64<=G<=1000
    */
    for (int i = 1; i <= M; i++) {//每个标签的删除请求数量
        for (int j = 1; j <= (T - 1) / FRE_PER_SLICING + 1; j++) {
            scanf("%*d");
        }
    }

    for (int i = 1; i <= M; i++) {//每个标签的写请求数量
        for (int j = 1; j <= (T - 1) / FRE_PER_SLICING + 1; j++) {
            scanf("%*d");
        }
    }

    for (int i = 1; i <= M; i++) {//每个标签的读请求数量
        for (int j = 1; j <= (T - 1) / FRE_PER_SLICING + 1; j++) {
            scanf("%*d");
        }
    }

    printf("OK\n");
    fflush(stdout);

    for (int i = 1; i <= N; i++) {//每个硬盘的磁头初始位置
        disk_point[i] = 1;
    }

    for (int t = 1; t <= T + EXTRA_TIME; t++) {
        timestamp_action();
        delete_action();
        write_action();
        read_action();
    }
    clean();

    return 0;
}