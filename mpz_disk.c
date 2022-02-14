#include "mpz_disk.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#ifdef _WIN32	/* Windows */
#include <Windows.h>

#elif defined(__unix__)	/* *nix */
#include <unistd.h>

#error "Not all POSIX functions have been implemented yet"
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

	FILE* mp_file = fopen(disk_integer->filename, "wb");
	if (!mp_file)
		return -1;

	fwrite(&zero, sizeof(mp_limb_t), 1, mp_file);
	
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
	FILE* op2_file = fopen(op2->filename, "rb");

	if (!rop_file || !op1_file || !op2_file)
	{
		fclose(rop_file);
		fclose(op1_file);
		fclose(op2_file);

		return MPZ_DISK_ADD_ERROR_FILE_OPEN_FAIL;
	}
	// Addition roughly works in the following way:
	// 1. Read blocks of size "block_size" bytes from op1
	//    and op2
	// 2. Add the two blocks (and the carry from previous
	//    additions) and record the result and carry
	// 3. Write the result to rop and record the carry

	size_t available_mem = MPZ_DISK_AVAILABLE_MEM_FUNCTION();

	// We need memory for three blocks and then some
	size_t block_size = available_mem / 3;
	size_t limbs_in_block = block_size / sizeof(mp_limb_t);
	block_size = limbs_in_block * sizeof(mp_limb_t);

	// Total number of blocks in op1 and op2
	// number of blocks = ceil( bytes in op1 / block size )

	size_t op1_filesize = _mpz_disk_get_file_size(op1->filename),
		   op2_filesize = _mpz_disk_get_file_size(op2->filename);
	size_t n_op1_blocks = op1_filesize / block_size;
	size_t n_op2_blocks = op2_filesize / block_size;
	// Round up
	if (op1_filesize > n_op1_blocks * block_size) n_op1_blocks++;
	if (op2_filesize > n_op2_blocks * block_size) n_op2_blocks++;

	// Minimum number of blocks in output (rop)
	size_t n_blocks = max(n_op1_blocks, n_op2_blocks);

	// If both the numbers fit inside a single block,
	// reduce block size to save memory
	if (n_blocks == 1) {
		block_size = max(_mpz_disk_get_file_size(op1->filename),
						 _mpz_disk_get_file_size(op2->filename));
		limbs_in_block = block_size / sizeof(mp_limb_t);
	}
	
	// Try to allocate memory for the blocks
	mp_limb_t* rop_block, * op1_block, * op2_block;

	rop_block = malloc(limbs_in_block * sizeof(mp_limb_t));
	op1_block = malloc(limbs_in_block * sizeof(mp_limb_t));
	op2_block = malloc(limbs_in_block * sizeof(mp_limb_t));

	// TODO Decrease blocks size progressively if any of the
	// memory allocation fails
	if (!rop_block || !op1_block || !op2_block) {
		fclose(rop_file);
		fclose(op1_file);
		fclose(op2_file);

		free(rop_block);
		free(op1_block);
		free(op2_block);

		return MPZ_DISK_ADD_ERROR_MEM_ALLOC_FAIL;
	}

	mp_limb_t carry = 0;
	for (int n = 1; n <= n_blocks; n++)
	{
		// Pad the input block with zero if the current block
		// number is greater than or equal to the number of
		// blocks in the input blocks
		if (n >= n_op1_blocks)
			memset(op1_block, 0, limbs_in_block * sizeof(mp_limb_t));
		if (n >= n_op2_blocks)
			memset(op2_block, 0, limbs_in_block * sizeof(mp_limb_t));

		// Read from the files into the blocks
		fread(op1_block, sizeof(mp_limb_t), limbs_in_block, op1_file);
		fread(op2_block, sizeof(mp_limb_t), limbs_in_block, op2_file);

		// Add the blocks
		mp_limb_t carry_now = MPZ_DISK_ADD_FUNCTION(rop_block, op1_block, op2_block, limbs_in_block);

		// Process carry as well
		carry_now += MPZ_DISK_ADD_CARRY_FUNCTION(rop_block, rop_block, limbs_in_block, carry);

		// Write rop_block to rop
		fwrite(rop_block, sizeof(mp_limb_t), limbs_in_block, rop_file);

		assert(carry_now <= 1);	// Carry can either by 0 or 1
		
		carry = carry_now;
	}

	fclose(op1_file);
	fclose(op2_file);
	// Don't close rop_file just yet
	free(op1_block);
	free(op2_block);
	// Don't free rop_block just yet
	
	// Finally, write out the carry
	if (carry != 0) {
		free(rop_block);

		fwrite(&carry, sizeof(mp_limb_t), 1, rop_file);
		fclose(rop_file);
	}
	else {
		fclose(rop_file);

		// Truncate unneccassary zereos in the output file
		size_t top_limb = limbs_in_block - 1;
		while (rop_block[top_limb] == 0 && top_limb > 0)
			top_limb--;

		free(rop_block);

		int ret = _mpz_disk_truncate_file(rop->filename, (limbs_in_block - top_limb - 1) * sizeof(mp_limb_t));
		if (ret != 0)
			return MPZ_DISK_ERROR_UNKNOWN;
	}
	return 0;
}

int mpz_disk_set_mpz(mpz_disk_ptr rop, mpz_srcptr op)
{
	FILE* mp_file = fopen(rop->filename, "wb+");
	if (!mp_file)
		return -1;

	fwrite(op->_mp_d, sizeof(mp_limb_t), op->_mp_size, mp_file);
	fclose(mp_file);

	return 0;
}

int mpz_disk_get_mpz(mpz_ptr mpz, mpz_disk_ptr op)
{
	size_t size = _mpz_disk_get_file_size(op->filename);
	
	size_t limbs = size / sizeof(mp_limb_t);
	if (size > limbs * sizeof(mp_limb_t))
		limbs++;

	// Round up
	size = limbs * sizeof(mp_limb_t);

	mpz_clear(mpz);
	mpz_init2(mpz, limbs * sizeof(mp_limb_t) * 8);

	char* buf = calloc(limbs, sizeof(mp_limb_t));

	if (buf == NULL)
		return -1;

	FILE* fp = fopen(op->filename, "rb");

	if (!fp) {
		fclose(fp);
		return -1;
	}

	fread(buf, 1, size, fp);
	fclose(fp);

	memcpy(mpz->_mp_d, buf, size);
	mpz->_mp_size = limbs;

	free(buf);

	return 0;
}

size_t _mpz_disk_get_available_mem()
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
	size_t name_size = strlen(filename) + 1;
	wchar_t* wfilename = malloc(name_size * sizeof(wchar_t));
	mbstowcs(wfilename, filename, name_size);

	HANDLE f = CreateFile
	(
		wfilename,
		FILE_READ_ATTRIBUTES,
		0,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL
	);

	free(wfilename);

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

int _mpz_disk_truncate_file(char* filename, size_t bytes_to_truncate)
{
#ifdef _WIN32
	size_t name_size = strlen(filename) + 1;
	wchar_t* wfilename = malloc(name_size * sizeof(wchar_t));
	mbstowcs(wfilename, filename, name_size);

	HANDLE f = CreateFile
	(
		wfilename,
		GENERIC_WRITE,
		0,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL
	);

	free(wfilename);

	if (f == INVALID_HANDLE_VALUE)
		return -1;

	LARGE_INTEGER pos;
	pos.QuadPart = -(LONGLONG) bytes_to_truncate;

	if (SetFilePointerEx(f, pos, NULL, FILE_END) != 0)
	{
		if (SetEndOfFile(f) == 0)
		{
			CloseHandle(f);
			return -1;
		}

		CloseHandle(f);
		return 0;
	}
	else {
		CloseHandle(f);
		return -1;
	}
#elif defined(__unix__)
#endif
}

