#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

#define DFILESZ 6 * 1024 * 1024    // 6MB
#define TFILESZ 16 * 1024 * 1024   // 16MB

void write_file(char* filename) {
  int fd = open(filename, O_CREATE | O_WRONLY);
  if (fd < 0) {
	  printf(2, "cannot open file\n");
  }
  char data_block[512];
  memset(data_block, 'A', sizeof(data_block));

  for (int i = 0; i < TFILESZ / sizeof(data_block); i++) {
    write(fd, data_block, sizeof(data_block));
    if (i % 2048 == 0)
        printf(2, "write %dMB\n", i / 2048);
  }

  close(fd);
}

void read_file(char* filename) {
  int fd = open(filename, O_RDONLY);
  if (fd < 0) {
	printf(2, "cannot open file\n");
  }
  char data_block[512];

  while (read(fd, data_block, sizeof(data_block)) > 0) {
    // printf(2, "data block: %s\n", data_block);
  }

  close(fd);
}

int main(void) {
  char filename[] = "testfile";

  // Write a file with 6MB of data
  printf(2, "file write start!\n");
  write_file(filename);
  printf(2, "file write finished\n");

  // Read the file
  printf(2, "read file start!\n");
  read_file(filename);
  printf(2, "read file finished\n");

  // Remove the file
  printf(2, "test finished\n");
  exit();
}
