#include "threads_images.h"
/**************************
 * verifica_ficheiro()
 *
 * Argumentos: output - path de uma foto para a diretoria onde estao as fotos processadas
 *
 * Returns: (int) encontrado - verifica se a foto ja foi processada (devolve 1 ou 0)
 *
 * Description: Verifica se a foto ja foi processada, devolve 1 se ja estiver na pasta ou 0 se ainda nao estiver
 *
 *************************/

int verifica_ficheiro(char *output)
{
    int encontrado;
    if (access(output, F_OK) != -1)
    {
        printf("%s encontrado\n", output);
        encontrado = 1;
    }
    else
    {
        printf("%s nao encontrado\n", output);
        encontrado = 0;
    }
    return encontrado;
}

void *thread_function(void *arg)
{
    thread_info *threads = (thread_info *)arg;
    char *reference;
    threads->n_imagens_processadas = 0;

    // inicia a contagem do tempo deste thread
    clock_gettime(CLOCK_MONOTONIC, &threads->start_time);

    while (read(threads->pipe_read, &reference, sizeof(char *)) > 0)
    {
        char out_file_name[60];
        sprintf(out_file_name, "%s/%s", threads->images->path, reference);
        // testa se a foto ja foi processada
        int encontrado = verifica_ficheiro(out_file_name);

        // se nao tiver sido processada
        if (encontrado == 0)

        {
            imagens_processadas *first = malloc(sizeof(imagens_processadas));
            first->image_name = strdup(reference);
            printf("%s", first->image_name);

            clock_gettime(CLOCK_MONOTONIC, &first->start_process_time);
            char in_file_name[50];
            sprintf(in_file_name, "%s/%s", threads->images->directory, reference);

            printf("image %s\t thread %d\n", reference, threads->thread_id);
            /* load of the input file */
            gdImagePtr in_img = read_jpeg_file(in_file_name);
            if (in_img == NULL)
            {
                fprintf(stderr, "Impossible to read %s image\n", reference);
                continue;
            }
            // alternar a destruicao das imagens com o seu processamento para diminuir a memoria utilizada
            gdImagePtr out_contrast_img = contrast_image(in_img);
            gdImageDestroy(in_img);
            gdImagePtr out_smoothed_img = smooth_image(out_contrast_img);
            gdImageDestroy(out_contrast_img);
            gdImagePtr out_textured_img = texture_image(out_smoothed_img, threads->images->papel);
            gdImageDestroy(out_smoothed_img);
            gdImagePtr out_sepia_img = sepia_image(out_textured_img);
            gdImageDestroy(out_textured_img);

            // guarda a foto antiga final na pasta
            if (write_jpeg_file(out_sepia_img, out_file_name) == 0)
            {
                fprintf(stderr, "Impossible to write %s image\n", out_file_name);
            }
            gdImageDestroy(out_sepia_img);
            threads->n_imagens_processadas++;
            clock_gettime(CLOCK_MONOTONIC, &first->end_process_time);
            first->next = threads->images->head;
            threads->images->head = first;
        }
    }
    // acaba a contagem do tempo deste thread
    clock_gettime(CLOCK_MONOTONIC, &threads->end_time);
}

void ficheiro_output_timing(thread_info *threads, struct timespec total_time, int n_threads)
{
    FILE *input_saida;
    char filename_timing[100];
    // cria o path para o ficheiro
    snprintf(filename_timing, sizeof(filename_timing), "%s/timing_%d.txt", threads->images->directory, n_threads);

    // abre o ficheiro
    input_saida = fopen(filename_timing, "w");
    if (input_saida == NULL)
    {
        printf("Error: Unable to open file.\n");
        exit(EXIT_FAILURE);
    }

    // imprime no ficheiro os tempos de cada thread
    for (int i = 0; i < n_threads; i++)
    {
        struct timespec thread_time = diff_timespec(&threads[i].end_time, &threads[i].start_time);
        fprintf(input_saida, "Thread_%d \t %d \t %10jd.%09ld\n", threads[i].thread_id, threads[i].n_imagens_processadas, thread_time.tv_sec, thread_time.tv_nsec);
    }

    imagens_processadas *current = threads->images->head;
    while (current != NULL)
    {
        struct timespec image_time = diff_timespec(&current->end_process_time, &current->start_process_time);
        fprintf(input_saida, "%s \t %10jd.%09ld\n", current->image_name, image_time.tv_sec, image_time.tv_nsec);
        current = current->next;
    }

    // imprime o tempo total calculado no main
    fprintf(input_saida, "total \t %10jd.%09ld\n", total_time.tv_sec, total_time.tv_nsec);
    fclose(input_saida);
}

/**************************
 * count_images()
 *
 * Argumentos: (FILE*) input -> ficheiro image-list.txt
 *
 * Returns: (int) counter
 *
 * Description: Conta o numero de imagens no ficheiro image-list.txt e devolve esse numero
 *************************/
int count_images(FILE *input)
{
    int counter = 0;
    char buffer[20];
    while (fscanf(input, "%s", buffer) != EOF)
    {
        counter++;
    }
    return counter;
}

/**************************
 * read_image_list_txt()
 *
 * Argumentos: (char*) directory, diretoria onde estao as imagens e o ficheiro image-list.txt
 *
 * Returns: (information*) resultado, struct do tipo information
 *
 * Description: Abre o ficheiro image-list.txt, aloca memoria para um array de strings (tamanho=n_images) para os nomes das fotos a processar.
 * Aloca memoria para uma struct resultado do tipo information e inicializa os seus parametros depois devolve a struct
 *************************/
information *read_image_list_txt(char *directory)
{
    char path[50];
    char *filename = "image-list.txt";
    snprintf(path, sizeof(path), "%s/%s", directory, filename);
    FILE *input = fopen(path, "r");

    if (input == NULL)
    {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    // conta o numero de imagens no ficheiro image-list.txt
    int n_images = count_images(input);
    char **images = malloc(n_images * sizeof(char *));
    if (images == NULL)
    {
        perror("Error allocating memory");
        exit(EXIT_FAILURE);
    }
    rewind(input);

    for (int i = 0; i < n_images; i++)
    {
        images[i] = malloc(20 * sizeof(char));
        if (images[i] == NULL)
        {
            perror("Error allocating memory");
            exit(EXIT_FAILURE);
        }
        fscanf(input, "%s", images[i]);
    }
    fclose(input);

    information *resultado = malloc(sizeof(information));
    if (resultado == NULL)
    {
        perror("Error allocating memory");
        exit(EXIT_FAILURE);
    }

    // inicilaiza os valores da struct
    resultado->images = images;
    resultado->n_images = n_images;
    resultado->papel = NULL;
    resultado->directory = NULL;
    resultado->head = NULL;
    return resultado;
}
