#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>

#define BUFSIZE 1048576

void reverse (char * buf, int size) {
  int i;
  char tmp;
  for (i= 0; i<size/2;i++) {
    tmp = buf[size-1-i];
    buf[size-1-i] = buf[i];
    buf[i]=tmp;
  }
}

int main (int argc, char ** argv) {
  int in,out;
  long chunk=1;
  char buf[BUFSIZE];
  char filename[1024];
  int more, ret;
  long pos, end;
  int read_size;
  if (argc != 3) {
    fprintf(stderr, "To few arguments\n");
    exit(0);
  }
  if (-1 == (in =open (argv[1], O_RDONLY)) ) {
    perror("Failed top open file");
    fprintf (stderr, "Unable to open file %s \n", argv[1]);
    exit(0);
  }
  if (-1 == (out =open (argv[2], O_CREAT | O_WRONLY | O_TRUNC, 0666) )) {
    fprintf (stderr, "Unable to open file %s \n", argv[1]);
    perror("Failed top open file");
    exit(0);
  }
  

  end = lseek (in, 0, SEEK_END);  
  fprintf (stderr, "seek ret=%ld\n", pos);

  do {
    //end = lseek(in, 0, SEEK_CUR);
    fprintf (stderr, "end = %ld \n", end);
    if ((end-BUFSIZE*chunk)>0) {
      fprintf(stderr, "Seek ing to pos %ld\n", end-BUFSIZE*chunk);
      //lseek(in, 0 , SEEK_SET);
      pos = lseek(in, end-BUFSIZE*chunk, SEEK_SET);
      more = 1;
      read_size = BUFSIZE;
    }
    else {
      lseek (in, 0, SEEK_SET);
      fprintf (stderr, "rewinding\n");
      more = 0;
      read_size = end-BUFSIZE*(chunk-1);
    }
    fprintf (stderr, "Read_size= %d\n", read_size);
    pos=lseek(in, 0, SEEK_CUR);
    fprintf (stderr, "Now at pos %ld\n", pos);
    ret = read (in, buf, read_size);
    fprintf (stderr, "Read %d bytes\n", ret);
    reverse(buf, ret);
    chunk++;
    ret=write(out, buf, ret);
    fprintf (stderr, "Wrote %d bytes \n", ret);
  } while (more);

  close (in);
  close (out);

}
