#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>

#include "lzss_help.h"

#define WINDOW 32768
#define MIN_MATCH 4
#define min(x,y) ((x < y) ? x : y)
#define max(x,y) ((x < y) ? y : x)

uint64_t match_len(char* old, char* fwd, uint64_t fwd_max)
{
  int64_t i;
  for(i=0;i<fwd_max+1;i++)
  {
    if( old[i] != fwd[i] )
    {
      break;
    }
  }
  return (uint64_t) i;
}

/* TODO: Replace this with the Z-algorithm later */
void window_match(char* word, int64_t back_max, int64_t fwd_max, match_t* dst)
{
  uint16_t best = 0;
  int16_t offset = 0;


  for(int16_t i=back_max;i<0;i++)
  {
    char* tmp = word + i;
    uint16_t curr = (uint16_t) match_len(tmp,word,fwd_max);
    if(curr > best)
    {
      offset = i;
      best = curr;
    }
  }

  dst->d = (int16_t) offset;
  dst->l = (uint16_t) best;
}

/* Pass in last pointer WITH INFORMATION IN IT */
/* buf_to_add should be memory aligned. If not, it should be zero appended */
void write_unaligned(unsigned char* last, uint64_t buf_bit_size, unsigned char* buf_to_add, uint64_t add_size)
{
  unsigned char l = last[0];
  uint64_t filled = buf_bit_size % CHAR_BIT;
  uint64_t space = CHAR_BIT - filled;

  unsigned char *buf = last;
  for(int i=0;i<add_size;i++)
  {
    buf[i] = (unsigned char) 0;
    buf[i] = buf[i] | l;
    buf[i] = buf[i] | (buf_to_add[i] >> filled);
    l = buf_to_add[i] << space;
  }
  buf[add_size] = l;
}

uint64_t compress(char* input, uint64_t input_len, char* dst)
{
  match_t match;
  int64_t i=0;
  int64_t w=0;
  
  fprintf(stderr,"\n\nSTARTING COMPRESSION\n\n");

  for(;i<input_len;)
  {
    char *curr = input + i;
    int64_t window_offset = max(input-curr,i-WINDOW);
    window_match(curr, window_offset, min(input_len - i, 2*WINDOW), &match);
    
    if( match.l < MIN_MATCH )
    {
      /* add 0 bit and the byte */
      for(int j=0;j<max(1,match.d);j++)
      {
        w++;
        char* last = dst + w/CHAR_BIT;
        fprintf(stderr,"%c",curr[j]);
        write_unaligned( (unsigned char*) last, w, (unsigned char*) curr+j, 1 );
        w += CHAR_BIT;
        i++;
      }

      /*
      w++;
      char* last = dst + w/CHAR_BIT;
      write_unaligned( (unsigned char*) last, w, (unsigned char*) curr, 1 );
      w += CHAR_BIT;
      i++;
      */
    }
    else
    {
      /* add 1 bit, d, and l */
      uint16_t buf[2];
      // buf[0] = 1 << 15 | match.l;
      buf[0] = match.d;
      buf[1] = match.l;

      fprintf(stderr,"(%x,%x)",match.d,match.l);

      /* write to dst + w */
      // memcpy(dst + i, buf, 2 * sizeof(uint16_t));
      char* last = dst + w/CHAR_BIT;
      write_unaligned( (unsigned char*) last, w, (unsigned char*) buf, 2 * sizeof(uint16_t) );
      w += 2 * sizeof(uint16_t) * CHAR_BIT;
      i += match.l;
    }
  }

  // fprintf(stderr,"\n\n\n%s\n\n\n???\n",input);

  human_readable_compression(dst,(w+CHAR_BIT)/CHAR_BIT);


  fprintf(stderr,"\n\nDONE WITH COMPRESSION\n\n\n");

  return (w+CHAR_BIT)/CHAR_BIT;
}

uint64_t decompress(char* input, uint64_t input_len, char* dst)
{
  if(1) return 0;

  int64_t i = 0;
  int64_t b = 0;
  char flag;
  char tmp;
  unsigned char buf[4];
  uint16_t *uintbuf = (uint16_t*) buf;

  int16_t d;
  uint16_t l;

  for(; b / CHAR_BIT < input_len-1 ;)
  {
    flag = (input[b/CHAR_BIT] >> ((CHAR_BIT-1)-(b%CHAR_BIT))) & 0x1;
    if( !flag )
    {
      b++;
      /* write the next character to dst */
      dst[i] = ((unsigned char*) input)[ b/CHAR_BIT ] <<  (b%CHAR_BIT);
      dst[i] |= ((unsigned char*) input)[ b/CHAR_BIT + 1] >> (CHAR_BIT-b%CHAR_BIT);
      b += CHAR_BIT;
      i++;
    }
    else
    {
      for(int t=0;t<4;t++)
      {
        buf[t] = ((unsigned char) input[ b/CHAR_BIT + t ]) << (b%CHAR_BIT);
        buf[t] |= ((unsigned char) input[ b/CHAR_BIT + t + 1 ]) >> (CHAR_BIT-(b%CHAR_BIT));
      }

      d = (int16_t) uintbuf[0];
      l = uintbuf[1];

      // fprintf(stderr,"d: %d, l: %d\n",d,l);
      // fprintf(stderr,"d: %d, l: %d\n",(int16_t) uintbuf[0], uintbuf[1]);

      for(int j=0;j<l;j++)
      {
        dst[i + j] = dst[i + j + d];
      }

      i += l;
      b += 2 * sizeof(uint16_t) * 8;
    }
  }

  return (uint64_t) i;
}

#define BIT_IDX(arr,b) (*(arr + (b/CHAR_BIT)))
#define TOP_NUM_BITS(b) (b%CHAR_BIT)
#define BOT_NUM_BITS(b) (CHAR_BIT - TOP_NUM_BITS(b))
#define TOP_BITS(c,b) (((unsigned char) c) << TOP_NUM_BITS(b))
#define BOT_BITS(c,b) (((unsigned char) c) >> BOT_NUM_BITS(b))
#define TOP(arr,b) TOP_BITS( BIT_IDX(arr,b) , b )
#define BOT(arr,b) (BOT_BITS( BIT_IDX(arr,b + CHAR_BIT) , b ) & 0)
#define FLAG(arr,b) ((((unsigned char) BIT_IDX(arr,b)) >> (BOT_NUM_BITS(b) - 1)) & 0x1)

void human_readable_compression(unsigned char *comp, uint64_t len)
{
  unsigned char test[5];
  test[0] = 0x0f;
  test[1] = 0xf0;
  test[2] = 0xff;
  test[3] = 0xff;
  for(unsigned char i=0;i<CHAR_BIT*2;i++)
  {
    fprintf(stderr,"TOP_NUM_BITS %d BOT_NUM_BITS %d\n",TOP_NUM_BITS(i),BOT_NUM_BITS(i));
    fprintf(stderr,"TOP_BITS %x BOT_BITS %x\n",TOP(test,i),BOT(test,i));
    fprintf(stderr,"BIT_IDX %x NEXT BIT_IDX %x\n",BIT_IDX(test,i),BIT_IDX(test,i + CHAR_BIT));
    fprintf(stderr,"\n");
  }
  fprintf(stderr,"\n\n\n");
  


  unsigned char p_buf[ LARGE ];
  memset(p_buf,0,LARGE);

  int64_t i = 0;
  int64_t b = 0;
  unsigned char flag;
  unsigned char tmp;
  unsigned char buf[4];
  uint16_t *uintbuf = (uint16_t*) buf;

  for(; b / CHAR_BIT < len-1 ;)
  {
    int ii = i;
    int bb = b;
    flag = FLAG( comp , b );
    // flag = (comp[b/CHAR_BIT] >> ((CHAR_BIT-1)-(b%CHAR_BIT))) & 0x1;

    if (bb==739)
    {
      for(int asd=0;asd<2;asd++)
      {
        for(int j=asd*10;j<(asd+1)*10;j++)
          char_dump_bin(comp[b/CHAR_BIT - 10 + j]);
        fprintf(stderr,"\n");
      }
      fprintf(stderr,"FLAG 0x%x >> %d\n",BIT_IDX(comp,b), BOT_NUM_BITS(b) - 1);
 
    }

    if( !flag )
    {
      b++;
      /* write the next character to dst */
      p_buf[i] = TOP(comp,b);
      p_buf[i] |= BOT(comp,b);
      b += CHAR_BIT;
      if(p_buf[i] >= 0x80 && p_buf[i] <= 0x9f)
      {
        fprintf(stderr,"special character?? 0x%x\n",p_buf[i]);
      }
      i++;
    }
    else
    {
      for(int t=0;t<4;t++)
      {
        buf[t] = TOP(comp,b);
        buf[t] |= BOT(comp,b);
        b += CHAR_BIT;
      }

      if(((int16_t) uintbuf[0]) >= 0)
      {
        char_dump_bin(comp[(b-1-4*CHAR_BIT)/CHAR_BIT]);
        fprintf(stderr,"FLAG 0x%x b %d b mod 8 %d\n",(comp[(b-9)/CHAR_BIT] >> ((CHAR_BIT-1)-((b-9)%CHAR_BIT))) & 0x1,
            (int)b-9,(int)((b-1-4*CHAR_BIT)%CHAR_BIT));
        char_dump_bin(comp[(b-4*CHAR_BIT)/CHAR_BIT]);
        fprintf(stderr,"FLAG 0x%x b %d b mod 8 %d\n",flag,(int)b,(int)(b%CHAR_BIT));
      }

      p_buf[i] = '(';
      i++;
      int len = sprintf(p_buf + i,"%x", (int16_t) uintbuf[0]);
      i += len;
      p_buf[i] = ',';
      i++;
      len = sprintf(p_buf + i,"%x",uintbuf[1]);
      i += len;
      p_buf[i] = ')';
      i++;
    }

    fprintf(stderr,"b: %d ||| ",bb);
    fprintf(stderr,"f: 0x%x ||| ",flag);
    fprintf(stderr,"s: %s\n",p_buf + ii);
  }

  p_buf[i] = '\0';

  fprintf(stderr,"\nHUMAN READABLE COMPRESSED (char size %d):\n%s\n",CHAR_BIT,p_buf);

}

void char_dump_bin(unsigned char c)
{
  unsigned char buf[CHAR_BIT + 1];
  for(int i=0;i<CHAR_BIT;i++)
  {
    buf[i] = '0' + ((c >> (CHAR_BIT-1-i)) & 0x1);
  }
  buf[8] = '\0';
  fprintf(stderr,"%s ",buf);
}



