#include <stdio.h>
#include <gmp.h>

int main (void) {
  
  mpz_t p, q, p_minus_1, a, power;

  mpz_init(p);
  mpz_init(p_minus_1);
  mpz_init(q);
  mpz_init(a);
  mpz_init(power);

  mpz_set_str(p, "179769313486231590772930519078902473361797697894230657273430081157732675805500963132708477322407536021120113879871393357658789768814416622492847430639474124377767893424865485276302219601246094119453082952085005768838150682342462881473913110540827237163350510684586298239947245938479716304835356329624225795083", 10);
  mpz_sub_ui(p_minus_1, p, 1);
  mpz_cdiv_q_ui(q, p_minus_1, 2);

  mpz_set_ui(a, 100);

  while (1) {
    mpz_add_ui(a, a, 1);

    mpz_powm_sec(power, a, q, p);
    if (!mpz_cmp_ui(power, 1)) {
      continue;
    }

    mpz_powm_ui(power, a, 2, p);
    if (!mpz_cmp_ui(power, 1)) {
      continue;
    }
    break;
  }

  printf("WE GOT IT BOYS: ");
  mpz_out_str(stdout, 10, a);
  printf("\n");

  return 0;
}
