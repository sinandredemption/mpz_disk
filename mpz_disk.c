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
    for (n = 0; n < MPZ_DISK_FILENAME_LEN - 5; n++) {
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
		mp_limb_t carry_now = 0;

		// Pad the input block with zero if the current block
		// number is the last block
		if (n == n_op1_blocks)
			memset(op1_block, 0, limbs_in_block * sizeof(mp_limb_t));
		if (n == n_op2_blocks)
			memset(op2_block, 0, limbs_in_block * sizeof(mp_limb_t));

		fread(op1_block, sizeof(mp_limb_t), limbs_in_block, op1_file);
		fread(op2_block, sizeof(mp_limb_t), limbs_in_block, op2_file);

		// Directly copy the block if the other block is zero
		if (n > n_op1_blocks)
			memcpy(rop_block, op2_block, limbs_in_block * sizeof(mp_limb_t));
		else if (n > n_op2_blocks)
			memcpy(rop_block, op1_block, limbs_in_block * sizeof(mp_limb_t));
		else // Add the blocks
			carry_now = MPZ_DISK_ADD_FUNCTION(rop_block, op1_block, op2_block, limbs_in_block);

		// Process carry as well
		if (carry)
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
	free(rop_block);
	
	// Finally, write out the carry
	if (carry != 0) {
		fwrite(&carry, sizeof(mp_limb_t), 1, rop_file);
		fclose(rop_file);
	}
	else {
		fclose(rop_file);

		// Truncate unneccassary zereos in the output file
		if (_mpz_disk_truncate_leading_zeroes(rop->filename) != 0)
			return MPZ_DISK_ERROR_UNKNOWN;
	}
	return 0;
}
int mpz_disk_sub(mpz_disk_ptr rop, mpz_disk_ptr op1, mpz_disk_t op2)
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
		mp_limb_t carry_now = 0;

		// Pad the input block with zero if the current block
		// number is the last block
		if (n == n_op1_blocks)
			memset(op1_block, 0, limbs_in_block * sizeof(mp_limb_t));
		if (n == n_op2_blocks)
			memset(op2_block, 0, limbs_in_block * sizeof(mp_limb_t));

		fread(op1_block, sizeof(mp_limb_t), limbs_in_block, op1_file);
		fread(op2_block, sizeof(mp_limb_t), limbs_in_block, op2_file);

		// Directly copy the block if the other block is zero
		if (n > n_op1_blocks)
			memcpy(rop_block, op2_block, limbs_in_block * sizeof(mp_limb_t));
		else if (n > n_op2_blocks)
			memcpy(rop_block, op1_block, limbs_in_block * sizeof(mp_limb_t));
		else // Add the blocks
			carry_now = MPZ_DISK_SUB_FUNCTION(rop_block, op1_block, op2_block, limbs_in_block);

		// Process carry as well
		if (carry)
			carry_now += MPZ_DISK_SUB_CARRY_FUNCTION(rop_block, rop_block, limbs_in_block, carry);

		// Write rop_block to rop
		fwrite(rop_block, sizeof(mp_limb_t), limbs_in_block, rop_file);

		assert(carry_now <= 1);	// Carry can either by 0 or 1

		carry = carry_now;
	}

	fclose(op1_file);
	fclose(op2_file);
	fclose(rop_file);
	free(op1_block);
	free(op2_block);
	free(rop_block);
	
	// Finally, write out the carry
	if (carry != 0) {
		assert(0);
	}
	else {
		int ret = _mpz_disk_truncate_leading_zeroes(rop->filename);
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

	//char mp_sign_filename[MPZ_DISK_FILENAME_LEN + 5];
	//_mpz_disk_get_sign_filename(mp_sign_filename, rop);

	//FILE* mp_sign_file = fopen(mp_sign_filename, "wb+");

	//char mp_sign = op->_mp_size < 0 ? MPZ_DISK_SIGN_NEGATIVE : MPZ_DISK_SIGN_POSITIVE;
	//fwrite(&mp_sign, 1, 1, mp_sign_file);
	//fclose(mp_sign_file);

	fwrite(op->_mp_d, sizeof(mp_limb_t), abs(op->_mp_size), mp_file);
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

size_t mpz_disk_size(mpz_disk_ptr mpd)
{
	size_t nlimbs = _mpz_disk_get_file_size(mpd->filename) / sizeof(mp_limb_t);
	if (nlimbs * sizeof(mp_limb_t) < _mpz_disk_get_file_size(mpd->filename))
		nlimbs += 1;

	return nlimbs;
}

void _mpz_disk_get_sign_filename(char* dest, mpz_disk_ptr rop)
{
	memcpy(dest, rop->filename, MPZ_DISK_FILENAME_LEN);
	strcpy(&dest[MPZ_DISK_FILENAME_LEN], ".sgn");
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

int _mpz_disk_truncate_leading_zeroes(char* filename)
{
	size_t bytes_to_truncate = 0;

	FILE* fp = fopen(filename, "rb");

	// If it's a small file, just map the file to memory and proceed
	if (_mpz_disk_get_file_size(filename) < _MPZ_DISK_DEFAULT_SEEK_COUNT * sizeof(mp_limb_t)) {
		mpz_disk_t mp;
		strcpy(mp->filename, filename);

		size_t limbs = mpz_disk_size(mp);	// FIXME This is a quick hack
		mp_limb_t* buf = malloc(limbs * sizeof(mp_limb_t));

		if (buf == NULL) {
			fclose(fp);
			return -1;
		}

		fread(buf, sizeof(mp_limb_t), limbs, fp);
		fclose(fp);

		int top_limb_idx;
		for (top_limb_idx = limbs - 1; top_limb_idx >= 0; top_limb_idx--)
			if (buf[top_limb_idx] != 0)
				break;

		free(buf);

		int ret = _mpz_disk_truncate_file(filename, (limbs - top_limb_idx - 1) * sizeof(mp_limb_t));

		if (ret != 0)
			return -1;
		return 0;
	}


	mp_limb_t buf[_MPZ_DISK_DEFAULT_SEEK_COUNT];

	fseek(fp, 0, SEEK_END);

	int top_limb_idx;
	do
	{
		fseek(fp, -_MPZ_DISK_DEFAULT_SEEK_COUNT * sizeof(mp_limb_t), SEEK_CUR);	// Seek back
		fread(buf, sizeof(mp_limb_t), _MPZ_DISK_DEFAULT_SEEK_COUNT, fp);
		fseek(fp, -_MPZ_DISK_DEFAULT_SEEK_COUNT * sizeof(mp_limb_t), SEEK_CUR);	// Seek back

		for (top_limb_idx = _MPZ_DISK_DEFAULT_SEEK_COUNT - 1; top_limb_idx >= 0; top_limb_idx--)
			if (buf[top_limb_idx] != 0)
				break;

		bytes_to_truncate += (_MPZ_DISK_DEFAULT_SEEK_COUNT - top_limb_idx - 1) * sizeof(mp_limb_t);
	} while (top_limb_idx == -1);

	fclose(fp);

	int ret = _mpz_disk_truncate_file(filename, bytes_to_truncate);

	if (ret != 0)
		return -1;

	return 0;
}


