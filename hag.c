/*

Anagram finder

syntax: hag <dictionary> <query>
example: hag lemmad.txt dongle

This program cuts many corners and makes silly assumptions! Not for general use!

Dictionary is from http://www.eki.ee/tarkvara/wordlist/lemmad.zip IN THE EXACT FORMAT!!!
* Windows newlines!
* 8-bit code table!

Search is by bytes, meaning that it is case-sensitive: "alpid" will not match "Alpid"

If you want to search for words with non-ASCII letters, then you have to go through some trouble.
For example, to get the 'z' with caron in interactive bash: hag lemmad.txt $'\xfe'urnalist

*/

#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

// Longest set of anagrams in lemmad.txt 48 chars
#define RSIZE 49

// Don't index the start of charset
#define NCHARS 224

int main(int argc, char *argv[]) {

  // Vars for timekeeping
  struct timespec starttime, stoptime;
  int timediff;

  // Start measuring time
  clock_gettime(CLOCK_MONOTONIC, &starttime);

  // Vars for file reading
  uint8_t *buffer;         // contains the whole file
  int fh;                  // file handle
  struct stat s;           // file info
  uint8_t *c;              // byte position pointer

  // Vars for tracking character counts
  int qlen = 0;                // query length
  uint8_t qcounter[NCHARS] = {0}; // character counts in query
  uint8_t wcounter[NCHARS] = {0}; // character counts in current word
  uint8_t qcsum = 0;           // XOR checksum of the query word

  // Result string
  char result[RSIZE] = {0}; // result buffer
  char *rpointer = result;  // pointer to end of current result string

  // Process the query word
  // Check against a list of characters that do not exist in lemmas.txt and bailout if any match
  for (char *i = argv[2]; *i; i++) {
    if ( 
      (0x00 <= *i && *i <= 0x1f) || 
      (0x22 <= *i && *i <= 0x26) || 
      (0x28 <= *i && *i <= 0x2c) || 
      (0x2e <= *i && *i <= 0x40) || 
      (0x46 <= *i && *i <= 0x46) || 
      (0x4c <= *i && *i <= 0x4d) || 
      (0x51 <= *i && *i <= 0x51) || 
      (0x57 <= *i && *i <= 0x60) || 
      (0x7b <= *i && *i <= 0xcf) ||
      (0xd1 <= *i && *i <= 0xe3) ||
      (0xe5 <= *i && *i <= 0xe8) ||
      (0xea <= *i && *i <= 0xef) ||
      (0xf1 <= *i && *i <= 0xf4) ||
      (0xf7 <= *i && *i <= 0xfb) ||
      (0xfd <= *i && *i <= 0xfd) ||
      (0xff <= *i && *i <= 0xff)
      ) {
      goto bailout;
    }
    // get character occurrences, checksum and word length of query
    qcounter[(uint8_t)*i - 0xff + NCHARS]++;
    qlen++;
    qcsum ^= *i;
    // Longest word in lemmad.txt is 31 characters, bail out if query is longer than that
    if (qlen > 31) {
      result[0] = 0;
      goto bailout;
    }
  }

  // Some more cheating..
  // The only one-letter word in lemmad.txt is 'a', bail out if one-letter query
  if (qlen == 1) {
    if ((char) *argv[2] == 'a') {
      result[0] = ',';
      result[1] = 'a';
      goto bailout;
    } else {
      goto bailout;
    }
  }

  // Read the words file into memory
  fh = open(argv[1],O_RDONLY);
  fstat(fh,&s);
  buffer = mmap(NULL, s.st_size, PROT_READ, MAP_PRIVATE | MAP_POPULATE, fh, 0);

  int snipskip = 0; // flag to handle chunk boundaries when parallel-processing the buffer
  
  // read through the buffered file, use OpenMP for parallelism
  # pragma omp parallel for private(c,wcounter) firstprivate(snipskip)
  for (c = buffer; c < buffer + s.st_size; c++) {
     uint8_t csum = 0;  // XOR checksum of current word
     uint8_t *start;    // beginning of current word

     // Handle split words in case of multithreading
     // Read until a new line unless:
     // - we already handled this
     // - this loop starts at the beginning of file
     // - we are already at the beginning of a word

     if (!snipskip) {
       snipskip = 1;
       if (c != buffer && (*(c-1) != 0xa)) {
         while (*c != 0xa) {
           c++;
         }
         c++;
       }
     }

     // Read our first whole word
     start = c; // remember beginning of word

     do {
       csum ^= *c;       // update running checksum of word
       c++;              
     } while (*c != 0xd);

     // If the word has length and checksum that match query, check if it is also an anagram
     if ( csum == qcsum && c - start == qlen ) {
       // get a copy of characters that are available to form the anagram
       memcpy(wcounter,qcounter,NCHARS*sizeof(uint8_t));

       // Go through the characters of this word
       for (uint8_t *l = start; l < start + qlen; l++) {
         // if current character is not available, stop processing this word
         if (!wcounter[*l - 0xff + NCHARS]) {
           goto nextline;
         } else {
           // decrease the available letter count for this character
           wcounter[*l - 0xff + NCHARS]--;
         }
       }

       // If we made it this far, then the word must be a valid anagram, add it to result
       #pragma omp critical 
       {
	 *rpointer = ',';
         memcpy(rpointer + 1, start, qlen);
         rpointer += qlen + 1;
       }
     }
nextline:
     // Skip the newline
     c++;
  } 

bailout:
  // Stop measuring time
  clock_gettime(CLOCK_MONOTONIC, &stoptime);

  // POSIX timespec requires handling whole seconds and fractions separately
  if (stoptime.tv_nsec < starttime.tv_nsec) {
    timediff = (stoptime.tv_sec - starttime.tv_sec) * 1000000 - (starttime.tv_nsec - stoptime.tv_nsec) / 1000;
  } else {
    timediff = (stoptime.tv_sec - starttime.tv_sec) * 1000000 + (stoptime.tv_nsec - starttime.tv_nsec) / 1000;
  }

  printf("%d%s\n", timediff, result);
}
