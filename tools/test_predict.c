/* 验证 num_discern_predict 在 MNIST 测试集上的准确率。
 * 读取 tools/export_mnist_test.py 导出的 test_imgs.bin / test_labels.bin，
 * 逐个推理并统计正确率。编译：
 *   gcc -O2 -o test_predict.exe tools/test_predict.c src/app/num_discern/num_discern_model.c \
 *       -Isrc/app/num_discern
 */
#include <stdio.h>
#include <stdlib.h>

#include "num_discern_model.h"

#define N 2000

int main(void)
{
    FILE* fimg = fopen("test_imgs.bin", "rb");
    FILE* flab = fopen("test_labels.bin", "rb");
    if (!fimg || !flab) {
        fprintf(stderr, "open test data failed\n");
        return 1;
    }

    float* x = (float*)malloc((size_t)N * ND_INPUT_SIZE * sizeof(float));
    unsigned char* y = (unsigned char*)malloc(N);
    fread(x, sizeof(float), (size_t)N * ND_INPUT_SIZE, fimg);
    fread(y, 1, N, flab);
    fclose(fimg);
    fclose(flab);

    int correct = 0;
    for (int i = 0; i < N; i++) {
        int pred = num_discern_predict(x + (size_t)i * ND_INPUT_SIZE, NULL);
        if (pred == (int)y[i]) {
            correct++;
        } else if (i < 20) {
            printf("img[%d] label=%d pred=%d\n", i, y[i], pred);
        }
    }

    printf("accuracy = %d / %d = %.2f%%\n", correct, N, 100.0 * correct / N);
    free(x);
    free(y);
    return 0;
}
