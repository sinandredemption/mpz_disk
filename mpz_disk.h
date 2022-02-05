#ifndef _INC_MPZ_DISK_H
#define _INC_MPZ_DISK_H

#include <stdio.h>

#define MPZ_DISK_FILENAME_LEN 20

typedef struct
{
	char filename[MPZ_DISK_FILENAME_LEN + 4 + 1]; // 4 for ".tmp", 1 for null termination
	FILE* mp_file;
	// TODO Do we need a FILE ptr here?
} _mpz_disk;

typedef _mpz_disk* mpz_disk_t;

// Initialize a mpz_disk_t 
int mpz_disk_init(mpz_disk_t);

void mpz_disk_add(mpz_disk_t, mpz_disk_t, mpz_disk_t);
void mpz_disk_sub(mpz_disk_t, mpz_disk_t, mpz_disk_t);
void mpz_disk_mul(mpz_disk_t, mpz_disk_t, mpz_disk_t);

//void mpz_disk_add_mpz(mpz_disk_t, mpz_t, mpz_disk_t);
//void mpz_disk_sub_mpz(mpz_disk_t, mpz_t, mpz_disk_t);
//void mpz_disk_mul_mpz(mpz_disk_t, mpz_t, mpz_disk_t);

void mpz_disk_clear(mpz_disk_t);

#endif
