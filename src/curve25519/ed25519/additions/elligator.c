#include <string.h>
#include "fe.h"
#include "ge.h"
#include "crypto_uint32.h"
#include "crypto_hash_sha512.h"
#include "crypto_additions.h"

unsigned int legendre_is_nonsquare(fe in)
{
  fe temp;
  fe_pow22523(temp, in);  /* temp = in^((q-5)/8) */
  fe_sq(temp, temp);      /*        in^((q-5)/4) */ 
  fe_sq(temp, temp);      /*        in^((q-5)/2) */
  fe_mul(temp, temp, in); /*        in^((q-3)/2) */
  fe_mul(temp, temp, in); /*        in^((q-1)/2) */

  /* temp is now the Legendre symbol:
   * 1  = square
   * 0  = input is zero
   * -1 = nonsquare
   */
  unsigned char bytes[32];
  fe_tobytes(bytes, temp);
  return 1 & bytes[31];
}

void elligator(fe u, const fe r)
{
  /* r = input
   * x = -A/(1+2r^2)                # 2 is nonsquare
   * e = (x^3 + Ax^2 + x)^((q-1)/2) # legendre symbol
   * if e == 1 (square) or e == 0 (because x == 0 and 2r^2 + 1 == 0)
   *   u = x
   * if e == -1 (nonsquare)
   *   u = -x - A
   */
  fe A, one, twor2, twor2plus1, twor2plus1inv;
  fe x, e, Atemp, uneg;
  unsigned int nonsquare;

  fe_1(one);
  fe_0(A);
  A[0] = 486662;                         /* A = 486662 */

  fe_sq2(twor2, r);                      /* 2r^2 */
  fe_add(twor2plus1, twor2, one);        /* 1+2r^2 */
  fe_invert(twor2plus1inv, twor2plus1);  /* 1/(1+2r^2) */
  fe_mul(x, twor2plus1inv, A);           /* A/(1+2r^2) */
  fe_neg(x, x);                          /* x = -A/(1+2r^2) */

  fe_mont_rhs(e, x);                     /* e = x^3 + Ax^2 + x */
  nonsquare = legendre_is_nonsquare(e); 

  fe_0(Atemp);
  fe_cmov(Atemp, A, nonsquare);          /* 0, or A if nonsquare */
  fe_add(u, x, Atemp);                   /* x, or x+A if nonsquare */ 
  fe_neg(uneg, u);                       /* -x, or -x-A if nonsquare */
  fe_cmov(u, uneg, nonsquare);           /* x, or -x-A if nonsquare */
}

void hash_to_point(ge_p3* p, const unsigned char* in, const unsigned long in_len)
{
  unsigned char hash[64];
  fe h, u;
  unsigned char sign_bit;
  ge_p3 p3;

  crypto_hash_sha512(hash, in, in_len);

  /* take the high bit as Edwards sign bit */
  sign_bit = (hash[31] & 0x80) >> 7; 
  hash[31] &= 0x7F;
  fe_frombytes(h, hash); 
  elligator(u, h);

  ge_montx_to_p3(&p3, u, sign_bit);
  ge_scalarmult_cofactor(p, &p3);
}


void calculate_Bv(ge_p3* Bv, 
                 unsigned char* buf,
                 const unsigned char* A,
                 const unsigned char* msg, const unsigned long msg_len)
{
  int count;

  /* Calculate SHA512(label(2) || A || msg) */
  buf[0] = 0xFD;
  for (count = 1; count < 32; count++)
    buf[count] = 0xFF;
  memmove(buf+32, A, 32);
  memmove(buf+64, msg, msg_len); 

  hash_to_point(Bv, buf, 64 + msg_len);
}


void calculate_Bv_and_V(ge_p3* Bv, 
                       unsigned char* V, 
                       unsigned char* buf,
                       const unsigned char* a,
                       const unsigned char* A,
                       const unsigned char* msg, const unsigned long msg_len)
{
  ge_p3 p3;

  calculate_Bv(Bv, buf, A, msg, msg_len);
  ge_scalarmult(&p3, a, Bv);
  ge_p3_tobytes(V, &p3);
}
