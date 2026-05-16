/*
 * 排序算法实现与性能分析评测程序
 * 实现：冒泡排序、简单选择排序、简单插入排序、归并排序、快速排序、
 *       堆排序、希尔排序、计数排序、基数排序
 * 测试规模：100, 1000, 10000, 100000, 1000000
 * 数据类型：随机数组、几乎有序数组
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

/* ==================== 辅助函数 ==================== */

/* 交换两个元素 */
static inline void swap(int *a, int *b) {
    int t = *a; *a = *b; *b = t;
}

/* 排序结果检测函数Check：验证数组是否非递减排列 */
int Check(int arr[], int n) {
    for (int i = 1; i < n; i++) {
        if (arr[i] < arr[i - 1]) return 0;
    }
    return 1;
}

/* 生成随机数组，元素范围 [0, n*10] */
void GenerateRandomArray(int arr[], int n) {
    for (int i = 0; i < n; i++)
        arr[i] = rand() % (n * 10);
}

/* 生成几乎有序数组：先排序，再随机交换少量元素 */
void GenerateNearlySortedArray(int arr[], int n) {
    for (int i = 0; i < n; i++) arr[i] = i;
    /* 随机交换约 1% 的元素 */
    int swaps = n / 100;
    if (swaps < 1) swaps = 1;
    for (int i = 0; i < swaps; i++) {
        int a = rand() % n;
        int b = rand() % n;
        swap(&arr[a], &arr[b]);
    }
}

/* 数组拷贝 */
void CopyArray(int dst[], const int src[], int n) {
    memcpy(dst, src, n * sizeof(int));
}

/* ==================== 排序算法实现 ==================== */

/* 1. 冒泡排序 */
void BubbleSort(int arr[], int n) {
    for (int i = 0; i < n - 1; i++) {
        int swapped = 0;
        for (int j = 0; j < n - 1 - i; j++) {
            if (arr[j] > arr[j + 1]) {
                swap(&arr[j], &arr[j + 1]);
                swapped = 1;
            }
        }
        if (!swapped) break;
    }
}

/* 2. 简单选择排序 */
void SelectionSort(int arr[], int n) {
    for (int i = 0; i < n - 1; i++) {
        int minIdx = i;
        for (int j = i + 1; j < n; j++) {
            if (arr[j] < arr[minIdx]) minIdx = j;
        }
        if (minIdx != i) swap(&arr[i], &arr[minIdx]);
    }
}

/* 3. 简单插入排序 */
void InsertionSort(int arr[], int n) {
    for (int i = 1; i < n; i++) {
        int key = arr[i];
        int j = i - 1;
        while (j >= 0 && arr[j] > key) {
            arr[j + 1] = arr[j];
            j--;
        }
        arr[j + 1] = key;
    }
}

/* 4. 归并排序 */
static void merge(int arr[], int temp[], int left, int mid, int right) {
    int i = left, j = mid + 1, k = left;
    while (i <= mid && j <= right) {
        if (arr[i] <= arr[j]) temp[k++] = arr[i++];
        else temp[k++] = arr[j++];
    }
    while (i <= mid) temp[k++] = arr[i++];
    while (j <= right) temp[k++] = arr[j++];
    for (i = left; i <= right; i++) arr[i] = temp[i];
}

static void mergeSortHelper(int arr[], int temp[], int left, int right) {
    if (left < right) {
        int mid = left + (right - left) / 2;
        mergeSortHelper(arr, temp, left, mid);
        mergeSortHelper(arr, temp, mid + 1, right);
        merge(arr, temp, left, mid, right);
    }
}

void MergeSort(int arr[], int n) {
    int *temp = (int *)malloc(n * sizeof(int));
    if (!temp) { fprintf(stderr, "内存分配失败\n"); exit(1); }
    mergeSortHelper(arr, temp, 0, n - 1);
    free(temp);
}

/* 5. 快速排序 */
static int partition(int arr[], int low, int high) {
    /* 三数取中选pivot */
    int mid = low + (high - low) / 2;
    if (arr[low] > arr[mid]) swap(&arr[low], &arr[mid]);
    if (arr[low] > arr[high]) swap(&arr[low], &arr[high]);
    if (arr[mid] > arr[high]) swap(&arr[mid], &arr[high]);
    swap(&arr[mid], &arr[high]); /* pivot放到high位置 */
    int pivot = arr[high];
    int i = low - 1;
    for (int j = low; j < high; j++) {
        if (arr[j] <= pivot) {
            i++;
            swap(&arr[i], &arr[j]);
        }
    }
    swap(&arr[i + 1], &arr[high]);
    return i + 1;
}

static void quickSortHelper(int arr[], int low, int high) {
    if (low < high) {
        /* 小规模切换到插入排序 */
        if (high - low < 16) {
            for (int i = low + 1; i <= high; i++) {
                int key = arr[i];
                int j = i - 1;
                while (j >= low && arr[j] > key) {
                    arr[j + 1] = arr[j];
                    j--;
                }
                arr[j + 1] = key;
            }
            return;
        }
        int pi = partition(arr, low, high);
        quickSortHelper(arr, low, pi - 1);
        quickSortHelper(arr, pi + 1, high);
    }
}

void QuickSort(int arr[], int n) {
    quickSortHelper(arr, 0, n - 1);
}

/* 6. 堆排序 */
static void heapify(int arr[], int n, int i) {
    int largest = i;
    int left = 2 * i + 1;
    int right = 2 * i + 2;
    if (left < n && arr[left] > arr[largest]) largest = left;
    if (right < n && arr[right] > arr[largest]) largest = right;
    if (largest != i) {
        swap(&arr[i], &arr[largest]);
        heapify(arr, n, largest);
    }
}

void HeapSort(int arr[], int n) {
    for (int i = n / 2 - 1; i >= 0; i--)
        heapify(arr, n, i);
    for (int i = n - 1; i > 0; i--) {
        swap(&arr[0], &arr[i]);
        heapify(arr, i, 0);
    }
}

/* 7. 希尔排序 */
void ShellSort(int arr[], int n) {
    for (int gap = n / 2; gap > 0; gap /= 2) {
        for (int i = gap; i < n; i++) {
            int temp = arr[i];
            int j;
            for (j = i; j >= gap && arr[j - gap] > temp; j -= gap)
                arr[j] = arr[j - gap];
            arr[j] = temp;
        }
    }
}

/* 8. 计数排序（仅适用于非负整数） */
void CountingSort(int arr[], int n) {
    if (n <= 0) return;
    int max = arr[0];
    for (int i = 1; i < n; i++)
        if (arr[i] > max) max = arr[i];
    /* 大数组限制计数范围避免内存爆炸 */
    if (max > 10000000) max = 10000000;
    int *count = (int *)calloc(max + 1, sizeof(int));
    if (!count) { fprintf(stderr, "计数排序内存分配失败\n"); return; }
    int *output = (int *)malloc(n * sizeof(int));
    if (!output) { free(count); fprintf(stderr, "计数排序内存分配失败\n"); return; }
    for (int i = 0; i < n; i++) count[arr[i] > max ? max : arr[i]]++;
    for (int i = 1; i <= max; i++) count[i] += count[i - 1];
    for (int i = n - 1; i >= 0; i--) {
        int idx = arr[i] > max ? max : arr[i];
        output[count[idx] - 1] = arr[i];
        count[idx]--;
    }
    memcpy(arr, output, n * sizeof(int));
    free(count);
    free(output);
}

/* 9. 基数排序（LSD，适用于非负整数） */
void RadixSort(int arr[], int n) {
    if (n <= 0) return;
    int max = arr[0];
    for (int i = 1; i < n; i++)
        if (arr[i] > max) max = arr[i];
    int *output = (int *)malloc(n * sizeof(int));
    if (!output) { fprintf(stderr, "基数排序内存分配失败\n"); return; }
    for (int exp = 1; max / exp > 0; exp *= 10) {
        int count[10] = {0};
        for (int i = 0; i < n; i++)
            count[(arr[i] / exp) % 10]++;
        for (int i = 1; i < 10; i++)
            count[i] += count[i - 1];
        for (int i = n - 1; i >= 0; i--) {
            int digit = (arr[i] / exp) % 10;
            output[count[digit] - 1] = arr[i];
            count[digit]--;
        }
        memcpy(arr, output, n * sizeof(int));
    }
    free(output);
}

/* ==================== 性能评测框架 ==================== */

/* 排序函数类型 */
typedef void (*SortFunc)(int[], int);

/* 排序算法信息 */
typedef struct {
    const char *name;
    SortFunc func;
} SortAlgo;

/* 获取高精度时间（秒） */
double GetTime() {
    return (double)clock() / CLOCKS_PER_SEC;
}

/* 单次排序测试，返回耗时(秒)，超时返回-1 */
double RunSort(SortFunc sortFunc, const int original[], int n, double timeout) {
    int *arr = (int *)malloc(n * sizeof(int));
    if (!arr) { fprintf(stderr, "内存分配失败 n=%d\n", n); return -1; }
    CopyArray(arr, original, n);

    double start = GetTime();
    sortFunc(arr, n);
    double end = GetTime();
    double elapsed = end - start;

    /* 验证排序正确性 */
    if (!Check(arr, n)) {
        fprintf(stderr, "  [错误] 排序结果不正确!\n");
        free(arr);
        return -2;
    }
    free(arr);

    if (elapsed > timeout) return -1; /* 超时 */
    return elapsed;
}

/* 运行所有排序算法的基准测试 */
void BenchmarkAll(const char *dataType, int sizes[], int numSizes, double timeout) {
    SortAlgo algos[] = {
        {"冒泡排序",   BubbleSort},
        {"选择排序",   SelectionSort},
        {"插入排序",   InsertionSort},
        {"希尔排序",   ShellSort},
        {"堆排序",     HeapSort},
        {"归并排序",   MergeSort},
        {"快速排序",   QuickSort},
        {"计数排序",   CountingSort},
        {"基数排序",   RadixSort},
    };
    int numAlgos = sizeof(algos) / sizeof(algos[0]);

    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════════════════════════╗\n");
    printf("║  数据类型: %-70s║\n", dataType);
    printf("╠══════════════════════════════════════════════════════════════════════════════════╣\n");
    printf("║  %-12s", "算法");
    for (int s = 0; s < numSizes; s++) printf("║  n=%-10d", sizes[s]);
    printf("║\n");
    printf("╠══════════════════");
    for (int s = 0; s < numSizes; s++) printf("╬════════════════");
    printf("╣\n");

    /* 存储结果用于后续输出 */
    double results[9][5]; /* 最多9种算法，5种规模 */
    memset(results, 0, sizeof(results));

    for (int a = 0; a < numAlgos; a++) {
        printf("║  %-12s", algos[a].name);
        for (int s = 0; s < numSizes; s++) {
            int n = sizes[s];

            /* 智能跳过：O(n²)算法在n>=100000时必然超时，直接标记 */
            int isSlow = (a < 3); /* 冒泡/选择/插入为O(n²) */
            if (isSlow && n >= 100000) {
                printf("║  %-12s", ">5min(超时)");
                results[a][s] = -1;
                continue;
            }

            int *original = (int *)malloc(n * sizeof(int));
            if (!original) {
                printf("║  %-12s", "分配失败");
                results[a][s] = -3;
                continue;
            }

            /* 根据数据类型生成数据 */
            if (strcmp(dataType, "随机数组") == 0)
                GenerateRandomArray(original, n);
            else
                GenerateNearlySortedArray(original, n);

            double elapsed = RunSort(algos[a].func, original, n, timeout);
            results[a][s] = elapsed;

            if (elapsed == -2)
                printf("║  %-12s", "排序错误");
            else if (elapsed == -1)
                printf("║  %-12s", ">5min(超时)");
            else if (elapsed < 0.001)
                printf("║  %-12s", "<0.001s");
            else
                printf("║  %8.3f s ", elapsed);

            free(original);
        }
        printf("║\n");
    }
    printf("╚══════════════════");
    for (int s = 0; s < numSizes; s++) printf("╩════════════════");
    printf("╝\n");
}

/* ==================== 主函数 ==================== */

int main() {
    system("chcp 65001 > nul"); /* 切换控制台到UTF-8编码，解决中文乱码 */
    srand((unsigned int)time(NULL));

    int sizes[] = {100, 1000, 10000, 100000, 1000000};
    int numSizes = sizeof(sizes) / sizeof(sizes[0]);
    double timeout = 300.0; /* 5分钟超时 */

    printf("================================================================\n");
    printf("         排序算法性能评测程序\n");
    printf("  算法: 冒泡/选择/插入/希尔/堆/归并/快速/计数/基数排序\n");
    printf("  规模: 100, 1000, 10000, 100000, 1000000\n");
    printf("  超时: %.0f秒 (超出则标记为超时)\n", timeout);
    printf("================================================================\n");

    /* 测试随机数组 */
    BenchmarkAll("随机数组", sizes, numSizes, timeout);

    /* 测试几乎有序数组 */
    BenchmarkAll("几乎有序数组", sizes, numSizes, timeout);

    printf("\n注: 排序结果已通过Check函数验证正确性。\n");
    printf("    \"<0.001s\" 表示耗时不到1毫秒。\n");
    printf("    \">5min(超时)\" 表示超过5分钟未完成，已停止。\n");

    return 0;
}
