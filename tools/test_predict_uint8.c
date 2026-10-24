/* 验证 num_discern_uint8_predict 在 MNIST 测试集上的准确率。
 * 输入是 tools/quantize_mnist_mlp.py --export-test-data 导出的 uint8 位图：
 *   test_imgs_u8.bin   2000 x 784 uint8
 *   test_labels.bin    2000 uint8
 *
 * 编译（仓库根目录）：
 *   gcc -O2 -o .tmp/test_predict_uint8.exe tools/test_predict_uint8.c \
 *       src/app/num_discern_uint8/num_discern_uint8_model.c \
 *       -Isrc/app/num_discern_uint8
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "num_discern_uint8_model.h"

#define DEFAULT_N 2000

int main(int argc, char** argv)
{
    const char* img_path = (argc > 1) ? argv[1] : ".tmp/test_imgs_u8.bin";
    const char* lab_path = (argc > 2) ? argv[2] : ".tmp/test_labels.bin";
    int n = (argc > 3) ? atoi(argv[3]) : DEFAULT_N;

    FILE* fimg = fopen(img_path, "rb");
    FILE* flab = fopen(lab_path, "rb");
    if (!fimg || !flab) {
        fprintf(stderr, "open test data failed: %s / %s\n", img_path, lab_path);
        return 1;
    }

    uint8_t* x = (uint8_t*)malloc((size_t)n * Q8_INPUT_SIZE);
    uint8_t* y = (uint8_t*)malloc((size_t)n);
    if (!x || !y) {
        fprintf(stderr, "malloc failed\n");
        return 1;
    }

    size_t got_x = fread(x, 1, (size_t)n * Q8_INPUT_SIZE, fimg);
    size_t got_y = fread(y, 1, (size_t)n, flab);
    fclose(fimg);
    fclose(flab);
    if (got_x != (size_t)n * Q8_INPUT_SIZE || got_y != (size_t)n) {
        fprintf(stderr, "short read: x=%zu y=%zu\n", got_x, got_y);
        return 1;
    }

    int correct = 0;
    int shown = 0;
    for (int i = 0; i < n; i++) {
        int pred = num_discern_uint8_predict(x + (size_t)i * Q8_INPUT_SIZE);
        if (pred == (int)y[i]) {
            correct++;
        } else if (shown < 20) {
            printf("img[%d] label=%d pred=%d\n", i, y[i], pred);
            shown++;
        }
    }

    printf("uint8 accuracy = %d / %d = %.2f%%\n", correct, n, 100.0 * correct / n);
    free(x);
    free(y);
    return 0;
}
