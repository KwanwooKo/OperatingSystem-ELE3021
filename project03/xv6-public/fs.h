// On-disk file system format.
// Both the kernel and user programs use this header file.


#define ROOTINO 1  // root i-number
#define BSIZE 512  // block size

// Disk layout:
// [ boot block | super block | log | inode blocks |
//                                          free bit map | data blocks]
//
// mkfs computes the super block and builds an initial file system. The
// super block describes the disk layout:
struct superblock {
  uint size;         // Size of file system image (blocks)
  uint nblocks;      // Number of data blocks
  uint ninodes;      // Number of inodes.
  uint nlog;         // Number of log blocks
  uint logstart;     // Block number of first log block
  uint inodestart;   // Block number of first inode block
  uint bmapstart;    // Block number of first free map block
};

#define NDIRECT 10          // dinode의 크기를 512의 약수로 맞춰줘야 해서
#define NINDIRECT (BSIZE / sizeof(uint))  // SINGLE INDIRECT Block에서 entry 개수 = 128, entry 1개 마다 data block 1개
#define ENTRYNUM  128   // entry 개수를 따로 설정 => 그냥 코드 가독성 때문에 = 128
#define NDINDIRECT (128 * 128)
#define NTINDIRECT (128 * 128 * 128)
// 기존의 direct block 12개 + single indirect block 128개 + double indirect block 128 * 128 + triple indirect block 128 * 128 * 128
#define MAXFILE (NDIRECT + NINDIRECT + NDINDIRECT + NTINDIRECT)

// dinode 구조체의 값을 변경하고 SIZE 크기를 선언해줘야 함
// On-disk inode structure
// Symbolic link: A라는 파일을 열 때 그냥 B를 호출하면 돼
struct dinode {         // directory inode
  short type;           // File type
  short major;          // Major device number (T_DEV only)
  short minor;          // Minor device number (T_DEV only)
  short nlink;          // Number of links to inode in file system
  uint size;            // Size of file (bytes)
  // 기존의 xv6 코드
  // 0~11: direct
  // 12: single indirect, addr[12]에 128개의 addr이 있고 이 128개는 data를 가리킴
  uint addrs[NDIRECT+1];   // Data block addresses
  // d_addr: double indirect
  uint d_addr;             // Data block addresses
  // t_addr: triple indirect
  uint t_addr;             // Data block addresses
};

// Inodes per block.
#define IPB           (BSIZE / sizeof(struct dinode))

// Block containing inode i
#define IBLOCK(i, sb)     ((i) / IPB + sb.inodestart)

// Bitmap bits per block
#define BPB           (BSIZE*8)

// Block of free map containing bit for block b
#define BBLOCK(b, sb) (b/BPB + sb.bmapstart)

// Directory is a file containing a sequence of dirent structures.
#define DIRSIZ 14

// directory entry
struct dirent {
  ushort inum;
  char name[DIRSIZ];
};

