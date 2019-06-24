#include <unistd.h>
#include <stdio.h>      /* printf */
#include <string.h>     /* strcat */
#include <stdlib.h>     /* strtol */
#include <sys/types.h>
//#include <sys/stat.h>
#include <fcntl.h>
#define DECODE_VALUE 19.53125
#define MARK_NUM_ZEROS 20

int stat [256][6];
int faults;

const char *int_to_binary(unsigned int x)
{
  static char b[33];
  b[0] = '\0';

  unsigned int  z;
  for (z = 0x80000000L; z > 0; z >>= 1)
    {
      strcat(b, ((x & z) == z) ? "1" : "0");
    }

  return b;
}

static inline float filter(float v, float *delay)
{
  float in, out;

  in = v + *delay;
  //out = in * 0.034446428576716f + *delay * -0.034124999994713f;
  out = in * 0.150f + *delay * -0.147f;
  *delay = in;
  return out;
}

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


// Decode a track's worth of deltas.
//
//
// drive_params: Drive parameters
// bytes: bytes to process
// bytes_crc_len: Length of bytes including CRC
// cyl,head: Physical Track data from
// sector_index: Sequential sector counter
// seek_difference: Return of difference between expected cyl and header
// sector_status_list: Return of status of decoded sector
// return: Or together of the status of each sector decoded
int mfm_decode(int fd, char * base_str)
{
  // This is which MFM clock and data bits are valid codes. So far haven't
  // found a good way to use this.
  int valid_code[16] = { 0, 1, 1, 0, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0 };
  float actual_time=0.0;
  // This converts the MFM clock and data bits into data bits.
  int code_bits[16] = { 0, 1, 0, 0, 2, 3, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0 };
  // This is the raw MFM data decoded with above
  unsigned int raw_word = 0;
  // Counter to know when to decode the next two bits.
  int raw_bit_cntr = 0;
  // The decoded bits
  unsigned int decoded_word = 0;
  // Counter to know when we have a bytes worth
  int decoded_bit_cntr = 0;
  // loop counter
  int i,j;
  // These are variables for the PLL filter. avg_bit_sep_time is the
  // "VCO" frequency
  float avg_bit_sep_time = DECODE_VALUE; // 200 MHz clocks
  // Clock time is the clock edge time from the VCO.
  float clock_time = 0;
  // How many bits the last delta corresponded to
  int int_bit_pos;
  float filter_state = 0;
  // Time in track for debugging
  int track_time = 0;
  // Counter for debugging
  int tot_raw_bit_cntr = 0;
  // How many zeros we need to see before we will look for the 0xa1 byte.
  // When write turns on and off can cause codes that look like the 0xa1
  // so this avoids them. Some drives seem to have small number of
  // zeros after sector marked bad in header.
  // TODO: look at  v170.raw file and see if number of bits since last
  // header should be used to help separate good from false ID.
  // also ignoring known write splice location should help.
  int zero_count = 0;
  // Number of deltas available so far to process
  int num_deltas;
  // And number from last time
  int last_deltas = 0;
  // Intermediate value
  int tmp_raw_word;
  // how many we have so far
  int byte_cntr = 0;
  // Count all the raw bits for emulation file
  int all_raw_bits_count = 0;
  float filter_out;
  int delta;
  int state=0;
  #define BUF_SIZE 1048576
  char buf[BUF_SIZE];
  int read_cnt;
  char last_value = 1;
  char ch;
  int new_delta;
  double time_stamp;
  int k,l;
  unsigned char outbuf[BUF_SIZE];
  int outbuf_cnt=0;
  int file_cnt = 0;
  FILE * block_file;
  char block_file_name[64];
  char file_end [8];
  int ret;

  raw_word = 0;
  i = 1;
  while ((read_cnt=read(fd,&buf,BUF_SIZE))!=0) {
    //fprintf(stderr, "read_cnt = %d\n", read_cnt);
    for (k=0; k< read_cnt; k++) {
      ch = buf[k];
      //      fprintf (stderr, "buf[%d]=%d\n", k, ch);
      time_stamp++;
      if ((last_value != ch) && (ch == 0)) {
	//edge 
	delta = new_delta;
	fprintf(stderr, "Timestamp %f us  Delta = %d \n", time_stamp/24, delta);
	new_delta = 0;
	last_value = ch;
      }
      else {
	last_value = ch;
	new_delta++;
	continue;
      }

      // This is simulating a PLL/VCO clock sampling the data.
    if ((delta<25) || (delta > 97)) {
	state = 0;
	zero_count=0;
	avg_bit_sep_time = DECODE_VALUE;
	filter_state = 0.0;
	continue;
      }
      clock_time += delta;
      if (state == 0) {
        fprintf (stderr, "Searching delta = %d clock_time %f\n", delta, clock_time);
      }
      else {
        fprintf (stderr, "Reading delta = %d clock_time %f\n", delta, clock_time);
      }
      // Move the clock in current frequency steps and count how many bits
      // the delta time corresponds to
      for (int_bit_pos = 0; clock_time > avg_bit_sep_time / 2;
	   clock_time -= avg_bit_sep_time, int_bit_pos++) {
            
      }
      if (state == 1 && int_bit_pos < 7) {
        stat[delta][int_bit_pos]++;
      }
      if (state == 1 && ((int_bit_pos > 4)||(int_bit_pos<2))) {
	fprintf (stderr, "Invalid mfm pulse length (%d) detected at timestamp %f us", int_bit_pos, time_stamp/24);
      }
      fprintf(stderr, "int_bit_pos = %d clock_time = %f\n", int_bit_pos, clock_time);
      // And then filter based on the time difference between the delta and
      // the clock
      filter_out = filter(clock_time, &filter_state);
      fprintf (stderr, "filter_out = %f", filter_out);
      avg_bit_sep_time = DECODE_VALUE + filter_out;
      
      fprintf (stderr, "avg_bit_sep_time = %f filter_state=%f\n", avg_bit_sep_time, filter_state);
      // Shift based on number of bit times then put in the 1 from the
      // delta. If we had a delta greater than the size of raw word we
      // will lose the unprocessed bits in raw_word. This is unlikely
      // to matter since this is invalid MFM data so the disk had a long
      // drop out so many more bits are lost.
      if (int_bit_pos >= sizeof(raw_word)*8) {
	raw_word = 1;
      } else {
	raw_word = (raw_word << int_bit_pos) | 1;
      }
      tot_raw_bit_cntr += int_bit_pos;
      raw_bit_cntr += int_bit_pos;

      fprintf(stderr, "raw_word = %s %08X\n", (int_to_binary(raw_word)), raw_word);
      fprintf(stderr, "raw_bit_cntr = %d\n", raw_bit_cntr);
      int rest, full, m, tmp;
      if (state == 0) {
	if (outbuf_cnt != 0) {
	  *block_file_name=0;
	  strcat(block_file_name, base_str);
	  sprintf(file_end, "_%d.hex", file_cnt);
	  file_cnt++;
	  strcat(block_file_name, file_end);
	  fprintf (stderr, "Writing block to %s\n", block_file_name);
	  block_file = fopen (block_file_name, "ab");
	  if (block_file == NULL) {
	    fprintf (stderr, "Failed to open %s\n", block_file_name);
	    exit(0);
	  }
	  fprintf (stderr, "About to write %d bytes\n", ((outbuf_cnt > 4098)? 4098: outbuf_cnt));
	    //ret = fwrite(outbuf, ((outbuf_cnt > 4098)? 4098: outbuf_cnt) ,1, block_file);
	    //fprintf (stderr, "Wrote %d bytes to block_file\n", ret);
	    //fclose(block_file);  
	  full = outbuf_cnt / 16;
	  rest = outbuf_cnt % 16;
	  fprintf (stderr, "outbuf_cnt = %d full = %d, rest = %d\n", outbuf_cnt, full, rest);

	  for (l=0; l < full*16; l+=16) {
	    fprintf (stderr, "%04x   ", l);
	    for (m=l; m<l+16; m++) {
	      fprintf (stderr, "%02x ", outbuf[m] );
	      	      if (m<4098) {
		fprintf (block_file, "%02X", outbuf[m]);
		}
	    }
	    fprintf (stderr, " | ");
	    for (m=l; m<l+16; m++) {
	      tmp = outbuf[m];
	      fprintf (stderr, "%c ", (tmp>0x20 && tmp<0x7f)?tmp:'.' );
	    }
	    fprintf(stderr, "| \n");
	  }
	  
	  fprintf (stderr, "%04x   ", l);
	  for (m=l; m<l+rest; m++) {
            fprintf (stderr, "%02x ", outbuf[m] );
	    	    if (m<4098) {
	      fprintf (block_file, "%02X", outbuf[m]);
	      }
          }
	  for (;m<l+16; m++) {
	    fprintf(stderr, "   ");
	  }
	  fprintf (stderr, " | ");
          for (m=l; m<l+rest; m++) {
	    tmp = outbuf[m];
            fprintf (stderr, "%c ", (tmp>0x20 && tmp<0x7f)?tmp:'.' );
          }	     
	  for (;m<l+16; m++) {
	    fprintf(stderr, "  ");
	  }

	  fprintf (stderr, "|\n");
	  fclose(block_file);  
	}

	outbuf_cnt=0;
	if (raw_word == 0x55555555) {
	  zero_count++;
	}
	else {
	  if (zero_count < MARK_NUM_ZEROS) {
	    zero_count = 0;
	  }
	}
	fprintf (stderr, "Zero count= %d\n", zero_count);
	if ((raw_word == 0x51555551) && (zero_count >= MARK_NUM_ZEROS)) {
	  fprintf (stderr, "Sync detected 1\n");
	  raw_bit_cntr = 2;
	  decoded_word = 0;
	  decoded_bit_cntr = 0;
	  state = 1;
	  crc=0x0000;
	  
	}
	else if ((raw_word == 0xA8AAAAA9)&&(zero_count >= MARK_NUM_ZEROS)) {
	  fprintf (stderr, "Sync detected 2\n");
	  raw_bit_cntr = 1;
	  decoded_word = 0;
	  decoded_bit_cntr = 0;
	  state = 1;
	  crc=0x0000;
	}
      }
      else {
      // Are we looking for a mark code?
	// If we have enough bits to decode do so. Stop if state changes
	while (raw_bit_cntr >= 4) {
	  // If we have more than 4 only process 4 this time
	  raw_bit_cntr -= 4;
	  tmp_raw_word = raw_word >> raw_bit_cntr;
	  fprintf (stderr, "tmp_raw_word = %04X\n", tmp_raw_word & 0xf);
	  fprintf (stderr, "decoded_bit_cntr = %d", decoded_bit_cntr);
	  fprintf (stderr, "decoded bits = %01X\n", code_bits[tmp_raw_word & 0xf]);
	  if (!valid_code[tmp_raw_word & 0xf]) {
	    fprintf(stderr, "invalid code %x at %d bit %d at timestamp %f us\n", tmp_raw_word, i,
		    tot_raw_bit_cntr, time_stamp/24);
	    faults++;
	  }
	  decoded_word = (decoded_word << 2) | (0x3 & (~(code_bits[tmp_raw_word & 0xf])));
	  crc_check ((decoded_word >> 1));
	  crc_check (decoded_word);
               decoded_bit_cntr += 2;
	      
               // And if we have a bytes worth store it
               if (decoded_bit_cntr >= 8) {
		 decoded_bit_cntr = 0;
		 fprintf (stderr, "outbuf_cnt = %d decoded_word = %02X\n", outbuf_cnt, decoded_word);
		 if (outbuf_cnt < 4096) {
		   write (1, &decoded_word, 1);
		 }
		 outbuf[outbuf_cnt] = decoded_word;
		 if (outbuf_cnt == 4097) {
		   if ((0xffff & crc) == 0xC00C) {
		     fprintf (stderr, "CRC OK\n");
		   }
		   else {
		     fprintf (stderr, "****** CRC NOT OK ******\n");
		   }
		 }
		 outbuf_cnt++;
               }
	
	}

      }

    }
  }
  fprintf (stderr, "Found %d decoding failures\n", faults);
  for (i=0; i<7; i++) {
    fprintf (stderr, "Number of bits = %d\n", i);
    for (j=0;j<256; j++) {
      fprintf (stderr, "Delta: %d = %d\n", j, stat[j][i]);
    }
  }
  return 0;
}

int main (int argc, char ** argv)
{
  int fd;
  if (argc != 2) {
    exit(0);
  }
  fd = open (argv[1], O_RDONLY);
  if (fd==-1) {
    exit(0);
  }
 
  mfm_decode(fd, argv[1]);
  return 0;
}
