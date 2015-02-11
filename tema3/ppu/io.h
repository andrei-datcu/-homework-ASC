#ifndef IO_H__
#define IO_H__ 1

#include "../utils/utils.h"

#define BUF_SIZE 		256

void write_btc(char *path, struct c_img* out_img);
void free_btc(struct c_img* image);
void read_pgm(char* path, struct img* in_img);
void write_pgm(char* path, struct img* out_img);
void free_pgm(struct img* image);

int _open_for_read(char* path);
int _open_for_write(char* path);
void _write_buffer(int fd, void* buf, int size);
void _read_buffer(int fd, void* buf, int size);
void* _alloc(int size);

#endif
