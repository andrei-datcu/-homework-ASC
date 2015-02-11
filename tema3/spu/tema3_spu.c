/*
 * Autor: Datcu Andrei Daniel
 * Grupa: 331CC
 * Tema 3 - ASC
 *
 * Codul SPU
*/

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <spu_intrinsics.h>
#include <spu_mfcio.h>
#include "../utils/utils.h"

#define MAX_WIDTH 2048
#define waitag(t) mfc_write_tag_mask(1<<t); mfc_read_tag_status_all();


unsigned short count_set_bits(unsigned char byte){

    /*
     * Functie care numara bitii de unul dintr-un octet
    */

    unsigned short result;
    for (result = 0; byte; ++result)
       byte &= (byte - 1);
    return result;
}

void btc_compress_bs_lines(short int *pixels, unsigned short blocks_per_line,
                           struct block *cblocks, short int *dec_pixels){

    /*
     * Functie care executa taskurile de comprimare si decomprimare primind
     * un sir de pixeli ca intrare precum si cate blocuri sunt pe o line
     * din acesti pixeli (vor fi BLOCK_SIZE linii in *pixels)
    */

	vector signed short pix_sum[blocks_per_line], *pv;
    vector unsigned short cmp[BLOCK_SIZE];
    vector signed int pix_sq_sum[blocks_per_line][2];
	float mean, stdev;
	short i, j, k, *ps;
    int *pps;

	memset(pix_sum, 0, blocks_per_line * BLOCK_SIZE * sizeof(short));
	memset(pix_sq_sum, 0, blocks_per_line * BLOCK_SIZE * 2 * sizeof(short));

    pv = (vector signed short *) pixels;

    for (i = 0; i < BLOCK_SIZE; ++i){
	    for (j = 0; j < blocks_per_line; ++j, ++pv){

		    //Pentru fiecare bloc aduna pixelii pe coloana si pune-i in pix_sum
            pix_sum[j] += *pv;
            //Aduna pixelii^2 si pune-i in pix_sq_sum
            //Un bloc are 8 coloane -> 2 * 4 inturi (vec signed int = 4 inturi)
            pix_sq_sum[j][0] += spu_mule(*pv, *pv);
            pix_sq_sum[j][1] += spu_mulo(*pv, *pv);
        }
    }

    for (j = 0; j < blocks_per_line; ++j){
        //Pentru fiecare bloc calculam mean si stdev

        pv = (vector signed short *) (pixels + j * BLOCK_SIZE);
        ps = (short *)&pix_sum[j];
        pps = (int *)&pix_sq_sum[j];
        mean = 0;
        stdev = 0;
        for (k = 0; k < BLOCK_SIZE; ++k){
            mean += ps[k];
            stdev += pps[k];
        }

        stdev -=  mean * mean / (BLOCK_SIZE * BLOCK_SIZE);
        mean /= BLOCK_SIZE * BLOCK_SIZE;
        stdev = sqrt(stdev / (BLOCK_SIZE * BLOCK_SIZE));

        // generam bit_planeul linie cu linie
        float q = 0;

        for (k = 0; k < BLOCK_SIZE; ++k, pv += blocks_per_line){
            cmp[k] = spu_cmpgt(*pv, (signed short)mean);
            cblocks[j].bitplane[k] = (unsigned char)((spu_gather(cmp[k]))[0]);
            q += count_set_bits(cblocks[j].bitplane[k]);
        }

	    //calculam a si b
        float a, b;

        if (q == 0){
            a = b = mean;
        }
        else {
            float f1, f2;
	    	f1 = sqrt(q / (BLOCK_SIZE * BLOCK_SIZE - q));
            f2 = sqrt((BLOCK_SIZE * BLOCK_SIZE - q) / q);
        	a = (mean - stdev * f1);
    		b = (mean + stdev * f2);
        }

        //avoid conversion issues due to precision errors
        if (a < 0)
            a = 0;
        if (b > 255)
            b = 255;

		cblocks[j].a = (unsigned char)a;
		cblocks[j].b = (unsigned char)b;

        //Decomprimam o data ce stim a si b

        //Replicam a si b intr-un vector de 8 valori pentru a face spu_sel
        vector signed short sa = spu_splats((signed short) a);
        vector signed short sb = spu_splats((signed short) b);
        vector signed short dec;

        short *dpp = dec_pixels + j * BLOCK_SIZE;

        for (k = 0; k < BLOCK_SIZE; ++k, dpp += blocks_per_line * BLOCK_SIZE){
            // Linie cu linie punem a sau b in functie de masca de comparare
            // generata la comprimare (cmp)
            dec = spu_sel(sa, sb, cmp[k]);
            memcpy(dpp, &dec, BLOCK_SIZE * sizeof(short));
        }
    }
}

void get_block_lines(short *dst_pixels, short *src_pixels, short line_width,
                     uint32_t tagid){

    /*
     * Functie care transfera BLOCK_SIZE * line_width pixeli si-i depune
     * in local store incepand cu adresa dst_pixels
    */

    const uint32_t transfer_blk_size = 1024 * 16;
    const uint32_t transfer_pix_size = transfer_blk_size / sizeof(short);
    uint32_t bytes_to_transfer = line_width * BLOCK_SIZE * sizeof(short);

    while (bytes_to_transfer > transfer_blk_size){

        mfc_get(dst_pixels, (unsigned int)src_pixels, transfer_blk_size, tagid,
                0, 0);
        dst_pixels += transfer_pix_size;
        src_pixels += transfer_pix_size;
        bytes_to_transfer -= transfer_blk_size;
    }

    //Ultimul transfer il facem cu bariera pentru double-buffering
    mfc_getb(dst_pixels, (unsigned int)src_pixels, bytes_to_transfer, tagid, 0,
             0);
}


void put_block_lines(short *spu_pixels, short *ppu_pixels, short line_width,
                     uint32_t tagid, int barrier){

    /*
     * Functie care transfera BLOCK_SIZE * line_width pixeli catre PPU si-i
     * depune incepand cu adresa ppu_pixels.
     * Daca barrier = 1 atunci ultimul transfer il realizam cu bariera - pentru
     * double buffering
    */

    const int transfer_blk_size = 1024 * 16;
    const uint32_t transfer_pix_size = transfer_blk_size / sizeof(short);
    int bytes_to_transfer = line_width * BLOCK_SIZE * sizeof(short);

    int stop_cond = barrier * transfer_blk_size;

    while (bytes_to_transfer > stop_cond){

        mfc_put(spu_pixels, (unsigned int)ppu_pixels,
                (uint32_t)MIN(transfer_blk_size, bytes_to_transfer), tagid, 0,
                0);
        spu_pixels += transfer_pix_size;
        ppu_pixels += transfer_pix_size;
        bytes_to_transfer -= transfer_blk_size;
    }

    if (barrier)
        mfc_putb(spu_pixels, (unsigned int)ppu_pixels, bytes_to_transfer, tagid, 0,
                 0);
}

void put_compressed_blocks(struct block *spu_blocks, struct block *ppu_blocks,
                           uint32_t blkno, uint32_t tagid, int barrier){

    /*
     * Functie care transfera BLOCK_SIZE * line_width blocuri comprimate
     * catre PPU si-i depune incepand cu adresa ppu_blocks.
     * Daca barrier = 1 atunci ultimul transfer il realizam cu bariera - pentru
     * double buffering
     *
     * Aceasta functie exista (pe langa put_block_lines) pentru ca in C nu
     * exista patternuri si pentru ca nu e permisa aritmetica pe void*
    */

    const int transfer_bytes_size = 1024 * 16;
    const uint32_t transfer_blocks = transfer_bytes_size / sizeof(struct block);
    int bytes_to_transfer = blkno * sizeof(struct block);
    int stop_cond = barrier * transfer_bytes_size;

    while (bytes_to_transfer > stop_cond){

        mfc_put(spu_blocks, (unsigned int)ppu_blocks,
                (uint32_t)MIN(transfer_bytes_size, bytes_to_transfer),
                tagid, 0, 0);
        spu_blocks += transfer_blocks;
        ppu_blocks += transfer_blocks;
        bytes_to_transfer -= transfer_bytes_size;
    }

    if (barrier)
        mfc_putb(spu_blocks, (unsigned int)ppu_blocks, bytes_to_transfer, tagid, 0, 0);
}

int main(unsigned long long speid, unsigned long long argp, unsigned long long envp){

	short i;
	short buf, nxt_buf;
    param_t p __attribute__ ((aligned(16)));
	uint32_t tag_id[2];

	tag_id[0] = mfc_tag_reserve();
	if (tag_id[0]==MFC_TAG_INVALID){

	}
	tag_id[1] = mfc_tag_reserve();
	if (tag_id[1]==MFC_TAG_INVALID){

	}

	/* transferul initial, cu structura de pointeri */
	mfc_get((void*)&p, argp, sizeof(param_t), tag_id[0], 0, 0);
	waitag(tag_id[0]);


    /*
     * Declaram buffere pentru pixelii originali, pixelii decomprimati
     * si blocurile btc. In functie de p.double_buff declaram doua sau unul
     * pentru fiecare in parte.
    */
    const short pixels_per_strip = p.full_width * BLOCK_SIZE;
	short pixels[p.double_buff + 1][pixels_per_strip] __attribute__ ((aligned(16)));
	short dec_pixels[p.double_buff + 1][pixels_per_strip]__attribute__ ((aligned(16)));
    const short number_of_blocks_per_strip = p.full_width / BLOCK_SIZE;
    struct block compressed_blocks[p.double_buff+1][number_of_blocks_per_strip] __attribute__ ((aligned(16)));

	buf = nxt_buf = 0;

	// daca avem double-buffering primul transfer de date va fi in afara buclei
    if (p. double_buff){
        get_block_lines(pixels[0], p.original_pixels, p.full_width, tag_id[0]);
        p.original_pixels += pixels_per_strip;
    }

	for (i = p.double_buff * BLOCK_SIZE; i < p.chunk_height; i += BLOCK_SIZE) {
        // cat timp nu s-a terminat de luat tot
		nxt_buf = buf ^ p.double_buff;

        //iau o fasie inalta de BLOCK_SIZE pixeli si lunga de p.full_width
        //pixeli
        get_block_lines(pixels[nxt_buf], p.original_pixels, p.full_width,
                        tag_id[nxt_buf]);
        p.original_pixels += pixels_per_strip;

		// Astept transferul precedent de date de la PPU
		waitag(tag_id[buf]);

		// Procesez liniile precedente
        btc_compress_bs_lines(pixels[buf], p.full_width / BLOCK_SIZE,
                              compressed_blocks[buf], dec_pixels[buf]);

		// Trimit datele precedente la PPU

        put_compressed_blocks(compressed_blocks[buf], p.compressed_blocks,
                              number_of_blocks_per_strip, tag_id[buf], 0);
        put_block_lines(dec_pixels[buf], p.dec_pixels, p.full_width,
                        tag_id[buf], 0);

		p.compressed_blocks += number_of_blocks_per_strip;
        p.dec_pixels += pixels_per_strip;

		// Pregatim urmatoarea iteratie
		buf = nxt_buf;
	}

	// Astept ultimul buffer de date de la PPU sau ultima trimitere de date la
    // single buffering

	waitag(tag_id[buf]);
    if (p.double_buff){

	    // Procesez ultimele date
        btc_compress_bs_lines(pixels[buf], p.full_width / BLOCK_SIZE,
                              compressed_blocks[buf], dec_pixels[buf]);

	    // Trimit ultimul buffer la PPU
        put_compressed_blocks(compressed_blocks[buf], p.compressed_blocks,
                              number_of_blocks_per_strip, tag_id[buf], 1);
        put_block_lines(dec_pixels[buf], p.dec_pixels, p.full_width,
                        tag_id[buf], 1);
	    waitag(tag_id[buf]);
    }

	return 0;
}
