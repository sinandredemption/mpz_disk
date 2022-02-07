#include <stdio.h>
#include <stdlib.h>
#include "mpz_disk.h"
#include <mpir.h>

int main()
{
	mpz_t mpz;
	mpz_init(mpz);

	mpz_set_str(mpz, "314159265", 10);
	mpz_pow_ui(mpz, mpz, 10);

	mpz_disk_t mpd;
	mpz_disk_init(mpd);

	mpz_disk_set_mpz(mpd, mpz);

	char mpz_str[10000];

	mpz_get_str(mpz_str, 16, mpz);

	printf("%s", mpz_str);

	mpz_disk_clear(mpd);

	return 0;
}
