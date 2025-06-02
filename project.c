/* imgproc.c
 * Универсальная консольная утилита обработки изображений на C
 *
 * Поддерживает:
 *   1. Медианный фильтр
 *   2. Гауссов фильтр
 *   3. Детекция границ
 *   4. Произвольная свёртка (Convolution)
 *   5. Градации серого + бинаризация по порогу
 *
 * Использует stb_image.h / stb_image_write.h для I/O.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>


#define pipi 3.14159265358979323846f

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

// Структура для хранения изображения (1, 3 или 4 канала, uint8_t)
typedef struct
{
    int width, height, channels;
    unsigned char *data;
} image_t;

//=== Вспомогательные функции ===

// «Отражающее» обрезание (reflect) границ
static inline int clamp_reflect(int v, int low, int high)
{
    if (v < low)
        return low + (low - v - 1);
    if (v > high)
        return high - (v - high - 1);
    return v;
}

// Сортировка вставками (для uint8_t)
static void vstavili(uint8_t *arr, int n)
{
    for (int i = 1; i < n; ++i)
    {
        uint8_t key = arr[i];
        int j = i - 1;
        while (j >= 0 && arr[j] > key)
        {
            arr[j + 1] = arr[j];
            j--;
        }
        arr[j + 1] = key;
    }
}

//=== Функции загрузки/сохранения/освобождения ===
int zagruzka(const char *fname, image_t *img)
{
    img->data = stbi_load(fname, &img->width, &img->height, &img->channels, 0);
    return img->data != NULL;
}

int save_image(const char *fname, const image_t *img)
{
    int w = img->width, h = img->height, c = img->channels;
    if (c == 1)
        return stbi_write_png(fname, w, h, 1, img->data, w);
    else if (c == 3)
        return stbi_write_png(fname, w, h, 3, img->data, w * 3);
    else // c == 4
        return stbi_write_png(fname, w, h, 4, img->data, w * 4);
}

void free_image(image_t *img)
{
    if (img->data)
        stbi_image_free(img->data);
    img->data = NULL;
}

//=== Реализация медианного фильтра на image_t без лишних проверок ===
void soap(const image_t *img, int ksize, image_t *out)
{
    int w = img->width;
    int h = img->height;
    int c = img->channels;
    int half = ksize / 2;
    int window_size = ksize * ksize;

    out->width = w;
    out->height = h;
    out->channels = c;
    out->data = malloc(w * h * c);

    uint8_t *window = malloc(window_size);

    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            for (int ch = 0; ch < c; ++ch)
            {
                int cnt = 0;
                for (int dy = -half; dy <= half; ++dy)
                {
                    int yy = clamp_reflect(y + dy, 0, h - 1);
                    for (int dx = -half; dx <= half; ++dx)
                    {
                        int xx = clamp_reflect(x + dx, 0, w - 1);
                        window[cnt++] = img->data[(yy * w + xx) * c + ch];
                    }
                }
                vstavili(window, window_size);
                out->data[(y * w + x) * c + ch] = window[window_size / 2];
            }
        }
    }

    free(window);
}

//=== Прочие функции (Gaussian, edge detection и т.д.), без проверок ===

static float *createGaussianKernel2D(int ksize, float sigma)
{
    int half = ksize / 2;
    float sum = 0.0f;
    float sigmasq = sigma * sigma;
    float coeff = 1.0f / (2.0f * pipi * sigmasq);

    float *kernel = malloc(sizeof(float) * ksize * ksize);

    for (int y = -half; y <= half; ++y)
    {
        for (int x = -half; x <= half; ++x)
        {
            int idx = (y + half) * ksize + (x + half);
            float exponent = -(x * (float)x + y * (float)y) / (2.0f * sigmasq);
            kernel[idx] = coeff * expf(exponent);
            sum += kernel[idx];
        }
    }
    for (int i = 0; i < ksize * ksize; ++i)
        kernel[i] /= sum;

    return kernel;
}

void convolve(const image_t *img, const float *kernel, int ksize, image_t *out)
{
    int w = img->width, h = img->height, c = img->channels, pad = ksize / 2;
    out->width = w;
    out->height = h;
    out->channels = c;
    out->data = malloc(w * h * c);

    for (int ch = 0; ch < c; ch++)
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++)
            {
                float sum = 0.0f;
                for (int ky = -pad; ky <= pad; ky++)
                    for (int kx = -pad; kx <= pad; kx++)
                    {
                        int ix = x + kx, iy = y + ky;
                        if (ix < 0)
                            ix = -ix;
                        if (iy < 0)
                            iy = -iy;
                        if (ix >= w)
                            ix = w - (ix - w) - 1;
                        if (iy >= h)
                            iy = h - (iy - h) - 1;
                        int idx = (iy * w + ix) * c + ch;
                        sum += kernel[(ky + pad) * ksize + (kx + pad)] * img->data[idx];
                    }
                int vidx = (y * w + x) * c + ch;
                int v = (int)(sum + 0.5f);
                if (v < 0)
                    v = 0;
                if (v > 255)
                    v = 255;
                out->data[vidx] = v;
            }
}

void detect_edges(const image_t *img, image_t *out)
{
    image_t gray, gx, gy;
    gray.width = img->width;
    gray.height = img->height;
    gray.channels = 1;
    gray.data = malloc(gray.width * gray.height);
    for (int i = 0, p = 0; i < img->width * img->height * img->channels; i += img->channels, p++)
    {
        float r = img->data[i];
        float g = img->data[i + 1];
        float b = img->data[i + 2];
        gray.data[p] = (unsigned char)(0.299f * r + 0.587f * g + 0.114f * b + 0.5f);
    }

    float kx[9] = {-1, 0, 1, -2, 0, 2, -1, 0, 1};
    float ky[9] = {-1, -2, -1, 0, 0, 0, 1, 2, 1};

    convolve(&gray, kx, 3, &gx);
    convolve(&gray, ky, 3, &gy);

    int w = gray.width, h = gray.height;
    out->width = w;
    out->height = h;
    out->channels = 1;
    out->data = malloc(w * h);
    for (int i = 0; i < w * h; i++)
    {
        int m = (int)(sqrtf(gx.data[i] * gx.data[i] + gy.data[i] * gy.data[i]) + 0.5f);
        out->data[i] = (m > 255) ? 255 : (unsigned char)m;
    }

    free_image(&gray);
    free_image(&gx);
    free_image(&gy);
}

void threshold_image(const image_t *img, unsigned char t, image_t *out)
{
    int w = img->width, h = img->height;
    out->width = w;
    out->height = h;
    out->channels = 1;
    out->data = malloc(w * h);
    for (int i = 0; i < w * h; i++)
        out->data[i] = (img->data[i] >= t) ? 255 : 0;
}

//=== main() ===
int main(int argc, char **argv)
{
    if (argc < 3)
    {
        printf("Usage:\n");
        printf("Write to terminal this text\n");
        printf(" 'project.exe' 1 path or name for <input.png> [kernel_size] (medianBlur (kernel_size odd >=3))\n", argv[0]);
        printf(" 'project.exe' 2 path or name for <input.png> <kernel_size> <sigma> (Gaussian blur (kernel_size odd >=1, sigma >0)\n", argv[0]);
        printf(" 'project.exe' 3 path or name for <input.png> (Edge detection (Sobel))\n", argv[0]);
        printf(" 'project.exe' 4 path or name for <input.png> [kernel_size] (Convolution with uniform kernel (normalized 1/(k*k)))\n", argv[0]);
        printf(" 'project.exe' 5 path or name for <input.png> [threshold] (Grayscale + threshold (threshold 0..255))\n", argv[0]);
        return 1;
    }

    int opt = atoi(argv[1]);
    const char *path = argv[2];
    int param = (argc >= 4) ? atoi(argv[3]) : 0;

    image_t img, tmp, out;

    if (opt == 1)
    {
        zagruzka(path, &img);
        
        int k = (param > 0) ? param : 3;

        if (k % 2 == 0 || k < 3)
        {
            printf("Kernel size for median must be odd and >=3\n");
            free_image(&img);
            return 1;
        }

        soap(&img, k, &out);

        save_image("median.png", &out);
        free_image(&img);
        free_image(&out);
    }
    else
    {
        zagruzka(path, &img);
        
        switch (opt)
        {
        
        case 2:
        {
            int k = atoi(argv[3]);
            float sigma = strtof(argv[4], NULL);
            float *ker = createGaussianKernel2D(k, sigma);
            
            convolve(&img, ker, k, &out);
            
            save_image("gauss.png", &out);
            
            free(ker);
            free_image(&out);
            free_image(&img);
            break; 
        }
        case 3:
            
            detect_edges(&img, &out);
            
            save_image("edges.png", &out);
            
            free_image(&out);
            free_image(&img);
            break;
        case 4:
        {
            int k = (param > 0) ? param : 3;
            int n = k * k;
            float *ker = malloc(sizeof(float) * n);
            
            for (int i = 0; i < n; i++)
            {
                ker[i] = 1.0f / n;
            }

            convolve(&img, ker, k, &out);

            save_image("conv.png", &out);
            
            free(ker);
            free_image(&out);
            free_image(&img);
            
            break;
        }
        case 5:
            tmp.width = img.width;
            tmp.height = img.height;
            tmp.channels = 1;
            tmp.data = malloc(tmp.width * tmp.height);
            for (int i = 0, p = 0; i < img.width * img.height * img.channels; i += img.channels, p++)
                tmp.data[p] = (unsigned char)(0.299f * img.data[i] + 0.587f * img.data[i + 1] + 0.114f * img.data[i + 2] + 0.5f);
            save_image("gray.png", &tmp);

            threshold_image(&tmp, (unsigned char)((param > 0) ? param : 128), &out);

            save_image("thresh.png", &out);

            free_image(&tmp);
            free_image(&out);
            free_image(&img);
            break;
        }
    }
    return 0;
}
