#include "mpz_disk.h"

int mpz_disk_init(mpz_disk_t mp) {
	// Generate a random filename (from https://codereview.stackexchange.com/questions/29198/random-string-generator-in-c)
    const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

    for (int n = 0; n < MPZ_DISK_FILENAME_LEN; n++) {
		int key = rand() % (int)(sizeof charset - 1);
		mp.filename[n] = charset[key];
	}

	mp.filename[MPZ_DISK_FILENAME_LEN] = '\0';

	char zero = 0;

	mp.mp_file = fopen(mp.filename, "wb");
	if (!mp.mp_file)
		return -1;

	fwrite(&zero, 1, 1, mp.mp_file);
}
