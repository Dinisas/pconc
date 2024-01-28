#include <gd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <string.h>

#include "image-lib.h"
#include "threads_images.h"

/**************************
 * main()
 *
 * Argumentos: (int) arg, (char**) argv -> inputs do utilizador
 *
 * Description: Inicializa todos os valores da struct images, cria threads e chama a thread_function de cada uma, faz a contagem dos tempos
 * sequencias, paralelos e totais do codigo de multi threading
 *************************/
int main(int argc, char **argv)
{
    struct timespec start_time_total, end_time_total;
    struct timespec start_time_seq, end_time_seq;
    struct timespec start_time_par, end_time_par;

    // inicializa a contagem do tempo sequencial e total
    clock_gettime(CLOCK_MONOTONIC, &start_time_total);
    clock_gettime(CLOCK_MONOTONIC, &start_time_seq);

    information *images;

    int pipe_fd[2];
    if (pipe(pipe_fd) == -1)
    {
        perror("Error creating pipe");
        exit(EXIT_FAILURE);
    }

    // verifica que o utilizador escreve 3 argumentos
    if (argc < 3)
    {
        printf("Error:missing filename in argument!\n");
        exit(EXIT_FAILURE);
    }

    // associa os argumentos inseridos a variaveis
    char *directory = argv[1];
    int n_threads = atoi(argv[2]);

    DIR *dir = opendir(directory);

    if (dir == NULL)
    {
        perror("Error opening directory");
        exit(EXIT_FAILURE);
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        if (strcmp(entry->d_name, "image-list.txt") == 0)
        { // Encontra o ficheiro image-list.txt e le-o
            images = read_image_list_txt(directory);
            break;
        }
    }

    if (images == NULL)
    {
        fprintf(stderr, "Error: image-list.txt not found in directory\n");
        exit(EXIT_FAILURE);
    }

    // cria as diretorias de output
    char full_path[50];
    snprintf(full_path, sizeof(full_path), "%s/%s", directory, old_photo_PAR_B);
    strcpy(images->path, full_path);
    if (create_directory(full_path) == 0)
    {
        fprintf(stderr, "Impossible to create %s directory\n", old_photo_PAR_B);
        exit(-1);
    }

    // le a imagem papel
    if (access("./paper-texture.png", F_OK) != -1)
    {
        images->papel = read_png_file("./paper-texture.png");
    }

    else
    {
        char path[50];
        snprintf(path, sizeof(path), "%s/%s", directory, "./paper-texture.png");
        images->papel = read_png_file(path);
    }

    images->directory = directory;

    // acaba a contagem do tempo sequencial e inicia a contagem do tempo paralelo
    clock_gettime(CLOCK_MONOTONIC, &end_time_seq);
    clock_gettime(CLOCK_MONOTONIC, &start_time_par);

    pthread_t thread_id[n_threads];
    thread_info threads[n_threads];
    // cria os threads
    for (int i = 0; i < n_threads; i++)
    {
        threads[i].thread_id = i;
        threads[i].images = images;
        threads[i].pipe_read = pipe_fd[0];
        pthread_create(&thread_id[i], NULL, thread_function, &threads[i]);
    }

    for (int k = 0; k < images->n_images; k++)
    {
        if (write(pipe_fd[1], &images->images[k], sizeof(char *)) == -1)
        {
            perror("Error writing to pipe");
            exit(EXIT_FAILURE);
        }
    }
    close(pipe_fd[1]);

    for (int j = 0; j < n_threads; j++)
    {
        pthread_join(thread_id[j], NULL);
    }

    // acaba a contagem dos tempos
    clock_gettime(CLOCK_MONOTONIC, &end_time_par);
    clock_gettime(CLOCK_MONOTONIC, &end_time_total);

    // calcula o tempo fazendo a diferenca entre o final e o inicial
    struct timespec par_time = diff_timespec(&end_time_par, &start_time_par);
    struct timespec seq_time = diff_timespec(&end_time_seq, &start_time_seq);
    struct timespec total_time = diff_timespec(&end_time_total, &start_time_total);

    // cria o ficheiro de output timing.txt
    ficheiro_output_timing(threads, total_time, n_threads);

    // imprime no terminal os tempos sequenciais, paralelos e totais
    printf("\tseq \t %10jd.%09ld\n", seq_time.tv_sec, seq_time.tv_nsec);
    printf("\tpar \t %10jd.%09ld\n", par_time.tv_sec, par_time.tv_nsec);
    printf("total \t %10jd.%09ld\n", total_time.tv_sec, total_time.tv_nsec);

    // da frees e destroi a imagem de papel
    imagens_processadas *current = threads->images->head;
    while (current != NULL)
    {
        imagens_processadas *temp = current;
        current = current->next;
        free(temp->image_name);
        free(temp);
    }

    gdImageDestroy(images->papel);
    for (int i = 0; i < images->n_images; i++)
    {
        free(images->images[i]);
    }
    free(images->images);
    free(images);

    // fecha a diretoria
    closedir(dir);

    exit(0);
}