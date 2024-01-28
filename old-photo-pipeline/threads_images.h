#include <time.h>
#include <stdio.h>
#include <unistd.h>
#include <gd.h>
#include "image-lib.h"
#include <dirent.h>
#include <pthread.h>
#include <string.h>

/* the directories wher output files will be placed */
#define old_photo_PIPELINE "./Old_photo_PIPELINE/"
#define n_threads 4

typedef enum
{
    contrast,
    smooth,
    texture,
    sepia
} stages;

// struct image_info
typedef struct
{
    gdImagePtr imagem_transformada;
    char *image_name;
    struct timespec image_contrast;
    struct timespec image_sepia;
} transformed;

// struct information
typedef struct
{
    char **images;
    int n_images;
} information;

// struct thread_info
typedef struct
{
    int pipe_read;
    int pipe_write;
    char *extra;
    int n_images;

} thread_info;

// funcoes descritas no threads_images.c
int verifica_ficheiro(char *output);
void *thread_function(void *arg);
void *ficheiro_output_timing(transformed **final, struct timespec total_time, char *directory, int n_images, struct timespec start_time_total);
int count_images(FILE *input);

information *read_image_list_txt(char *directory);
void *thread_contrast(void *arg);
void *thread_smooth(void *arg);
void *thread_texture(void *arg);
void *thread_sepia(void *arg);