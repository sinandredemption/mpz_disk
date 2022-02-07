#include "mpz_disk.h"
#include <stdlib.h>
#include <string.h>
#include <cassert>

#ifdef _WIN32	/* Windows */
#include <Windows.h>
#elif defined(__unix__)	/* *nix */
#include <unistd.h>
#endif


int mpz_disk_init(mpz_disk_ptr disk_integer) {
	// Generate a random filename (from https://codereview.stackexchange.com/questions/29198/random-string-generator-in-c)
    const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

	int n = 0;
    for (n = 0; n < MPZ_DISK_FILENAME_LEN; n++) {
		int key = rand() % (int)(sizeof charset - 1);
		disk_integer->filename[n] = charset[key];
	}

	// End with .tmp extension
	strcpy(&disk_integer->filename[n], ".tmp");

	mp_limb_t zero = 0;

	/*disk_integer->*/ FILE* mp_file = fopen(disk_integer->filename, "wb");
	if (!/*disk_integer->*/mp_file)
		return -1;

	fwrite(&zero, sizeof(mp_limb_t), 1, /*disk_integer->*/mp_file);
	
	fclose(mp_file);

	return 0;
}

int mpz_disk_clear(mpz_disk_ptr disk_integer)
{
	return remove(disk_integer->filename);
}

int mpz_disk_add(mpz_disk_ptr rop, mpz_disk_ptr op1, mpz_disk_t op2)
{
	FILE* rop_file = fopen(rop->filename, "wb");
	FILE* op1_file = fopen(op1->filename, "rb");
	FILE* op2_file = fopen(op1->filename, "rb");

	if (!rop_file || !op1_file || !op2_file)
		return -1;

	// Addition roughly works in the following way:
	// 1. Read blocks of size "block_size" bytes from op1
	//    and op2
	// 2. Add the two blocks (and the carry from previous
	//    additions) and record the result and carry
	// 3. Write the result to rop and record the carry

	size_t available_mem = _mpz_disk_get_system_free_RAM();

	// We need memory for three blocks and then some
	size_t block_size = available_mem / 3;
	size_t limbs_in_block = block_size / sizeof(mp_limb_t);

	// Number of blocks = 1 + max(bytes in op1, bytes in op2) / block_size
	size_t n_blocks = 1 +
		max(_mpz_disk_get_file_size(op1->filename), _mpz_disk_get_file_size(op2->filename))
		 /
		block_size;
	
	// Try to allocate memory for the blocks
	mp_limb_t* rop_block, * op1_block, * op2_block;

	rop_block = malloc(limbs_in_block * sizeof(mp_limb_t));
	op1_block = malloc(limbs_in_block * sizeof(mp_limb_t));
	op2_block = malloc(limbs_in_block * sizeof(mp_limb_t));

	// TODO Decrease blocks size progressively if any of the
	// memory allocation fails
	if (!rop_block || !op1_block || !op2_block)
		return -2;

	mp_limb_t carry = 0;
	for (int n = 0; n < n_blocks; n++)
	{
		// Read from the files into the blocks
		fread(op1_block, sizeof(mp_limb_t), limbs_in_block, op1_file);
		fread(op2_block, sizeof(mp_limb_t), limbs_in_block, op2_file);

		// Add the blocks
		mp_limb_t carry_now = MPZ_ADD_FUNCTION(rop_block, op1_block, op2_block, limbs_in_block);

		// Process carry as well
		carry_now += MPZ_ADD_CARRY_FUNCTION(rop_block, rop_block, limbs_in_block, carry);

		assert(carry_now <= 1);	// Carry can either by 0 or 1
		
		carry = carry_now;
	}
}

int mpz_disk_set_mpz(mpz_disk_ptr rop, mpz_srcptr op)
{
	FILE* mp_file = fopen(rop->filename, "wb+");
	if (!mp_file)
		return -1;

	fwrite(op->_mp_d, sizeof(mp_limb_t), op->_mp_size, /*rop->*/mp_file);
	fclose(mp_file);

	return 0;
}
size_t _mpz_disk_get_system_free_RAM()
{
#ifdef _WIN32
	MEMORYSTATUSEX status;
	status.dwLength = sizeof(status);
	GlobalMemoryStatusEx(&status);
	return status.ullAvailPhys;
#elif defined(__unix__)
#endif
}

int64_t _mpz_disk_get_file_size(char* filename)
{
#ifdef _WIN32
	HANDLE f = CreateFile
	(
		filename,
		FILE_READ_ATTRIBUTES,
		0,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL
	);

	if (f != INVALID_HANDLE_VALUE)
	{
		PLARGE_INTEGER size;
		GetFileSizeEx(f, &size);

		CloseHandle(f);
		return size;
	}
	else
		return -1;
#elif defined(__unix__)
#endif
}
