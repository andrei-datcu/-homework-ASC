/*
 * Autor: ASC Team
 * Tema 3 - ASC
 *
 * Parsarea de fisiere PGM
 *
 * Orice imagine mai lata de MAX_WIDTH pixeli va fi impartita
 * cu un factor par incepand cu 2. Pentru probelam noastra
 * avand in vedre ca latimea e divizibila cu 16 va rezulta
 * o imagine divizibila cu 8
 *
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <libmisc.h>

#include "io.h"

#define MAX_WIDTH 4096

static void read_line(int fd, char* path, char* buf, int buf_size){
	char c = 0;
	int i = 0;
	while (c != '\n'){
		if (read(fd, &c, 1) == 0){
			fprintf(stderr, "Error reading from %s\n", path);
			exit(0);
		}
		if (i == buf_size){
			fprintf(stderr, "Unexpected input in %s\n", path);
			exit(0);
		}
		buf[i++] = c;
	}
	buf[i] = '\0';
}

void read_pgm(char* path, struct img* in_img){
	int fd, i;
	char buf[BUF_SIZE], *token;
	unsigned char* tmp_pixels;

	fd = _open_for_read(path);

	//read file type; expecting P5
	read_line(fd, path, buf, BUF_SIZE);
	if (strncmp(buf, "P5", 2)){
		fprintf(stderr, "Expected binary PGM (P5 type), got %s\n", path);
		exit(0);
	}

	//read comment line
	read_line(fd, path, buf, BUF_SIZE);

	//read image width and height
	read_line(fd, path, buf, BUF_SIZE);
	token = strtok(buf, " ");
	if (token == NULL){
		fprintf(stderr, "Expected token when reading from %s\n", path);
		exit(0);
	}
	in_img->width = atoi(token);
	token = strtok(NULL, " ");
	if (token == NULL){
		fprintf(stderr, "Expected token when reading from %s\n", path);
		exit(0);
	}
	in_img->height = atoi(token);
	if (in_img->width < 0 || in_img->height < 0){
		fprintf(stderr, "Invalid width or height when reading from %s\n", path);
		exit(0);
	}

	//read max value
	read_line(fd, path, buf, BUF_SIZE);

	//allocate memory for image pixels
	tmp_pixels = _alloc(in_img->width * in_img->height);
	in_img->pixels = (short int*) malloc_align(in_img->width * in_img->height *
			sizeof (short int), 4);

	_read_buffer(fd, tmp_pixels, in_img->width * in_img->height);

    //We calculate the div factor knowing 16 | in_img->width

    in_img->div_factor = MAX(((in_img->width / MAX_WIDTH) * 2), 1);

	//from char to short int

    int j, stripe = -1, thresh = 0, new_off = 0, old_off = 0, new_it = 0;

    int new_width = in_img->width / in_img->div_factor;
    for (i = 0; i < in_img->height; ++i){
        //pentru fiecare linie din fiecare stirp
        old_off = i * in_img->width;
        stripe = -1;
        thresh = 0;
        for (j = 0; j < in_img->width; ++j, ++new_it){
            if (j == thresh){
                thresh += new_width;
                ++stripe;
                new_it = 0;
                new_off = (in_img->height * stripe + i) * new_width;
            }
            if (new_it > new_width)
                fprintf(stderr, "Baa");
            in_img->pixels[new_off + new_it]=
                (short int)tmp_pixels[old_off +j];
        }
    }

    in_img->width /= in_img->div_factor;
    in_img->height *= in_img->div_factor;

	free(tmp_pixels);
	close(fd);
}

void write_pgm(char* path, struct img* out_img){
	int fd;
	char buf[BUF_SIZE];
	unsigned char* tmp_pixels;
	int i, j;

	fd = _open_for_write(path);

	//write image type
	strcpy(buf, "P5\n");
	_write_buffer(fd, buf, strlen(buf));

	//write comment
	strcpy(buf, "#Created using BTC\n");
	_write_buffer(fd, buf, strlen(buf));

	//write image width and height
	sprintf(buf, "%d %d\n", out_img->width  * out_img->div_factor,
            out_img->height / out_img->div_factor);
	_write_buffer(fd, buf, strlen(buf));

	//write max value
	strcpy(buf, "255\n");
	_write_buffer(fd, buf, strlen(buf));

	tmp_pixels = calloc(out_img->width * out_img->height, sizeof (char));
	if (!tmp_pixels){
		fprintf(stderr, "Error allocating memory when reading from %s\n",path);
		exit(0);
	}

    int actual_width = out_img->width * out_img->div_factor;
    int actual_height = out_img->height / out_img->div_factor;

    int thresh = 0, line_it = 0, line_off =0, strip = -1;
    register int new_off, old_off = 0;

    for (i = 0; i < out_img->height; ++i, ++line_it){
        if (i == thresh){
            ++strip;
            thresh += actual_height;
            line_it = 0;
            line_off = strip * out_img->width;
        }
        new_off = line_it * actual_width + line_off;
        old_off = i * out_img->width;
        for (j = 0; j < out_img->width; ++j)
            tmp_pixels[new_off + j]= (char)out_img->pixels[old_off + j];
    }

	//write image pixels
	_write_buffer(fd, tmp_pixels, out_img->width * out_img->height);

	free(tmp_pixels);
	close(fd);
}

void free_pgm(struct img* image){
	free_align(image->pixels);
}
