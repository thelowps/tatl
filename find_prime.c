#include <stdio.h>
#include <gmp.h>
#include <assert.h>

int main (void) {
  
  mpz_t p, q, next, temp, lower_bound;
  mpz_init(p);
  mpz_init(q);
  mpz_init(next);
  mpz_init(temp);
  mpz_init(lower_bound);

  // lower bound
  mpz_set_ui(lower_bound, (unsigned long)2);
  mpz_pow_ui(lower_bound, lower_bound, (unsigned long)1024);

  // count the lower bound digits
  while (mpz_cmp_ui(lower_bound))

  // q
  mpz_cdiv_q_ui(q, lower_bound, 2);

  int found = 0;
  while (!found) {
    mpz_nextprime(next, q);
    mpz_set(q, next);
    mpz_mul_ui(temp, q, (unsigned long)2);
    mpz_add_ui(p, temp, (unsigned long)1);

    /*
    printf("q = ");
    mpz_out_str(stdout, 10, q);
    printf("\n\np = ");
    mpz_out_str(stdout, 10, p);
    printf("\n\n--------\n");
    */

    if (mpz_probab_prime_p(p, 50)) {
      if (mpz_cmp(p, lower_bound) > 0 ) {
	printf("WE HAVE FOUND THE CHOSEN ONE: ");
	mpz_out_str(stdout, 10, p);
	printf("\n");
	found = 1;
      }
    }
  }

  return 0;
}
