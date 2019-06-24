#include <stdio.h>
#include <string.h>
#include <stdlib.h>

typedef signed char             int8_t;
typedef unsigned char           uint8_t;
typedef signed short int        int16_t;
typedef unsigned short int      uint16_t;

int crc;




static inline int crc_check (unsigned int data_bit) {
  int high_bit = (crc >> 15) & 1; // Highest bit of CRC word 
  //int in = high_bit ^ (data_bit & 1);
  int in = data_bit & 1;
  //int xor_word = in | (in << 2) | (in << 15);
  int xor_word = high_bit | (high_bit << 2) | (high_bit << 15);
  crc = crc << 1;
  crc = crc | in;
  crc = crc ^ xor_word;
  fprintf (stderr, "CRC data_bit=%d high_bit=%d in=%d xor_word=%04X crc=%04X\n", data_bit&1, high_bit, in, 0xffff & xor_word, 0xffff & crc);
  return 0;
}


#define CRC16 0x8005


uint16_t gen_crc16(const uint8_t *data, uint16_t size)
{
  uint16_t out = 0;
  int bits_read = 0, bit_flag;

  /* Sanity check: */
  if(data == NULL)
    return 0;

  while(size > 0)
    {
      bit_flag = out >> 15;

      /* Get next bit: */
      out <<= 1;
      out |= (*data >> bits_read) & 1; // item a) work from the least significant bits

      /* Increment bit counter: */
      bits_read++;
      if(bits_read > 7)
        {
	  bits_read = 0;
	  data++;
	  size--;
        }

      /* Cycle check: */
      if(bit_flag)
	out ^= CRC16;

      fprintf (stderr, "out=%04X bit_flag = %d \n", out & 0xffff,bit_flag );
    }

  // item b) "push out" the last 16 bits
  int i;
  for (i = 0; i < 16; ++i) {
    bit_flag = out >> 15;
    out <<= 1;
    if(bit_flag)
      out ^= CRC16;
    fprintf (stderr, "out=%04X bit_flag = %d \n", out & 0xffff,bit_flag );
  }

  // item c) reverse the bits
  uint16_t crc = 0;
  i = 0x8000;
  int j = 0x0001;
  for (; i != 0; i >>=1, j <<= 1) {
    if (i & out) crc |= j;
  }

  return crc;
}




int main (int argc, char ** argv) {
  unsigned int poly, init, tmp;
  int i,j ;
  FILE * in, * out;
  char buf[4098];
  if (argc !=3) {
    fprintf (stderr, "To few arguments\n");
  } 
  in = fopen (argv[1], "rb");
  if (in == NULL) {
    fprintf (stderr, "Unable to open file %s \n", argv[1]);
    exit(0);
  }
  
  out = fopen (argv[2], "wb");
  if (out == NULL) {
    fprintf (stderr, "Unable to open file %s \n", argv[2]);
    exit(0);
  }
  /*
  if (4096 != fread(buf, 1, 4096, in)) {
    fprintf (stderr, "Failed to read 4098 bytes of data from input file\n");
    exit(0);
  }
  */
  for (init=0; init<4100;init++) {
    fscanf(in,"%2X",&tmp);
    buf[init] = (unsigned char) 0xff&tmp;
  }
  /*  for (init=0; init < 4096; init++) {
    buf[init] = ~buf[init];
    }*/
 
  fwrite (buf, 1, 4098, out);

  //for (init=0; init < 65536; init++) {
    crc = init = 0x1038;
    for (j=0; j<4098; j++) {
      for (i=0; i<8; i++) {
	crc_check(buf[j]>>(7-i));
      }
    }
    fprintf (stderr, "init = %04X, crc=%04X\n", init, 0xffff & crc);
    //if (crc == 0) break;
    //}
  return 0;

}

