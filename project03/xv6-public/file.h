struct file {
  enum { FD_NONE, FD_PIPE, FD_INODE } type;
  int ref; // reference count
  char readable;
  char writable;
  struct pipe *pipe;
  // int symbolic;
  struct inode *ip;   // 이게 공유돼야 함
  uint off;
};


// in-memory copy of an inode
struct inode {
  uint dev;           // Device number
  uint inum;          // Inode number
  int ref;            // Reference count
  int symbolic;       // symbolic 여부
  char symbolpath[30];   // path로 설정
  struct sleeplock lock; // protects everything below here

  // cache에 있으면 false, cache에 없으면 true
  int valid;          // inode has been read from disk? => true false
  short type;         // copy of disk inode
  short major;
  short minor;
  short nlink;
  uint size;

  // direct + single indirect
  uint addrs[NDIRECT+1];
  // multi indirect
  uint d_addr;
  uint t_addr;
  
};

// table mapping major device number to
// device functions
struct devsw {
  int (*read)(struct inode*, char*, int);
  int (*write)(struct inode*, char*, int);
};

extern struct devsw devsw[];

#define CONSOLE 1
