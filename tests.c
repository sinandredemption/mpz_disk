#ifdef MPZ_DISK_TESTING
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

int test_mpz_disk_truncate_leading_zereos()
{
	printf("Testing _mpz_disk_truncate_leading_zereos()...");

	const int TestCases = 10;
	for (int test_n = 0; test_n < TestCases; ++test_n) {
		FILE* fp = fopen(".__mpz_disk_test.tmp", "wb");

		size_t non_zero_limbs = (rand() * 256) / RAND_MAX;
		non_zero_limbs *= non_zero_limbs;
		size_t n = non_zero_limbs;

		while (n-- > 0)
		{
			// Not the best quality random number...
			mp_limb_t m = rand() * rand();
			m *= rand() * rand();

			fwrite(&m, sizeof(mp_limb_t), 1, fp);
		}

		size_t z = (rand() * 1024) / RAND_MAX;
		z *= z;
		while (z-- > 0)
		{
			// Not the best quality random number...
			mp_limb_t m = 0;
			fwrite(&m, sizeof(mp_limb_t), 1, fp);
		}

		fclose(fp);

		_mpz_disk_truncate_leading_zeroes(".__mpz_disk_test.tmp");

		if (_mpz_disk_get_file_size(".__mpz_disk_test.tmp") != non_zero_limbs * sizeof(mp_limb_t))
		{
			printf(" FAILED\n");
			printf("[ERR] expected: %d got: %d", non_zero_limbs, _mpz_disk_get_file_size(".__mpz_disk_test.tmp"));

			return -1;
		}

		remove(".__mpz_disk_test.tmp");
	}

	printf(" OK\n");
	return 0;
}
int test_mpz_disk_get_mpz() {
	printf("Testing mpz_get_disk_mpz()...");

	const int TestCases = 100;
	int i;
	for (i = 0; i < TestCases; i++) {
		mpz_t rand_mp, mp;
		mpz_init(rand_mp);
		mpz_init(mp);

		mpz_set_str(rand_mp, "3", 10);
		mpz_pow_ui(rand_mp, rand_mp, (rand() * 100) / RAND_MAX);

		mpz_disk_t disk_mp;
		mpz_disk_init(disk_mp);

		mpz_disk_set_mpz(disk_mp, rand_mp);

		mpz_disk_get_mpz(mp, disk_mp);

		if (mpz_cmp(rand_mp, mp) != 0) {
			printf(" FAILED\n");
			mpz_clears(rand_mp, mp);
			mpz_disk_clear(disk_mp);
			return -1;
		}

		mpz_clear(rand_mp);
		mpz_clear(mp);
		mpz_disk_clear(disk_mp);
	}

	printf(" OK [%d cases tested]\n", TestCases);
	return 0;
}

int test_mpz_disk_add() {
	const int TestCases = 100;

	gmp_randstate_t mp_randstate;
	gmp_randinit_default(mp_randstate);

	printf("Testing mpz_disk_add()...");

	int i;
	// Add two random numbers 
	for (i = 0; i < TestCases; ++i)
	{
		mpz_t rand_op1, rand_op2, rand_rop, rop;
		mpz_disk_t disk_op1, disk_op2, disk_rop;

		mpz_init(rop);
		mpz_init(rand_op1);
		mpz_init(rand_op2);
		mpz_init(rand_rop);
		mpz_disk_init(disk_op1);
		mpz_disk_init(disk_op2);
		mpz_disk_init(disk_rop);

		mpz_urandomb(rand_op1, mp_randstate, 1 + (rand() << 14) / RAND_MAX);
		mpz_urandomb(rand_op2, mp_randstate, 1 + (rand() << 14) / RAND_MAX);

		mpz_disk_set_mpz(disk_op1, rand_op1);
		mpz_disk_set_mpz(disk_op2, rand_op2);

		mpz_add     (rand_rop, rand_op1, rand_op2);
		mpz_disk_add(disk_rop, disk_op1, disk_op2);

		mpz_disk_get_mpz(rop, disk_rop);

		if (mpz_cmp(rop, rand_rop) != 0) {
			printf(" FAILED\n");
			printf("[ERR] Incorrect result while adding numbers\n");

			// --
			printf("CASE #%d\n", i);
			gmp_printf("op1: %Zx\n", rand_op1);
			gmp_printf("op2: %Zx\n", rand_op2);
			gmp_printf("rop: %Zx\n", rand_rop);
			gmp_printf("got: %Zx\n", rop);
			// --

			mpz_clear(rop);
			mpz_clear(rand_rop);
			mpz_clear(rand_op1);
			mpz_clear(rand_op2);
			mpz_disk_clear(disk_rop);
			mpz_disk_clear(disk_op1);
			mpz_disk_clear(disk_op2);

			return -1;
		}

		mpz_clear(rop);
		mpz_clear(rand_rop);
		mpz_clear(rand_op1);
		mpz_clear(rand_op2);
		mpz_disk_clear(disk_rop);
		mpz_disk_clear(disk_op1);
		mpz_disk_clear(disk_op2);
	}
	// Check the case if rop == op1 or rop == op2
	// Check the case if op1 == op2

	printf(" OK [%d cases tested]\n", TestCases);
	return 0;
}

int test_mpz_disk_sub() {
	const int TestCases = 100;

	gmp_randstate_t mp_randstate;
	gmp_randinit_default(mp_randstate);

	printf("Testing mpz_disk_sub()...");

	int i;
	// Add two random numbers of equal size
	for (i = 0; i < TestCases; ++i)
	{
		mpz_t rand_op1, rand_op2, rand_rop, rop;
		mpz_disk_t disk_op1, disk_op2, disk_rop;

		mpz_init(rop);
		mpz_init(rand_op1);
		mpz_init(rand_op2);
		mpz_init(rand_rop);
		mpz_disk_init(disk_op1);
		mpz_disk_init(disk_op2);
		mpz_disk_init(disk_rop);

		mpz_urandomb(rand_op1, mp_randstate, 1 + (rand() << 14) / RAND_MAX);
		mpz_urandomb(rand_op2, mp_randstate, 1 + (rand() << 14) / RAND_MAX);

		mpz_disk_set_mpz(disk_op1, rand_op1);
		mpz_disk_set_mpz(disk_op2, rand_op2);

		if (mpz_cmp(rand_op1, rand_op2) > 0) {
			mpz_sub(rand_rop, rand_op1, rand_op2);
			mpz_disk_sub(disk_rop, disk_op1, disk_op2);
		}
		else {
			mpz_sub(rand_rop, rand_op2, rand_op1);
			mpz_disk_sub(disk_rop, disk_op2, disk_op1);
		}

		mpz_disk_get_mpz(rop, disk_rop);

		if (mpz_cmp(rop, rand_rop) != 0) {
			printf(" FAILED\n");
			printf("[ERR] Incorrect result while subtracting numbers\n");

			// --
			printf("CASE #%d\n", i);
			gmp_printf("op1: %Zx\n", rand_op1);
			gmp_printf("op2: %Zx\n", rand_op2);
			gmp_printf("rop: %Zx\n", rand_rop);
			gmp_printf("got: %Zx\n", rop);
			// --

			mpz_clear(rop);
			mpz_clear(rand_rop);
			mpz_clear(rand_op1);
			mpz_clear(rand_op2);
			mpz_disk_clear(disk_rop);
			mpz_disk_clear(disk_op1);
			mpz_disk_clear(disk_op2);

			return -1;
		}

		mpz_clear(rop);
		mpz_clear(rand_rop);
		mpz_clear(rand_op1);
		mpz_clear(rand_op2);
		mpz_disk_clear(disk_rop);
		mpz_disk_clear(disk_op1);
		mpz_disk_clear(disk_op2);
	}
	// Check the case if rop == op1 or rop == op2
	// Check the case if op1 == op2
	// Add 10 random numbers of different size
	// Add 2^x - 1 and 1
	// Add two gigantic numbers of size > 4Gb

	printf(" OK [%d cases tested]\n", TestCases);
	return 0;
}

int main()
{
	int passed = 1;
	passed = passed && !test_mpz_disk_get_file_size();
	passed = passed && !test_mpz_disk_truncate_file();
	passed = passed && !test_mpz_disk_truncate_leading_zereos();
	passed = passed && !test_mpz_disk_get_mpz();
	passed = passed && !test_mpz_disk_add();
	passed = passed && !test_mpz_disk_sub();

	if (!passed)
		return -1;
	return 0;
}
size_t _mpz_disk_simulate_available_mem()
{
	return 1000;
}
#endif