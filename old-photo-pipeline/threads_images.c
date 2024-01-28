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

void *thread_contrast(void *arg)
{
    thread_info *contrast = (thread_info *)arg;
    char *reference;

    while (read(contrast->pipe_read, &reference, sizeof(char *)) > 0)
    {
        char out_file_name[100];
        sprintf(out_file_name, "%s/%s/%s", contrast->extra, old_photo_PIPELINE, reference);
        int encontrado = verifica_ficheiro(out_file_name);

        if (encontrado == 0)
        {
            transformed *processada = malloc(sizeof(transformed));
            clock_gettime(CLOCK_MONOTONIC, &processada->image_contrast);
            char image_in[100];
            sprintf(image_in, "%s/%s", contrast->extra, reference);
            gdImagePtr in_img = read_jpeg_file(image_in);
            gdImagePtr out_contrast_img = contrast_image(in_img);
            gdImageDestroy(in_img);
            processada->image_name = strdup(reference);
            processada->imagem_transformada = out_contrast_img;
            write(contrast->pipe_write, &processada, sizeof(transformed *));
        }
    }
    close(contrast->pipe_write);
}

void *thread_smooth(void *arg)
{
    thread_info *smooth = (thread_info *)arg;
    transformed *reference;

    while (read(smooth->pipe_read, &reference, sizeof(transformed *)) > 0)
    {
        gdImagePtr out_smoothed_img = smooth_image(reference->imagem_transformada);
        gdImageDestroy(reference->imagem_transformada);
        reference->imagem_transformada = out_smoothed_img;
        write(smooth->pipe_write, &reference, sizeof(transformed *));
    }
    close(smooth->pipe_write);
}

void *thread_texture(void *arg)
{
    thread_info *texture = (thread_info *)arg;
    transformed *reference;

    while (read(texture->pipe_read, &reference, sizeof(transformed *)) > 0)
    {
        gdImagePtr papel = read_png_file(texture->extra);
        gdImagePtr out_textured_img = texture_image(reference->imagem_transformada, papel);
        gdImageDestroy(papel);
        gdImageDestroy(reference->imagem_transformada);
        reference->imagem_transformada = out_textured_img;
        write(texture->pipe_write, &reference, sizeof(transformed *));
    }
    close(texture->pipe_write);
}

void *thread_sepia(void *arg)
{
    thread_info *sepia = (thread_info *)arg;
    transformed *reference;
    int i = 0;
    transformed **array_transformadas = malloc(sepia->n_images * sizeof(transformed));

    while (read(sepia->pipe_read, &reference, sizeof(transformed *)) > 0)
    {
        gdImagePtr out_sepia_img = sepia_image(reference->imagem_transformada);
        gdImageDestroy(reference->imagem_transformada);
        char out_file_name[60];
        sprintf(out_file_name, "%s/%s", sepia->extra, reference->image_name);
        if (write_jpeg_file(out_sepia_img, out_file_name) == 0)
        {
            fprintf(stderr, "Impossible to write %s image\n", reference->image_name);
        }
        gdImageDestroy(out_sepia_img);
        clock_gettime(CLOCK_MONOTONIC, &reference->image_sepia);
        array_transformadas[i] = reference;
        i++;
    }
    return (void *)array_transformadas;
}

void *ficheiro_output_timing(transformed **final, struct timespec total_time, char *directory, int n_images, struct timespec start_time_total)
{
    FILE *input_saida;
    char filename_timing[100];
    // int counter = 0;
    //  cria o path para o ficheiro
    snprintf(filename_timing, sizeof(filename_timing), "%s/timing_pipeline.txt", directory);

    // abre o ficheiro
    input_saida = fopen(filename_timing, "w");
    if (input_saida == NULL)
    {
        printf("Error: Unable to open file.\n");
        exit(EXIT_FAILURE);
    }

    // imprime no ficheiro os tempos de cada thread
    for (int i = 0; i < n_images; i++)
    {
        if (!final[i])
            break;
        // struct timespec thread_time = diff_timespec(&threads[i].end_time, &threads[i].start_time);
        struct timespec start_time = diff_timespec(&final[i]->image_contrast, &start_time_total);
        struct timespec final_time = diff_timespec(&final[i]->image_sepia, &start_time_total);
        fprintf(input_saida, "%s\t start \t %jd.%09ld\n", final[i]->image_name, start_time.tv_sec, start_time.tv_nsec);
        fprintf(input_saida, "%s\t end \t %jd.%09ld\n", final[i]->image_name, final_time.tv_sec, final_time.tv_nsec);
        // counter++;
    }

    // imprime o tempo total calculado no main
    fprintf(input_saida, "total \t %10jd.%09ld\n", total_time.tv_sec, total_time.tv_nsec);
    fclose(input_saida);
    // return counter;
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
    return resultado;
}
