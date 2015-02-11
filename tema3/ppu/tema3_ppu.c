/*
 * Autor: Datcu Andrei Daniel
 * Grupa: 331CC
 * Tema 3 - ASC
 *
 * Codul PPU
*/

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <libspe2.h>
#include <pthread.h>
#include <libmisc.h>
#include <sys/time.h>

#include "../utils/utils.h"
#include "io.h"

#define MAX_SPU_THREADS 8

extern spe_program_handle_t tema3_spu;

void *ppu_pthread_function(void *thread_arg) {

	spe_context_ptr_t ctx;
	param_t *arg = (param_t *) thread_arg;

	/* Create SPE context */
	if ((ctx = spe_context_create (0, NULL)) == NULL) {
                perror ("Failed creating context");
                exit (1);
        }

	/* Load SPE program into context */
	if (spe_program_load (ctx, &tema3_spu)) {
                perror ("Failed loading program");
                exit (1);
    }

	/* Run SPE context */
	unsigned int entry = SPE_DEFAULT_ENTRY;
	if (spe_context_run(ctx, &entry, 0, arg, NULL, NULL) < 0) {
		perror ("Failed running context");
		exit (1);
	}

	/* Destroy context */
	if (spe_context_destroy (ctx) != 0) {
                perror("Failed destroying context");
                exit (1);
    }

    return NULL;
}

int main(int argc, char **argv)
{

    if (argc != 5 + 1){
        fprintf(stderr,
                "Not the right args! Usage: %s mod"
                " num_spus in.pgm out.btc out.pgm\n", argv[0]);
        return 1;
    }

    short double_buff = atoi(argv[1]);
    short num_spus = atoi(argv[2]);

    struct img image, dec_image;
	struct timeval t1, t2, t3, t4;

    gettimeofday(&t1, NULL);

    //Citim imaginea initiala
    read_pgm(argv[3], &image);

    //Alocam datele pentru imaginea comprimata
    struct c_img c_image;
    c_image.height = image.height;
    c_image.width = image.width;
    c_image.div_factor = image.div_factor;
    c_image.blocks = malloc_align(c_image.height * c_image.width /
                        (BLOCK_SIZE * BLOCK_SIZE) * sizeof(struct block), 4);

    //Alocam date pentru imaginea decomprimata
    dec_image = image;
    dec_image.pixels = malloc_align(image.height * image.width * sizeof(short),
                                    4);

    int i;
    pthread_t threads[MAX_SPU_THREADS];
    param_t thread_arg[MAX_SPU_THREADS] __attribute__ ((aligned(16)));

    short chunk_height = (image.height / num_spus) / BLOCK_SIZE * BLOCK_SIZE;

    gettimeofday(&t3, NULL);
    for(i = 0; i < num_spus; i++) {
        //Pentru fiecare SPU setam argumentele corespunzatoare

        thread_arg[i].original_pixels = image.pixels +
                i * chunk_height * image.width;
        thread_arg[i].dec_pixels = dec_image.pixels +
            i * chunk_height * image.width;
        thread_arg[i].full_width = image.width;
        thread_arg[i].chunk_height = (i < num_spus - 1) ? chunk_height :
            (image.height - chunk_height * (num_spus - 1));
        thread_arg[i].double_buff = double_buff;
        thread_arg[i].compressed_blocks = c_image.blocks +
            i * chunk_height / BLOCK_SIZE * image.width / BITS_IN_BYTE;

        // Creeam un  thread pentru fiecare context SPU
        if (pthread_create (&threads[i], NULL, &ppu_pthread_function,
                            &thread_arg[i]))  {
            perror ("Failed creating thread");
            exit (1);
        }
    }

    // Asteptam terminarea SPU-thread-urilor
    for (i = 0; i < num_spus; i++)
        if (pthread_join (threads[i], NULL)) {
            perror("Failed pthread_join");
            exit (1);
        }
    gettimeofday(&t4, NULL);

    //Scriem rezultatele
    write_btc(argv[4], &c_image);
    write_pgm(argv[5], &dec_image);
    free_pgm(&image);
    free_pgm(&dec_image);
    free_btc(&c_image);

    gettimeofday(&t2, NULL);

    printf("Timp comprimare/decomprimare: %lf\n", GET_TIME_DELTA(t3, t4));
    printf("Timp total: %lf\n", GET_TIME_DELTA(t1, t2));
    return 0;
}
