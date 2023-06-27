#include "types.h"
#include "stat.h"
#include "user.h"

int
main(int argc, char *argv[])
{
  if(argc != 4){
    printf(2, "Usage: ln option old new\n");
    exit();
  }

  // argv[0]: ln
  // argv[1]: option(-h, -s)
  // argv[2]: old
  // argv[3]: new

  // ^ hard link
  if (strcmp(argv[1], "-h") == 0) {
    if(link(argv[2], argv[3]) < 0)
      printf(2, "link %s %s: failed\n", argv[2], argv[3]);
  }

  // ^ symbolic link
  if (strcmp(argv[1], "-s") == 0) {
    if(symbolic_link(argv[2], argv[3]) < 0)
      printf(2, "link %s %s: failed\n", argv[2], argv[3]);
  }

  exit();
}
