#ifndef _INC_MPZ_DISK_H
#define _INC_MPZ_DISK_H

#include <stdio.h>
#include <mpir.h>
#include <stdint.h>

#define MPZ_DISK_FILENAME_LEN 20
#define MPZ_DISK_ADD_FUNCTION mpn_add_n
#define MPZ_DISK_ADD_CARRY_FUNCTION mpn_add_1
#define MPZ_DISK_SUB_FUNCTION mpn_sub_n
#define MPZ_DISK_SUB_CARRY_FUNCTION mpn_sub_1
#define MPZ_DISK_AVAILABLE_MEM_FUNCTION _mpz_disk_get_available_mem

#define _MPZ_DISK_DEFAULT_SEEK_COUNT 1024


// Error codes
#define MPZ_DISK_ADD_ERROR_FILE_OPEN_FAIL -1
#define MPZ_DISK_ADD_ERROR_MEM_ALLOC_FAIL -2
#define MPZ_DISK_ERROR_UNKNOWN -314159

#ifdef MPZ_DISK_TESTING
#undef MPZ_DISK_AVAILABLE_MEM_FUNCTION
// Returns a predefined number as available memory (useful for testing purposes)
size_t _mpz_disk_simulate_available_mem();
#define MPZ_DISK_AVAILABLE_MEM_FUNCTION _mpz_disk_simulate_available_mem
#endif

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

int mpz_disk_get_mpz(mpz_ptr mpz, mpz_disk_ptr op);
size_t mpz_disk_size(mpz_disk_ptr mpd);

int mpz_disk_add(mpz_disk_ptr rop, mpz_disk_ptr op1, mpz_disk_t op2);
int mpz_disk_sub(mpz_disk_ptr rop, mpz_disk_ptr op1, mpz_disk_t op2);
void mpz_disk_mul(mpz_disk_ptr rop, mpz_disk_ptr op1, mpz_disk_t op2);

//void mpz_disk_add_mpz(mpz_disk_t, mpz_t, mpz_disk_t);
//void mpz_disk_sub_mpz(mpz_disk_t, mpz_t, mpz_disk_t);
//void mpz_disk_mul_mpz(mpz_disk_t, mpz_t, mpz_disk_t);

size_t _mpz_disk_get_available_mem(); // FIXME Rename
// Get size of file in bytes
int64_t _mpz_disk_get_file_size(char* filename);
// Truncate the last 'bytes_to_truncate' bytes_to_truncate of a file
int _mpz_disk_truncate_file(char* filename, size_t bytes_to_truncate);
// Truncate leading limbs from a mpz_disk_t
int _mpz_disk_truncate_leading_zeroes(char* filename);

#endif
