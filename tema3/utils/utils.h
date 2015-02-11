#ifndef UTILS__
#define UTILS__

#define BLOCK_SIZE 		8
#define BITS_IN_BYTE	8

#define GET_TIME_DELTA(t1, t2) ((t2).tv_sec - (t1).tv_sec + \
		((t2).tv_usec - (t1).tv_usec) / 1000000.0)

#define MIN(a, b) ( (a) < (b) ? (a) : (b) )
#define MAX(a, b) ( (a) > (b) ? (a) : (b) )

struct img {
	//regular image
	int width, height, div_factor;
	short int* pixels;
};

struct block{
	//data for a block from the compressed image
	unsigned char a, b;
	unsigned char bitplane[BLOCK_SIZE * BLOCK_SIZE / 8];
	unsigned char _pad[6];
};

struct c_img{
	//compressed image
	int width, height, div_factor;
	struct block* blocks;
};

typedef struct{
  short int *original_pixels, *dec_pixels;
  short int full_width, chunk_height;
  unsigned short double_buff;
  struct block *compressed_blocks;
  unsigned char _pad[10];
} param_t;
  
#endif  
