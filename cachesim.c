#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <errno.h>
// other headers as needed

#define ADDRESS_LENGTH 64  // 64-bit memory addressing

// other variables as needed
int hits = 0;
int misses = 0;
int evictions = 0;

typedef struct cacheline
{
    unsigned int valid; // valid bit
    unsigned long long tag; // tag 
    unsigned int lru; // least recently used

} CACHELINE;

typedef struct cacheset
{
    CACHELINE * cache_lines; // array of cache lines
} CACHESET;

typedef struct cache
{
    int s; // set bits
    int S; // num of sets (2^s)
    int b; // block bits
    int B; // num of block size (2^b)
    int E; // num of lines per set (associativity)
    CACHESET * cache_sets;
    
}CACHE;

CACHE* make_cache(int s, int b, int E)
{
    // initialize cache parameters
    CACHE *cache = (CACHE*)malloc(sizeof(CACHE));
    cache->s = s;
    cache->S = pow(2, s);
    cache->b = b;
    cache->B = pow(2, b);
    cache->E = E;
    CACHELINE *curlines;
    // create the array of cache sets
    cache->cache_sets = (CACHESET*)malloc(cache->S * sizeof(CACHESET));
    for(int i = 0; i < cache->S; i++)
    {
        cache->cache_sets[i].cache_lines = (CACHELINE*)malloc(cache->E*sizeof(CACHELINE));
        curlines = cache->cache_sets[i].cache_lines;
        for(int j = 0; j < cache->E; j++)
        {
            curlines[j].tag = 0;
            curlines[j].valid = 0;
            curlines[j].lru = 0;
        }
    }

    return cache;
}

void free_cache(CACHE* cache)
{
    // free up the cache
    for(int i = 0; i < cache->S; i++)
    {
        free(cache->cache_sets[i].cache_lines);
    }
    free(cache->cache_sets);
    free(cache);
}

void access_cache(CACHE * cache, char op, unsigned long long address, int v_flag)
{
    // extract the tag
    unsigned long long tag = address >> (cache->s + cache->b);

    // extract the set index
    unsigned long long set_index = address >> (cache->b) & ((1 << cache->s) - 1);
    
    // get the set
    CACHESET *set = &(cache->cache_sets[set_index]);

    int is_hit = 0, lru_index = 0, empty_line = -1, max_lru = -1;


    // loops through each line and check if address already exists
    for(int i = 0; i < cache->E; i++)
    {
        // check if the tags match and valid
        if(set->cache_lines[i].tag == tag && set->cache_lines[i].valid)
        {
            // we found a match
            is_hit = 1;
            hits++; // update hits
            set->cache_lines[i].lru = 0; // update lru
            if(v_flag)
            {
                printf(" hit");
            }
            break;
        }
    }

    if(!is_hit)
    {
        // not a hit
        misses ++; // updated misses
        if(v_flag)
        {
            printf(" miss");
        }

        // need to find an empty line to insert into
        for(int i = 0; i < cache->E; i++)
        {
            if(set->cache_lines[i].valid == 0)
            {
                // we found an empty spot to insert into
                empty_line = i;
                break;
            }
            if(max_lru < set->cache_lines[i].lru)
            {
                // update the lru spot
                max_lru = set->cache_lines[i].lru;
                lru_index = i;
            }
        }

        if(empty_line == -1) // we did not find a spot
        {
            // we need to evict
            evictions++;
            empty_line = lru_index;
            if(v_flag) 
            {
                printf(" eviction");
            }
        }
        for(int i = 0; i < cache->E; i++)
        {
            // update lru for each valid entry
            if(set->cache_lines[i].valid == 1)
            {
                set->cache_lines[i].lru+=1;
            }
        }
        CACHELINE *curlines = set->cache_lines;
        // insert at our spot
        curlines[empty_line].lru = 0;
        curlines[empty_line].tag = tag;
        curlines[empty_line].valid = 1;
    }

    // modify operation is a hit always after the first access
    if(op == 'M')
    {
        hits++;
        if(v_flag)
        {
            printf(" hit");
        }
    }
    if(v_flag)
    {
        printf("\n");
    }
}

void read_file(char* tracename, CACHE *cache, int v_flag)
{
    FILE* file = fopen(tracename, "r");

    if(!file)
    {
        printf("failed to open file!\n");
        return;
    }

    char operation;
    char hex_address[20];
    unsigned long long address;
    int size;
    char line[40] = " ";
    char * tok;

    // parse through the file and simulate cache
    while(!feof(file))
    {
        if(!fgets(line, 40, file))
        {
            fclose(file);
            return;
        }
        if(line[0] == ' ')
        {
            tok = strtok(line+1, " ");
            operation = tok[0];
            tok = strtok(NULL, ",");
            strcpy(hex_address, tok);
            tok = strtok(NULL, "\n");
            size = atoi(tok);
            address = strtoull(hex_address, NULL, 16);
            if(v_flag)
            {
               printf("%c %s,%d", operation, hex_address, size);
            }
            access_cache(cache, operation, address, v_flag);
        }
    }
    fclose(file);
}



/* 
 * this function provides a standard way for your cache
 * simulator to display its final statistics (i.e., hit and miss)
 */ 
void print_summary(int hits, int misses, int evictions)
{
    printf("hits:%d misses:%d evictions:%d\n", hits, misses, evictions);
}

/*
 * print usage info
 */
void print_usage(char* argv[])
{
    printf("Usage: %s [-hv] -s <num> -E <num> -b <num> -t <file>\n", argv[0]);
    printf("Options:\n");
    printf("  -h         Print this help message.\n");
    printf("  -v         Optional verbose flag.\n");
    printf("  -s <num>   Number of set index bits.\n");
    printf("  -E <num>   Number of lines per set.\n");
    printf("  -b <num>   Number of block offset bits.\n");
    printf("  -t <file>  Trace file.\n");
    printf("\nExamples:\n");
    printf("  linux>  %s -s 4 -E 1 -b 4 -t traces/trace01.dat\n", argv[0]);
    printf("  linux>  %s -v -s 8 -E 2 -b 4 -t traces/trace01.dat\n", argv[0]);
    exit(0);
}

/*
 * starting point
 */
int main(int argc, char* argv[])
{
    int opt;
    int set_bits, associativity, block_bits, v_flag = 0;
    char tracefile[64] = "\0";
    CACHE *cache;

    // handle the command line arguments
    while((opt = getopt(argc, argv, "[hv]:s:E:b:t:")) !=-1 )
    {
        switch(opt){
            case 'h':
            {
                //printf("Option -h flag set\n");
                print_usage(argv);
                break;
            }
            case 'v':
            {
                //printf("Option -v flag set\n");
                v_flag=1;
                break;
            }
            case 's':
            {
                //printf("Option -s set with %s\n", optarg);
                set_bits = atoi(optarg);
                break;
            }
            case 'E':
            {
                //printf("Option -E set with %s\n", optarg);
                associativity = atoi(optarg);
                break;
            }
            case 'b':
            {
                //printf("Option -b set with %s\n",optarg);
                block_bits = atoi(optarg);
                break;
            }
            case 't':
            {
                //printf("Option -t set with %s\n", optarg);
                strcpy(tracefile, optarg);
                break;
            }

        }
    }

    cache = make_cache(set_bits, block_bits, associativity);
    //printf("Cache with %d sets, %d-way associativity, %d-byte blocks created.\n",
     //      cache->S, cache->E, cache->B);
    
    //printf("%d, %s\n", h_flag, tracefile);
    read_file(tracefile, cache, v_flag);
    print_summary(hits, misses, evictions);
    
    // assignment done. life is good!
    free_cache(cache);
    return 0;
}
