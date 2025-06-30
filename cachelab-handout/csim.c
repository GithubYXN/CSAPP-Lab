#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include "cachelab.h"

#define MAX_E 1024

typedef struct {
  int is_valid;
  unsigned tag;
  int LRU_counter;
} Cache;

void parse_arg(int argc, char **argv, char **tracefile,
               int *s, int *E, int *b) {
  int opt;
  while ((opt = getopt(argc, argv, "s:E:b:t:")) != -1) {
    switch(opt) {
      case 's':
        *s = atoi(optarg);
        break;
      case 'E':
        *E = atoi(optarg);
        break;
      case 'b':
        *b = atoi(optarg);
        break;
      case 't':
        *tracefile = optarg;
        break;
      default:
        printf("Unsupport argument %c.\n", opt);
        break;
    }     
  }
}

void update_LRU(Cache *cache, int start, int end, int id) {
  int bound = cache[id].is_valid ? cache[id].LRU_counter : MAX_E;
  for (int i = start; i < end; i++) {
    if (cache[i].is_valid && cache[i].LRU_counter < bound) {
      cache[i].LRU_counter += 1;
    }
  }
  cache[id].LRU_counter = 0;
}

void handle_request(Cache *cache, unsigned set_number, unsigned tag,
                    int E, int *hit, int *miss, int *eviction) {
  int start = set_number * E;
  int substitute_id = -1, max_LRU_conuter = -1;
  int hit_id = -1, empty_id = -1;
  for (int i = start; i < start + E; i++) {
    if (!cache[i].is_valid) {
      empty_id = i;
    } else if (cache[i].tag == tag) {
      hit_id = i;
      break;
    } else if (cache[i].LRU_counter > max_LRU_conuter) {
      max_LRU_conuter = cache[i].LRU_counter;
      substitute_id = i;
    }
  }  

  if (hit_id != -1) {
    // hit
    update_LRU(cache, start, start + E, hit_id);
    *hit += 1;
    return;
  } else if (empty_id != -1) {
    // empty
    update_LRU(cache, start, start + E, empty_id);
    cache[empty_id].is_valid = 1;
    cache[empty_id].tag = tag;
    *miss += 1;
    return;
  } else {
    // eviction
    update_LRU(cache, start, start + E, substitute_id);
    cache[substitute_id].tag = tag;
    *miss += 1;
    *eviction += 1;
    return;
  }
}

void simulate(char *tracefile, Cache *cache, int s, int E, int b,
              int *hit, int *miss, int *eviction) {
  FILE *fp = fopen(tracefile, "r");
  if (fp == NULL) {
    printf("Open trace file failed!\n");
    return;
  }
  
  char identifier;
  unsigned address;
  int size;
  while(fscanf(fp, " %c %x,%d", &identifier, &address, &size) > 0) {
    unsigned set_number = (unsigned)(address << (64 - s - b)) >> (64 - s);
    unsigned tag = (unsigned)address >> (s + b);
    switch (identifier) {
      case 'M':
        // 'M' represents modify, which access address twice -- load and store
        handle_request(cache, set_number, tag, E, hit, miss, eviction); 
      case 'S':
      case 'L':
        handle_request(cache, set_number, tag, E, hit, miss, eviction); 
        break;
      default:
        break;
    } 
  }
  fclose(fp); 
}

int main(int argc, char **argv) {
  int s, E, b;
  char *tracefile = "";

  parse_arg(argc, argv, &tracefile, &s, &E, &b);

  int hit = 0, miss = 0, eviction = 0; 
  Cache *cache = calloc((1 << s) * E, sizeof(Cache));
  simulate(tracefile, cache, s, E, b, &hit, &miss, &eviction);
  free(cache);

  printSummary(hit, miss, eviction);
  return 0;
}
