//
// File-system system calls.
// Mostly argument checking, since we don't trust
// user code, and calls into file.c and fs.c.
//

#include "types.h"
#include "defs.h"
#include "param.h"
#include "stat.h"
#include "mmu.h"
#include "proc.h"
#include "fs.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "file.h"
#include "fcntl.h"




static struct inode*
create(char *path, short type, short major, short minor)
{
  struct inode *ip, *dp;
  char name[DIRSIZ];

  if((dp = nameiparent(path, name)) == 0)
    return 0;
  ilock(dp);

  if((ip = dirlookup(dp, name, 0)) != 0){
    iunlockput(dp);
    ilock(ip);
    if(type == T_FILE && ip->type == T_FILE)
      return ip;
    iunlockput(ip);
    return 0;
  }

  if((ip = ialloc(dp->dev, type)) == 0)
    panic("create: ialloc");

  ilock(ip);
  ip->major = major;
  ip->minor = minor;
  ip->nlink = 1;
  iupdate(ip);

  if(type == T_DIR){  // Create . and .. entries.
    dp->nlink++;  // for ".."
    iupdate(dp);
    // No ip->nlink++ for ".": avoid cyclic ref count.
    if(dirlink(ip, ".", ip->inum) < 0 || dirlink(ip, "..", dp->inum) < 0)
      panic("create dots");
  }

  if(dirlink(dp, name, ip->inum) < 0)
    panic("create: dirlink");

  iunlockput(dp);

  return ip;
}





// Fetch the nth word-sized system call argument as a file descriptor
// and return both the descriptor and the corresponding struct file.
static int
argfd(int n, int *pfd, struct file **pf)
{
  int fd;
  struct file *f;

  if(argint(n, &fd) < 0)
    return -1;
  if(fd < 0 || fd >= NOFILE || (f=myproc()->ofile[fd]) == 0)
    return -1;
  if(pfd)
    *pfd = fd;
  if(pf)
    *pf = f;
  return 0;
}

// file f를 비어있는 fd에 저장
// Allocate a file descriptor for the given file.
// Takes over file reference from caller on success.
static int
fdalloc(struct file *f)
{
  int fd;
  struct proc *curproc = myproc();

  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd] == 0){
      curproc->ofile[fd] = f;
      return fd;
    }
  }
  return -1;
}

int
findfd(struct file *f)
{
  int fd;
  struct proc *curproc = myproc();

  for (fd = 0; fd < NOFILE; fd++) {
    if (curproc->ofile[fd] == f) {
      return fd;
    }
  }
  return -1;
}


int
sys_dup(void)
{
  struct file *f;
  int fd;

  if(argfd(0, 0, &f) < 0)
    return -1;
  if((fd=fdalloc(f)) < 0)
    return -1;
  filedup(f);
  return fd;
}


// read의 wrapper function
int
sys_read(void)
{
  struct file *f;
  int n;
  char *p;

  if(argfd(0, 0, &f) < 0 || argint(2, &n) < 0 || argptr(1, &p, n) < 0)
    return -1;
  return fileread(f, p, n);
}

// write의 wrapper function
int
sys_write(void)
{
  struct file *f;
  int n;
  char *p;

  if(argfd(0, 0, &f) < 0 || argint(2, &n) < 0 || argptr(1, &p, n) < 0)
    return -1;
  return filewrite(f, p, n);
}

// close의 wrapper function
int
sys_close(void)
{
  int fd;
  struct file *f;

  if(argfd(0, &fd, &f) < 0)
    return -1;
  myproc()->ofile[fd] = 0;
  fileclose(f);
  return 0;
}

int
sys_fstat(void)
{
  struct file *f;
  struct stat *st;

  if(argfd(0, 0, &f) < 0 || argptr(1, (void*)&st, sizeof(*st)) < 0)
    return -1;
  return filestat(f, st);
}

// 이게 현재 hard link로 되어있는거같네
// Create the path new as a link to the same inode as old.
int
sys_link(void)
{
  char name[DIRSIZ], *new, *old;
  struct inode *dp, *ip;

  if(argstr(0, &old) < 0 || argstr(1, &new) < 0)
    return -1;

  begin_op();
  // * namex 함수 사용
  // * 기존의 old path를 찾아서 있는지 확인
  // * 없으면 그냥 종료(link를 하는게 목적이니까)
  if((ip = namei(old)) == 0){
    end_op();
    return -1;
  }

  ilock(ip);
  // * directory이면 그냥 넘어가는듯? file을 찾는게 목표니까
  if(ip->type == T_DIR){
    iunlockput(ip);
    end_op();
    return -1;
  }

  // * 연결되어 있는 file 개수 증가
  ip->nlink++;
  iupdate(ip);
  iunlock(ip);

  // * 얘도 namex 함수 이용하는데, namex의 nameiparent가 1로 들어감
  // * new 라는 영역에 새로운 파일을 만들 것이다 라고 하는데
  // * new = "/root/hello/newhello" 라고 해보면
  // * hello directory가 있는지부터 확인해야돼
  // * parent가 존재해야 해당 directory 내부에 file이 있는 걸 알 수 있어
  // ! parent가 존재하면 name에다가 parent의 이름을 복사
  // * 그래서 dp가 parent
  if((dp = nameiparent(new, name)) == 0)
    goto bad;
  ilock(dp);
  /* dirlink함수가 하는일
   * 1. dp(parent) entry에서 비어있는 공간(inum이 0인 공간)을 찾기
   * 2. 해당 공간에 이름과 inum을 복사
   * 3. 해당 정보를 dp에 반영하고 disk에 기록
  */
  if(dp->dev != ip->dev || dirlink(dp, name, ip->inum) < 0){
    iunlockput(dp);
    goto bad;
  }
  iunlockput(dp);
  iput(ip);

  end_op();

  return 0;

bad:
  ilock(ip);
  ip->nlink--;
  iupdate(ip);
  iunlockput(ip);
  end_op();
  return -1;
}

int
sys_symbolic_link(void)
{
  char *new, *old;
  struct inode *ip, *newip;

  if(argstr(0, &old) < 0 || argstr(1, &new) < 0)
    return -1;

  begin_op();
  // * namex 함수 사용
  // * 기존의 old path를 찾아서 있는지 확인
  // * 없으면 그냥 종료(link를 하는게 목적이니까)
  if((ip = namei(old)) == 0){
    end_op();
    return -1;
  }

  ilock(ip);
  // * directory이면 그냥 넘어가는듯? file을 찾는게 목표니까
  if(ip->type == T_DIR){
    iunlockput(ip);
    end_op();
    return -1;
  }
  iunlock(ip);
// ^ 내가 추가한 문장  

  // ! dirlookup 쓰면 안된다 => 이거 directory 찾는 함수였어 그래서 원본 복사본 둘 다 그지랄 난거고
  newip = create(new, T_FILE, 0, 0);
  newip->symbolic = 1;
  strncpy(newip->symbolpath, old, 30);
  iupdate(newip);
  // ^ 이거 iunlockput 하면 안되나?
  iunlock(newip);
  end_op();

  return 0;
}



// Is the directory dp empty except for "." and ".." ?
static int
isdirempty(struct inode *dp)
{
  int off;
  struct dirent de;

  for(off=2*sizeof(de); off<dp->size; off+=sizeof(de)){
    if(readi(dp, (char*)&de, off, sizeof(de)) != sizeof(de))
      panic("isdirempty: readi");
    if(de.inum != 0)
      return 0;
  }
  return 1;
}

//PAGEBREAK!
int
sys_unlink(void)
{
  struct inode *ip, *dp;
  struct dirent de;
  char name[DIRSIZ], *path;
  uint off;

  if(argstr(0, &path) < 0)
    return -1;

  begin_op();
  if((dp = nameiparent(path, name)) == 0){
    end_op();
    return -1;
  }

  ilock(dp);

  // Cannot unlink "." or "..".
  if(namecmp(name, ".") == 0 || namecmp(name, "..") == 0)
    goto bad;

  // name에 해당하는 inode를 찾아
  if((ip = dirlookup(dp, name, &off)) == 0)
    goto bad;
  ilock(ip);

  if(ip->nlink < 1)
    panic("unlink: nlink < 1");
  if(ip->type == T_DIR && !isdirempty(ip)){
    iunlockput(ip);
    goto bad;
  }

  memset(&de, 0, sizeof(de));
  if(writei(dp, (char*)&de, off, sizeof(de)) != sizeof(de))
    panic("unlink: writei");
  if(ip->type == T_DIR){
    dp->nlink--;
    iupdate(dp);
  }
  iunlockput(dp);

  ip->nlink--;
  iupdate(ip);
  iunlockput(ip);

  end_op();

  return 0;

bad:
  iunlockput(dp);
  end_op();
  return -1;
}


int
sys_open(void)
{
  char *path;
  int fd, omode;
  // int findsymbol = 0;
  struct file *f;
  struct inode *ip;
  // struct inode *symbol = 0;
  if(argstr(0, &path) < 0 || argint(1, &omode) < 0)
    return -1;

  begin_op();
  // A, B가 symbolic link이면 A를 열 때 이게 symbolic 인지 검사하고
  // symbolic 이면 이 과정을 그대로 진행하는 것이 아닌
  // 그냥 B를 열어버리면 됨

  // * file을 처음 생성하는 경우
  if(omode & O_CREATE){
    ip = create(path, T_FILE, 0, 0);
    if(ip == 0){
      end_op();
      return -1;
    }
  } else {
    if((ip = namei(path)) == 0){
      end_op();
      return -1;
    }
    ilock(ip);
    if(ip->type == T_DIR && omode != O_RDONLY){
      iunlockput(ip);
      end_op();
      return -1;
    }
  }

  if (ip->symbolic == 1) {
    // cprintf("symbolic file\n");
    char symbolpath[30];
    strncpy(symbolpath, ip->symbolpath, 30);
    iupdate(ip);
    iunlock(ip);
    // ^ return 0을 하는 이유
    // ^ ls에서 0보다 작은 경우에는 cannot stat을 출력하기 때문에
    if((ip = namei(symbolpath)) == 0){
      end_op();
      return 0;
    }
    ilock(ip);
  }


  // file을 할당하고, file table에서 index를 저장
  // 둘 다 성공해야 밑으로 내려감
  if((f = filealloc()) == 0 || (fd = fdalloc(f)) < 0){
    if(f)
      fileclose(f);
    iunlockput(ip);
    end_op();
    return -1;
  }
  iunlock(ip);
  end_op();

  f->type = FD_INODE;
  f->ip = ip;
  f->off = 0;
  f->readable = !(omode & O_WRONLY);
  f->writable = (omode & O_WRONLY) || (omode & O_RDWR);
  return fd;
}

int
sys_mkdir(void)
{
  char *path;
  struct inode *ip;

  begin_op();
  if(argstr(0, &path) < 0 || (ip = create(path, T_DIR, 0, 0)) == 0){
    end_op();
    return -1;
  }
  iunlockput(ip);
  end_op();
  return 0;
}

int
sys_mknod(void)
{
  struct inode *ip;
  char *path;
  int major, minor;

  begin_op();
  if((argstr(0, &path)) < 0 ||
     argint(1, &major) < 0 ||
     argint(2, &minor) < 0 ||
     (ip = create(path, T_DEV, major, minor)) == 0){
    end_op();
    return -1;
  }
  iunlockput(ip);
  end_op();
  return 0;
}

int
sys_chdir(void)
{
  char *path;
  struct inode *ip;
  struct proc *curproc = myproc();
  
  begin_op();
  if(argstr(0, &path) < 0 || (ip = namei(path)) == 0){
    end_op();
    return -1;
  }
  ilock(ip);
  if(ip->type != T_DIR){
    iunlockput(ip);
    end_op();
    return -1;
  }
  iunlock(ip);
  iput(curproc->cwd);
  end_op();
  curproc->cwd = ip;
  return 0;
}

int
sys_exec(void)
{
  char *path, *argv[MAXARG];
  int i;
  uint uargv, uarg;

  if(argstr(0, &path) < 0 || argint(1, (int*)&uargv) < 0){
    return -1;
  }
  memset(argv, 0, sizeof(argv));
  for(i=0;; i++){
    if(i >= NELEM(argv))
      return -1;
    if(fetchint(uargv+4*i, (int*)&uarg) < 0)
      return -1;
    if(uarg == 0){
      argv[i] = 0;
      break;
    }
    if(fetchstr(uarg, &argv[i]) < 0)
      return -1;
  }
  return exec(path, argv);
}

int
sys_pipe(void)
{
  int *fd;
  struct file *rf, *wf;
  int fd0, fd1;

  if(argptr(0, (void*)&fd, 2*sizeof(fd[0])) < 0)
    return -1;
  if(pipealloc(&rf, &wf) < 0)
    return -1;
  fd0 = -1;
  if((fd0 = fdalloc(rf)) < 0 || (fd1 = fdalloc(wf)) < 0){
    if(fd0 >= 0)
      myproc()->ofile[fd0] = 0;
    fileclose(rf);
    fileclose(wf);
    return -1;
  }
  fd[0] = fd0;
  fd[1] = fd1;
  return 0;
}

int
sys_symbolcheck(void)
{
  struct inode *ip = 0;
  if (argptr(0, (char**)&ip, sizeof(struct inode)) < 0)
    return -1;
  return symbolcheck(ip);
}

int
sys_getsymbolpath(void)
{
  struct inode *ip = 0;
  char *path;
  if (argptr(0, (char**)&ip, sizeof(struct inode)) < 0 || argptr(1, (char**)&path, sizeof(char*)) < 0)
    return -1;
  getsymbolpath(ip, path);
  return 0;
}