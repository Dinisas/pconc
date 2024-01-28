#include <time.h>
#include <stdio.h>
#include <unistd.h>
#include <gd.h>
#include "image-lib.h"
#include <dirent.h>
#include <pthread.h>
#include <string.h>

/* the directories wher output files will be placed */
#define old_photo_PAR_B "./Old_photo_PAR_B/"

// struct imagens processadas
typedef struct imagens_processadas
{
    char *image_name;
    struct timespec start_process_time;
    struct timespec end_process_time;
    struct imagens_processadas *next;
} imagens_processadas;

// struct information
typedef struct
{
    imagens_processadas *head;
    char **images;
    int n_images;
    gdImagePtr papel;
    char path[50];
    char *directory;
} information;

// struct thread_info
typedef struct
{
    int thread_id;
    struct timespec start_time;
    struct timespec end_time;
    int n_imagens_processadas;
    information *images;
    int pipe_read;
} thread_info;

// funcoes descritas no threads_images.c
int verifica_ficheiro(char *output);
void *thread_function(void *arg);
void ficheiro_output_timing(thread_info *threads, struct timespec total_time, int n_threads);
int count_images(FILE *input);
information *read_image_list_txt(char *directory);