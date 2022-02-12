#include "mpz_disk.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int test_mpz_disk_get_file_size()
{
	printf("Testing _mpz_disk_get_file_size()...");

	mpz_disk_t disk_mpz;
	mpz_disk_init(disk_mpz);

	// Number of random bytes to write
	size_t nbytes = (rand() * 10000) / RAND_MAX;

	// Initialize a buffer to temporarily store the random data
	char buf[10000];
	memset(buf, 0, 10000);

	// Fill in the buffer
	for (size_t i = 0; i < nbytes; ++i)
		buf[i] = rand() & 0xff;

	FILE* fp = fopen(disk_mpz->filename, "wb");

	// Write random data to file from the buffer
	fwrite(buf, 1, nbytes, fp);
	fclose(fp);
	
	// Check if length of file is reported correctly
	if (_mpz_disk_get_file_size(disk_mpz->filename) != nbytes)
	{
		printf("\n[ERR] _mpz_disk_get_file_size() returned %d (expected = %d)",
		    	_mpz_disk_get_file_size(disk_mpz->filename), nbytes);

		mpz_disk_clear(disk_mpz);
		return -1;
	}
	
	printf(" OK\n");
	mpz_disk_clear(disk_mpz);
	return 0;
}

int test_mpz_disk_truncate_file()
{
	printf("Testing _mpz_disk_truncate_file()...");

	mpz_disk_t disk_mpz;
	mpz_disk_init(disk_mpz);

	// Number of random bytes to write
	size_t nbytes = (rand() * 10000) / RAND_MAX;
	nbytes = max(nbytes, 0xff);	// Ensure 256 bytes minimum

	// Initialize a buffer to temporarily store the random data
	char buf[10000];
	memset(buf, 0, 10000);

	// Fill in the buffer
	for (size_t i = 0; i < nbytes; ++i)
		buf[i] = rand() & 0xff;

	FILE* fp = fopen(disk_mpz->filename, "wb");
	// Write random data to file from the buffer
	fwrite(buf, 1, nbytes, fp);
	fclose(fp);

	// Truncate the last few bytes
	size_t bytes_to_truncate = (rand() & 0x7f) + 1;	// [1, 128]
	if (_mpz_disk_truncate_file(disk_mpz->filename, bytes_to_truncate) != 0)
	{
		printf("\n[ERR] _mpz_disk_truncate_file() failed unexpectedly");
		mpz_disk_clear(disk_mpz);
		return -1;
	}

	size_t new_expected_size = nbytes - bytes_to_truncate;

	if (_mpz_disk_get_file_size(disk_mpz->filename) != new_expected_size)
	{
		printf("\n[ERR] _mpz_disk_truncate_file() truncated file to size %d bytes (expected = %d bytes)",
		    	_mpz_disk_get_file_size(disk_mpz->filename), new_expected_size);

		mpz_disk_clear(disk_mpz);
		return -1;
	}
	
	// Check if data in the file is correct
	fp = fopen(disk_mpz->filename, "rb");
	char new_buf[10000];
	fread(new_buf, 1, new_expected_size, fp);
	fclose(fp);

	if (memcmp(buf, new_buf, new_expected_size) != 0)
	{
		printf("\n[ERR] Disagreement in file content after _mpz_disk_truncate_file() was called");

		mpz_disk_clear(disk_mpz);
		return -2;
	}
	printf(" OK\n");
	mpz_disk_clear(disk_mpz);
	return 0;
}

int main()
{
	int passed = 1;
	passed = passed && !test_mpz_disk_get_file_size();
	passed = passed && !test_mpz_disk_truncate_file();

	if (!passed)
		return -1;
	return 0;
}