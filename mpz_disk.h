#ifndef _INC_MPZ_DISK_H
#define _INC_MPZ_DISK_H

#include <stdio.h>
#include <mpir.h>
#include <stdint.h>

#define MPZ_DISK_FILENAME_LEN 20
#define MPZ_ADD_FUNCTION mpn_add_n
#define MPZ_ADD_CARRY_FUNCTION mpn_add_1

typedef struct
{
	char filename[MPZ_DISK_FILENAME_LEN + 4 + 1]; // 4 for ".tmp", 1 for null termination
	// FILE* mp_file;
	// TODO Do we need a FILE ptr here?
} _mpz_disk_struct;

typedef _mpz_disk_struct  mpz_disk_t[1];
typedef _mpz_disk_struct* mpz_disk_ptr;
typedef const _mpz_disk_struct* mpz_disk_srcptr;

// Initializes the disk_integer with a random filename and zero value
int mpz_disk_init(mpz_disk_ptr disk_integer);
int mpz_disk_clear(mpz_disk_ptr disk_integer);

// Set value of rop from op, i.e. initialize value of a mpz_disk_t from a mpz_t
int mpz_disk_set_mpz(mpz_disk_ptr rop, mpz_srcptr op);

int mpz_disk_set_str_file(mpz_disk_ptr rop, char* filename);

int mpz_disk_add(mpz_disk_ptr rop, mpz_disk_ptr op1, mpz_disk_t op2);
void mpz_disk_sub(mpz_disk_ptr rop, mpz_disk_ptr op1, mpz_disk_t op2);
void mpz_disk_mul(mpz_disk_ptr rop, mpz_disk_ptr op1, mpz_disk_t op2);

//void mpz_disk_add_mpz(mpz_disk_t, mpz_t, mpz_disk_t);
//void mpz_disk_sub_mpz(mpz_disk_t, mpz_t, mpz_disk_t);
//void mpz_disk_mul_mpz(mpz_disk_t, mpz_t, mpz_disk_t);

size_t _mpz_disk_get_system_free_RAM(); // FIXME Rename
// Get size of file in bytes
int64_t _mpz_disk_get_file_size(char* filename);
// Truncate the last 'bytes_to_truncate' bytes_to_truncate of a file
int _mpz_disk_truncate_file(char* filename, size_t bytes_to_truncate);

#endif
