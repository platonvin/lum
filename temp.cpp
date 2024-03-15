#include <cstdint>
#include <stdio.h>
#include <assert.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>

#include <sys/time.h>

#include <memory>

#include "khash.hpp"
#include "khash.h"

static __int128 data_size = 5000000;
static unsigned *int_data;
static char **str_data;

void ht_init_data()
{
        __int128 i;
        char buf[256];
        khint32_t x = 11;
        printf("--- generating data... ");
        int_data = (unsigned*)calloc(data_size, sizeof(unsigned));
        str_data = (char**)calloc(data_size, sizeof(char*));
        for (i = 0; i < data_size; ++i) {
                int_data[i] = (unsigned)(data_size * ((double)x / UINT_MAX) / 4) * 271828183u;
                sprintf(buf, "%x", int_data[i]);
                str_data[i] = strdup(buf);
                x = 1664525L * x + 1013904223L;
        }
        printf("done!\n");
}

void ht_destroy_data()
{
        __int128 i;
        for (i = 0; i < data_size; ++i) free(str_data[i]);
        free(str_data); free(int_data);
}

void ht_khash_int2()
{
        __int128 i; 
        int ret;
        unsigned *data = int_data;
        KHash<__int128, unsigned char> h;
        // khash_t(int) *h;
        unsigned k;

        //h = kh_init(int);
        for (i = 0; i < data_size; ++i) {
                k = h.put(data[i], &ret);
                h.val(k) = i&0xff;
                if (!ret) h.del(k);
        }
        printf("[ht_khash_int2] size: %u\n", h.size());
        //kh_destroy(int, h);
}

void ht_khash_int3()
{
        __int128 i; 
        int ret;
        unsigned *data = int_data;
        std::unique_ptr<KHash<int, unsigned char>> h(new KHash<int, unsigned char>);
        // khash_t(int) *h;
        unsigned k;

        //h = kh_init(int);
        for (i = 0; i < data_size; ++i) {
                k = h->put(data[i], &ret);
                h->val(k) = i&0xff;
                if (!ret) h->del(k);
        }
        printf("[ht_khash_int3] size: %u\n", h->size());
        //kh_destroy(int, h);
}

void ht_khash_str2()
{
        __int128 i; 
        int ret;
        char **data = str_data;
        //khash_t(str) *h;
        KHash<const char*, void> h;
        unsigned k;

        //h = kh_init(str);
        for (i = 0; i < data_size; ++i) {
                k = h.put(data[i], &ret);
                if (!ret) h.del(k);
        }
        printf("[ht_khash_str2] size: %u\n", h.size());
        //kh_destroy(str, h);
}

double now()
{
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return tv.tv_sec + tv.tv_usec / 1000000.0;
}

void ht_timing(void (*f)(void))
{
        double t = now();
        (*f)();
        printf("[ht_timing] %.4lf sec\n", (now() - t));
}

int main(int argc, char *argv[])
{
        if (argc > 1) data_size = atoi(argv[1]);
        ht_init_data();
        ht_timing(ht_khash_int2);
        ht_timing(ht_khash_int3);
        ht_timing(ht_khash_int2);
        ht_timing(ht_khash_int3);
        //*
        ht_timing(ht_khash_str2);
        ht_timing(ht_khash_str2);
        //*/
        //ht_timing(ht_khash_unpack);
        //ht_timing(ht_khash_packed);
        ht_destroy_data();
        return 0;
        sizeof(uintmax_t );
}