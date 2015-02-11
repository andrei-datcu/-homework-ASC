/*
 * Autor: ASC Team
 * Tema 3 - ASC
 *
 * Parsarea de fisiere BTC
 *
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "io.h"
#include <libmisc.h>

void write_btc(char* path, struct c_img* out_img){
	int i, nr_blocks, fd, k, stripe, blit, bc, j;
	char *buf;

	fd = _open_for_write(path);

    int real_width = out_img->width * out_img->div_factor,
        real_height = out_img->height / out_img->div_factor;

	write(fd, &real_width, sizeof(int));
	write(fd, &real_height, sizeof(int));

	nr_blocks = out_img->width * out_img->height / (BLOCK_SIZE * BLOCK_SIZE);
	buf = _alloc(nr_blocks * (2 + BLOCK_SIZE * BLOCK_SIZE / BITS_IN_BYTE));

	k = 0;
	for (i=0; i<real_height / BLOCK_SIZE; i++)
        for (stripe = 0; stripe < out_img->div_factor; ++stripe){
            blit = (stripe * real_height + i * BLOCK_SIZE) *
                out_img->width / (BLOCK_SIZE * BLOCK_SIZE);
            for (bc = 0; bc < out_img->width / BLOCK_SIZE; ++bc){
		    //write a and b
    		buf[k++] = out_img->blocks[bc + blit].a;
    		buf[k++] = out_img->blocks[bc + blit].b;
    		j = 0;
    		    //write bitplane
                for (j = 0; j < BLOCK_SIZE * BLOCK_SIZE / BITS_IN_BYTE; ++j)
                    buf[k++] = out_img->blocks[bc + blit].bitplane[j];
    	    }
        }

	_write_buffer(fd, buf,
		nr_blocks * (2 + BLOCK_SIZE * BLOCK_SIZE / BITS_IN_BYTE));

	free(buf);
	close(fd);
}

void free_btc(struct c_img* image){
	free_align(image->blocks);
}
