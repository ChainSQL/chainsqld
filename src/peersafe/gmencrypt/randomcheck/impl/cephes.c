/* --------------------------------------------------------------------------

Origin:     http://www.DiceLock.org 
Programmer: Angel Ferré
Date:       September 2007
File:       cephes.c - vers.1.0.0

Purpose:    
Provide NIST Statistical Test Suite (those tests that can be used 
with short bit streams) as library through the API creation.

Warnings:
- Porting issues.
- In order to create the library the inner original code has been modified in
  some cases.
- Tested over openSUSE 10.2 x86 32 bits.     

                              DISCLAIMER

THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESSED OR IMPLIED WARRANTIES, 
INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY 
AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE 
REGENTS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; 
OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR 
OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF 
ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

-------------------------------------------------------------------------- */

/* --------------------------------------------------------------------------

DiceLock.org note:

The following NIST notice has been extracted from assess.c of The NIST 
Statistical Test Suite in order to provide information about the original
source code creator.  

-------------------------------------------------------------------------- */

/* --------------------------------------------------------------------------
   Title       :  The NIST Statistical Test Suite

   Date        :  December 1999

   Programmer  :  Juan Soto

   Summary     :  For use in the evaluation of the randomness of bitstreams
		  produced by cryptographic random number generators.

   Package     :  Version 1.0

   Copyright   :  (c) 1999 by the National Institute Of Standards & Technology

   History     :  Version 1.0 by J. Soto, October 1999
		  Revised by J. Soto, November 1999

   Keywords    :  Pseudorandom Number Generator (PRNG), Randomness, Statistical 
                  Tests, Complementary Error functions, Incomplete Gamma 
	          Function, Random Walks, Rank, Fast Fourier Transform, 
                  Template, Cryptographically Secure PRNG (CSPRNG),
		  Approximate Entropy (ApEn), Secure Hash Algorithm (SHA-1), 
                  Blum-Blum-Shub (BBS) CSPRNG, Micali-Schnorr (MS) CSPRNG, 

   Source      :  David Banks, Elaine Barker, James Dray, Allen Heckert, 
		  Stefan Leigh, Mark Levenson, James Nechvatal, Andrew Rukhin, 
		  Miles Smid, Juan Soto, Mark Vangel, and San Vo.

   Technical
   Assistance  :  Lawrence Bassham, Ron Boisvert, James Filliben, Sharon Keller,
		  Daniel Lozier, and Bert Rust.

   Warning     :  Portability Issues.

   Limitation  :  Amount of memory allocated for workspace.

   Restrictions:  Permission to use, copy, and modify this software without 
		  fee is hereby granted, provided that this entire notice is 
		  included in all copies of any software which is or includes
                  a copy or modification of this software and in all copies 
                  of the supporting documentation for such software.
   -------------------------------------------------------------------------- */

#include <stdio.h>
#include <math.h>
#include <peersafe/gmencrypt/randomcheck/config.h>
#include <peersafe/gmencrypt/randomcheck/mconf.h>

double p1evl(double, double [], int);
double polevl(double, double [], int);

#ifndef ANSIPROT
double lgam(), exp(), log(), fabs(), igam(), igamc();
#endif

int merror = 0;

/* Notice: the order of appearance of the following
 * messages is bound to the error codes defined
 * in mconf.h.
 */
static char *ermsg[7] = {
"unknown",      /* error code 0 */
"domain",       /* error code 1 */
"singularity",  /* et seq.      */
"overflow",
"underflow",
"total loss of precision",
"partial loss of precision"
};



/*                                                      const.c
 *
 *     Globally declared constants
 *
 *
 *
 * SYNOPSIS:
 *
 * extern double nameofconstant;
 *
 *
 *
 *
 * DESCRIPTION:
 *
 * This file contains a number of mathematical constants and
 * also some needed size parameters of the computer arithmetic.
 * The values are supplied as arrays of hexadecimal integers
 * for IEEE arithmetic; arrays of octal constants for DEC
 * arithmetic; and in a normal decimal scientific notation for
 * other machines.  The particular notation used is determined
 * by a symbol (DEC, IBMPC, or UNK) defined in the include file
 * mconf.h.
 *
*/ 
/*
Cephes Math Library Release 2.3:  March, 1995
Copyright 1984, 1995 by Stephen L. Moshier
*/

#ifdef UNK
#if 1
double MACHEP =  1.11022302462515654042E-16;   /* 2**-53 */
#else
double MACHEP =  1.38777878078144567553E-17;   /* 2**-56 */
#endif
double UFLOWTHRESH =  2.22507385850720138309E-308; /* 2**-1022 */
#ifdef DENORMAL
double MAXLOG =  7.09782712893383996732E2;     /* log(MAXNUM) */
/* double MINLOG = -7.44440071921381262314E2; */     /* log(2**-1074) */
double MINLOG = -7.451332191019412076235E2;     /* log(2**-1075) */
#else
double MAXLOG =  7.08396418532264106224E2;     /* log 2**1022 */
double MINLOG = -7.08396418532264106224E2;     /* log 2**-1022 */
#endif
double MAXNUM =  1.79769313486231570815E308;    /* 2**1024*(1-MACHEP) */
double PI     =  3.14159265358979323846;       /* pi */
double PIO2   =  1.57079632679489661923;       /* pi/2 */
double PIO4   =  7.85398163397448309616E-1;    /* pi/4 */
double SQRT2  =  1.41421356237309504880;       /* sqrt(2) */
double SQRTH  =  7.07106781186547524401E-1;    /* sqrt(2)/2 */
double LOG2E  =  1.4426950408889634073599;     /* 1/log(2) */
double SQ2OPI =  7.9788456080286535587989E-1;  /* sqrt( 2/pi ) */
double LOGE2  =  6.93147180559945309417E-1;    /* log(2) */
double LOGSQ2 =  3.46573590279972654709E-1;    /* log(2)/2 */
double THPIO4 =  2.35619449019234492885;       /* 3*pi/4 */
double TWOOPI =  6.36619772367581343075535E-1; /* 2/pi */
#ifdef INFINITIES
double INFINITYN = 1.0/0.0;  /* 99e999; */
#else
double INFINITYN =  1.79769313486231570815E308;    /* 2**1024*(1-MACHEP) */
#endif
#ifdef NANNS
double NANN = 1.0/0.0 - 1.0/0.0;
#else
double NANN = 0.0;
#endif
#ifdef MINUSZERO
double NEGZERO = -0.0;
#else
double NEGZERO = 0.0;
#endif
#endif

#ifdef IBMPC
                       /* 2**-53 =  1.11022302462515654042E-16 */
unsigned short MACHEP[4] = {0x0000,0x0000,0x0000,0x3ca0};
unsigned short UFLOWTHRESH[4] = {0x0000,0x0000,0x0000,0x0010};
#ifdef DENORMAL
                       /* log(MAXNUM) =  7.09782712893383996732224E2 */
unsigned short MAXLOG[4] = {0x39ef,0xfefa,0x2e42,0x4086};
                       /* log(2**-1074) = - -7.44440071921381262314E2 */
/*unsigned short MINLOG[4] = {0x71c3,0x446d,0x4385,0xc087};*/
unsigned short MINLOG[4] = {0x3052,0xd52d,0x4910,0xc087};
#else
                       /* log(2**1022) =   7.08396418532264106224E2 */
unsigned short MAXLOG[4] = {0xbcd2,0xdd7a,0x232b,0x4086};
                       /* log(2**-1022) = - 7.08396418532264106224E2 */
unsigned short MINLOG[4] = {0xbcd2,0xdd7a,0x232b,0xc086};
#endif
                       /* 2**1024*(1-MACHEP) =  1.7976931348623158E308 */
unsigned short MAXNUM[4] = {0xffff,0xffff,0xffff,0x7fef};
unsigned short PI[4]     = {0x2d18,0x5444,0x21fb,0x4009};
unsigned short PIO2[4]   = {0x2d18,0x5444,0x21fb,0x3ff9};
unsigned short PIO4[4]   = {0x2d18,0x5444,0x21fb,0x3fe9};
unsigned short SQRT2[4]  = {0x3bcd,0x667f,0xa09e,0x3ff6};
unsigned short SQRTH[4]  = {0x3bcd,0x667f,0xa09e,0x3fe6};
unsigned short LOG2E[4]  = {0x82fe,0x652b,0x1547,0x3ff7};
unsigned short SQ2OPI[4] = {0x3651,0x33d4,0x8845,0x3fe9};
unsigned short LOGE2[4]  = {0x39ef,0xfefa,0x2e42,0x3fe6};
unsigned short LOGSQ2[4] = {0x39ef,0xfefa,0x2e42,0x3fd6};
unsigned short THPIO4[4] = {0x21d2,0x7f33,0xd97c,0x4002};
unsigned short TWOOPI[4] = {0xc883,0x6dc9,0x5f30,0x3fe4};
#ifdef INFINITIES
unsigned short INFINITYN[4] = {0x0000,0x0000,0x0000,0x7ff0};
#else
unsigned short INFINITYN[4] = {0xffff,0xffff,0xffff,0x7fef};
#endif
#ifdef NANNS
unsigned short NANN[4] = {0x0000,0x0000,0x0000,0x7ffc};
#else
unsigned short NANN[4] = {0x0000,0x0000,0x0000,0x0000};
#endif
#ifdef MINUSZERO
unsigned short NEGZERO[4] = {0x0000,0x0000,0x0000,0x8000};
#else
unsigned short NEGZERO[4] = {0x0000,0x0000,0x0000,0x0000};
#endif
#endif

#ifdef MIEEE
                       /* 2**-53 =  1.11022302462515654042E-16 */
unsigned short MACHEP[4] = {0x3ca0,0x0000,0x0000,0x0000};
unsigned short UFLOWTHRESH[4] = {0x0010,0x0000,0x0000,0x0000};
#ifdef DENORMAL
                       /* log(2**1024) =   7.09782712893383996843E2 */
unsigned short MAXLOG[4] = {0x4086,0x2e42,0xfefa,0x39ef};
                       /* log(2**-1074) = - -7.44440071921381262314E2 */
/* unsigned short MINLOG[4] = {0xc087,0x4385,0x446d,0x71c3}; */
unsigned short MINLOG[4] = {0xc087,0x4910,0xd52d,0x3052};
#else
                       /* log(2**1022) =  7.08396418532264106224E2 */
unsigned short MAXLOG[4] = {0x4086,0x232b,0xdd7a,0xbcd2};
                       /* log(2**-1022) = - 7.08396418532264106224E2 */
unsigned short MINLOG[4] = {0xc086,0x232b,0xdd7a,0xbcd2};
#endif
                       /* 2**1024*(1-MACHEP) =  1.7976931348623158E308 */
unsigned short MAXNUM[4] = {0x7fef,0xffff,0xffff,0xffff};
unsigned short PI[4]     = {0x4009,0x21fb,0x5444,0x2d18};
unsigned short PIO2[4]   = {0x3ff9,0x21fb,0x5444,0x2d18};
unsigned short PIO4[4]   = {0x3fe9,0x21fb,0x5444,0x2d18};
unsigned short SQRT2[4]  = {0x3ff6,0xa09e,0x667f,0x3bcd};
unsigned short SQRTH[4]  = {0x3fe6,0xa09e,0x667f,0x3bcd};
unsigned short LOG2E[4]  = {0x3ff7,0x1547,0x652b,0x82fe};
unsigned short SQ2OPI[4] = {0x3fe9,0x8845,0x33d4,0x3651};
unsigned short LOGE2[4]  = {0x3fe6,0x2e42,0xfefa,0x39ef};
unsigned short LOGSQ2[4] = {0x3fd6,0x2e42,0xfefa,0x39ef};
unsigned short THPIO4[4] = {0x4002,0xd97c,0x7f33,0x21d2};
unsigned short TWOOPI[4] = {0x3fe4,0x5f30,0x6dc9,0xc883};
#ifdef INFINITIES
unsigned short INFINITYN[4] = {0x7ff0,0x0000,0x0000,0x0000};
#else
unsigned short INFINITYN[4] = {0x7fef,0xffff,0xffff,0xffff};
#endif
#ifdef NANNS
unsigned short NANN[4] = {0x7ff8,0x0000,0x0000,0x0000};
#else
unsigned short NANN[4] = {0x0000,0x0000,0x0000,0x0000};
#endif
#ifdef MINUSZERO
unsigned short NEGZERO[4] = {0x8000,0x0000,0x0000,0x0000};
#else
unsigned short NEGZERO[4] = {0x0000,0x0000,0x0000,0x0000};
#endif
#endif

#ifdef DEC
                       /* 2**-56 =  1.38777878078144567553E-17 */
unsigned short MACHEP[4] = {0022200,0000000,0000000,0000000};
unsigned short UFLOWTHRESH[4] = {0x0080,0x0000,0x0000,0x0000};
                       /* log 2**127 = 88.029691931113054295988 */
unsigned short MAXLOG[4] = {041660,007463,0143742,025733,};
                       /* log 2**-128 = -88.72283911167299960540 */
unsigned short MINLOG[4] = {0141661,071027,0173721,0147572,};
                       /* 2**127 = 1.701411834604692317316873e38 */
unsigned short MAXNUM[4] = {077777,0177777,0177777,0177777,};
unsigned short PI[4]     = {040511,007732,0121041,064302,};
unsigned short PIO2[4]   = {040311,007732,0121041,064302,};
unsigned short PIO4[4]   = {040111,007732,0121041,064302,};
unsigned short SQRT2[4]  = {040265,002363,031771,0157145,};
unsigned short SQRTH[4]  = {040065,002363,031771,0157144,};
unsigned short LOG2E[4]  = {040270,0125073,024534,013761,};
unsigned short SQ2OPI[4] = {040114,041051,0117241,0131204,};
unsigned short LOGE2[4]  = {040061,071027,0173721,0147572,};
unsigned short LOGSQ2[4] = {037661,071027,0173721,0147572,};
unsigned short THPIO4[4] = {040426,0145743,0174631,007222,};
unsigned short TWOOPI[4] = {040042,0174603,067116,042025,};
/* Approximate infinity by MAXNUM.  */
unsigned short INFINITYN[4] = {077777,0177777,0177777,0177777,};
unsigned short NANN[4] = {0000000,0000000,0000000,0000000};
#ifdef MINUSZERO
unsigned short NEGZERO[4] = {0000000,0000000,0000000,0100000};
#else
unsigned short NEGZERO[4] = {0000000,0000000,0000000,0000000};
#endif
#endif

#ifndef UNK
extern unsigned short MACHEP[];
extern unsigned short UFLOWTHRESH[];
extern unsigned short MAXLOG[];
extern unsigned short UNDLOG[];
extern unsigned short MINLOG[];
extern unsigned short MAXNUM[];
extern unsigned short PI[];
extern unsigned short PIO2[];
extern unsigned short PIO4[];
extern unsigned short SQRT2[];
extern unsigned short SQRTH[];
extern unsigned short LOG2E[];
extern unsigned short SQ2OPI[];
extern unsigned short LOGE2[];
extern unsigned short LOGSQ2[];
extern unsigned short THPIO4[];
extern unsigned short TWOOPI[];
extern unsigned short INFINITYN[];
extern unsigned short NANN[];
extern unsigned short NEGZERO[];
#endif

extern double MACHEP, MAXLOG;
static double big = 4.503599627370496e15;
static double biginv =  2.22044604925031308085e-16;

#ifdef UNK
static double P[] = {
  1.60119522476751861407E-4,
  1.19135147006586384913E-3,
  1.04213797561761569935E-2,
  4.76367800457137231464E-2,
  2.07448227648435975150E-1,
  4.94214826801497100753E-1,
  9.99999999999999996796E-1
};
static double Q[] = {
-2.31581873324120129819E-5,
 5.39605580493303397842E-4,
-4.45641913851797240494E-3,
 1.18139785222060435552E-2,
 3.58236398605498653373E-2,
-2.34591795718243348568E-1,
 7.14304917030273074085E-2,
 1.00000000000000000320E0
};
#define MAXGAM 171.624376956302725
static double LOGPI = 1.14472988584940017414;
#endif

#ifdef DEC
static unsigned short P[] = {
0035047,0162701,0146301,0005234,
0035634,0023437,0032065,0176530,
0036452,0137157,0047330,0122574,
0037103,0017310,0143041,0017232,
0037524,0066516,0162563,0164605,
0037775,0004671,0146237,0014222,
0040200,0000000,0000000,0000000
};
static unsigned short Q[] = {
0134302,0041724,0020006,0116565,
0035415,0072121,0044251,0025634,
0136222,0003447,0035205,0121114,
0036501,0107552,0154335,0104271,
0037022,0135717,0014776,0171471,
0137560,0034324,0165024,0037021,
0037222,0045046,0047151,0161213,
0040200,0000000,0000000,0000000
};
#define MAXGAM 34.84425627277176174
static unsigned short LPI[4] = {
0040222,0103202,0043475,0006750,
};
#define LOGPI *(double *)LPI
#endif

#ifdef IBMPC
static unsigned short P[] = {
0x2153,0x3998,0xfcb8,0x3f24,
0xbfab,0xe686,0x84e3,0x3f53,
0x14b0,0xe9db,0x57cd,0x3f85,
0x23d3,0x18c4,0x63d9,0x3fa8,
0x7d31,0xdcae,0x8da9,0x3fca,
0xe312,0x3993,0xa137,0x3fdf,
0x0000,0x0000,0x0000,0x3ff0
};
static unsigned short Q[] = {
0xd3af,0x8400,0x487a,0xbef8,
0x2573,0x2915,0xae8a,0x3f41,
0xb44a,0xe750,0x40e4,0xbf72,
0xb117,0x5b1b,0x31ed,0x3f88,
0xde67,0xe33f,0x5779,0x3fa2,
0x87c2,0x9d42,0x071a,0xbfce,
0x3c51,0xc9cd,0x4944,0x3fb2,
0x0000,0x0000,0x0000,0x3ff0
};
#define MAXGAM 171.624376956302725
static unsigned short LPI[4] = {
0xa1bd,0x48e7,0x50d0,0x3ff2,
};
#define LOGPI *(double *)LPI
#endif

#ifdef MIEEE
static unsigned short P[] = {
0x3f24,0xfcb8,0x3998,0x2153,
0x3f53,0x84e3,0xe686,0xbfab,
0x3f85,0x57cd,0xe9db,0x14b0,
0x3fa8,0x63d9,0x18c4,0x23d3,
0x3fca,0x8da9,0xdcae,0x7d31,
0x3fdf,0xa137,0x3993,0xe312,
0x3ff0,0x0000,0x0000,0x0000
};
static unsigned short Q[] = {
0xbef8,0x487a,0x8400,0xd3af,
0x3f41,0xae8a,0x2915,0x2573,
0xbf72,0x40e4,0xe750,0xb44a,
0x3f88,0x31ed,0x5b1b,0xb117,
0x3fa2,0x5779,0xe33f,0xde67,
0xbfce,0x071a,0x9d42,0x87c2,
0x3fb2,0x4944,0xc9cd,0x3c51,
0x3ff0,0x0000,0x0000,0x0000
};
#define MAXGAM 171.624376956302725
static unsigned short LPI[4] = {
0x3ff2,0x50d0,0x48e7,0xa1bd,
};
#define LOGPI *(double *)LPI
#endif

/* Stirling's formula for the gamma function */
#if UNK
static double STIR[5] = {
 7.87311395793093628397E-4,
-2.29549961613378126380E-4,
-2.68132617805781232825E-3,
 3.47222221605458667310E-3,
 8.33333333333482257126E-2,
};
#define MAXSTIR 143.01608
static double SQTPI = 2.50662827463100050242E0;
#endif
#if DEC
static unsigned short STIR[20] = {
0035516,0061622,0144553,0112224,
0135160,0131531,0037460,0165740,
0136057,0134460,0037242,0077270,
0036143,0107070,0156306,0027751,
0037252,0125252,0125252,0146064,
};
#define MAXSTIR 26.77
static unsigned short SQT[4] = {
0040440,0066230,0177661,0034055,
};
#define SQTPI *(double *)SQT
#endif
#if IBMPC
static unsigned short STIR[20] = {
0x7293,0x592d,0xcc72,0x3f49,
0x1d7c,0x27e6,0x166b,0xbf2e,
0x4fd7,0x07d4,0xf726,0xbf65,
0xc5fd,0x1b98,0x71c7,0x3f6c,
0x5986,0x5555,0x5555,0x3fb5,
};
#define MAXSTIR 143.01608
static unsigned short SQT[4] = {
0x2706,0x1ff6,0x0d93,0x4004,
};
#define SQTPI *(double *)SQT
#endif
#if MIEEE
static unsigned short STIR[20] = {
0x3f49,0xcc72,0x592d,0x7293,
0xbf2e,0x166b,0x27e6,0x1d7c,
0xbf65,0xf726,0x07d4,0x4fd7,
0x3f6c,0x71c7,0x1b98,0xc5fd,
0x3fb5,0x5555,0x5555,0x5986,
};
#define MAXSTIR 143.01608
static unsigned short SQT[4] = {
0x4004,0x0d93,0x1ff6,0x2706,
};
#define SQTPI *(double *)SQT
#endif

int sgngam = 0;
extern int sgngam;
extern double MAXLOG, MAXNUM, PI;
#ifndef ANSIPROT
//double pow(), log(), exp(), sin(), polevl(), p1evl(), floor(), fabs();
double pow(), log(), exp(), sin(), polevl(), p1evl(), fabs();
//int isnan(val), isfinite(val);
#endif
#ifdef INFINITIES
extern double INFINITYN;
#endif
#ifdef NANNS
extern double NANN;
#endif


/*                                                     igamc()
 *
 *     Complemented incomplete gamma integral
 *
 *
 *
 * SYNOPSIS:
 *
 * double a, x, y, igamc();
 *
 * y = igamc( a, x );
 *
 * DESCRIPTION:
 *
 * The function is defined by
 *
 *
 *  igamc(a,x)   =   1 - igam(a,x)
 *
 *                            inf.
 *                              -
 *                     1       | |  -t  a-1
 *               =   -----     |   e   t   dt.
 *                    -      | |
 *                   | (a)    -
 *                             x
 *
 *
 * In this implementation both arguments must be positive.
 * The integral is evaluated by either a power series or
 * continued fraction expansion, depending on the relative
 * values of a and x.
 *
 * ACCURACY:
 *
 * Tested at random a, x.
 *                a         x                      Relative error:
 * arithmetic   domain   domain     # trials      peak         rms
 *    IEEE     0.5,100   0,100      200000       1.9e-14     1.7e-15
 *    IEEE     0.01,0.5  0,100      200000       1.4e-13     1.6e-15
 */

/*
Cephes Math Library Release 2.0:  April, 1987
Copyright 1985, 1987 by Stephen L. Moshier
Direct inquiries to 30 Frost Street, Cambridge, MA 02140
*/

// double igamc( a, x )
// double a, x;
double igamc( double a, double x )
{
double ans, ax, c, yc, r, t, y, z;
double pk, pkm1, pkm2, qk, qkm1, qkm2;

if( (x <= 0) || ( a <= 0) )
       return( 1.0 );

if( (x < 1.0) || (x < a) )
       return( 1.e0 - igam(a,x) );

ax = a * log(x) - x - lgam(a);
if( ax < -MAXLOG )
       {
       mtherr( "igamc", UNDERFLOW );
       return( 0.0 );
       }
ax = exp(ax);
/* continued fraction */
y = 1.0 - a;
z = x + y + 1.0;
c = 0.0;
pkm2 = 1.0;
qkm2 = x;
pkm1 = x + 1.0;
qkm1 = z * x;
ans = pkm1/qkm1;

do
       {
       c += 1.0;
       y += 1.0;
       z += 2.0;
       yc = y * c;
       pk = pkm1 * z  -  pkm2 * yc;
       qk = qkm1 * z  -  qkm2 * yc;
       if( qk != 0 )
               {
               r = pk/qk;
               t = fabs( (ans - r)/r );
               ans = r;
               }
       else
               t = 1.0;
       pkm2 = pkm1;
       pkm1 = pk;
       qkm2 = qkm1;
       qkm1 = qk;
       if( fabs(pk) > big )
               {
               pkm2 *= biginv;
               pkm1 *= biginv;
               qkm2 *= biginv;
               qkm1 *= biginv;
               }
       }
while( t > MACHEP );

return( ans * ax );
}


/* left tail of incomplete gamma function:
 *
 *          inf.      k
 *   a  -x   -       x
 *  x  e     >   ----------
 *           -     -
 *          k=0   | (a+k+1)
 *
 */

// double igam( a, x )
// double a, x;
double igam( double a, double x )
{
double ans, ax, c, r;

if( (x <= 0) || ( a <= 0) )
       return( 0.0 );

if( (x > 1.0) && (x > a ) )
       return( 1.e0 - igamc(a,x) );

/* Compute  x**a * exp(-x) / gamma(a)  */
ax = a * log(x) - x - lgam(a);
if( ax < -MAXLOG )
       {
       mtherr( "igam", UNDERFLOW );
       return( 0.0 );
       }
ax = exp(ax);

/* power series */
r = a;
c = 1.0;
ans = 1.0;

do
       {
       r += 1.0;
       c *= x/r;
       ans += c;
       }
while( c/ans > MACHEP );

return( ans * ax/a );
}



/* A[]: Stirling's formula expansion of log gamma
 * B[], C[]: log gamma function between 2 and 3
 */
#ifdef UNK
static double A[] = {
 8.11614167470508450300E-4,
-5.95061904284301438324E-4,
 7.93650340457716943945E-4,
-2.77777777730099687205E-3,
 8.33333333333331927722E-2
};
static double B[] = {
-1.37825152569120859100E3,
-3.88016315134637840924E4,
-3.31612992738871184744E5,
-1.16237097492762307383E6,
-1.72173700820839662146E6,
-8.53555664245765465627E5
};
static double C[] = {
/* 1.00000000000000000000E0, */
-3.51815701436523470549E2,
-1.70642106651881159223E4,
-2.20528590553854454839E5,
-1.13933444367982507207E6,
-2.53252307177582951285E6,
-2.01889141433532773231E6
};
/* log( sqrt( 2*pi ) ) */
static double LS2PI  =  0.91893853320467274178;
#define MAXLGM 2.556348e305
#endif

#ifdef DEC
static unsigned short A[] = {
0035524,0141201,0034633,0031405,
0135433,0176755,0126007,0045030,
0035520,0006371,0003342,0172730,
0136066,0005540,0132605,0026407,
0037252,0125252,0125252,0125132
};
static unsigned short B[] = {
0142654,0044014,0077633,0035410,
0144027,0110641,0125335,0144760,
0144641,0165637,0142204,0047447,
0145215,0162027,0146246,0155211,
0145322,0026110,0010317,0110130,
0145120,0061472,0120300,0025363
};
static unsigned short C[] = {
/*0040200,0000000,0000000,0000000*/
0142257,0164150,0163630,0112622,
0143605,0050153,0156116,0135272,
0144527,0056045,0145642,0062332,
0145213,0012063,0106250,0001025,
0145432,0111254,0044577,0115142,
0145366,0071133,0050217,0005122
};
/* log( sqrt( 2*pi ) ) */
static unsigned short LS2P[] = {040153,037616,041445,0172645,};
#define LS2PI *(double *)LS2P
#define MAXLGM 2.035093e36
#endif

#ifdef IBMPC
static unsigned short A[] = {
0x6661,0x2733,0x9850,0x3f4a,
0xe943,0xb580,0x7fbd,0xbf43,
0x5ebb,0x20dc,0x019f,0x3f4a,
0xa5a1,0x16b0,0xc16c,0xbf66,
0x554b,0x5555,0x5555,0x3fb5
};
static unsigned short B[] = {
0x6761,0x8ff3,0x8901,0xc095,
0xb93e,0x355b,0xf234,0xc0e2,
0x89e5,0xf890,0x3d73,0xc114,
0xdb51,0xf994,0xbc82,0xc131,
0xf20b,0x0219,0x4589,0xc13a,
0x055e,0x5418,0x0c67,0xc12a
};
static unsigned short C[] = {
/*0x0000,0x0000,0x0000,0x3ff0,*/
0x12b2,0x1cf3,0xfd0d,0xc075,
0xd757,0x7b89,0xaa0d,0xc0d0,
0x4c9b,0xb974,0xeb84,0xc10a,
0x0043,0x7195,0x6286,0xc131,
0xf34c,0x892f,0x5255,0xc143,
0xe14a,0x6a11,0xce4b,0xc13e
};
/* log( sqrt( 2*pi ) ) */
static unsigned short LS2P[] = {
0xbeb5,0xc864,0x67f1,0x3fed
};
#define LS2PI *(double *)LS2P
#define MAXLGM 2.556348e305
#endif

#ifdef MIEEE
static unsigned short A[] = {
0x3f4a,0x9850,0x2733,0x6661,
0xbf43,0x7fbd,0xb580,0xe943,
0x3f4a,0x019f,0x20dc,0x5ebb,
0xbf66,0xc16c,0x16b0,0xa5a1,
0x3fb5,0x5555,0x5555,0x554b
};
static unsigned short B[] = {
0xc095,0x8901,0x8ff3,0x6761,
0xc0e2,0xf234,0x355b,0xb93e,
0xc114,0x3d73,0xf890,0x89e5,
0xc131,0xbc82,0xf994,0xdb51,
0xc13a,0x4589,0x0219,0xf20b,
0xc12a,0x0c67,0x5418,0x055e
};
static unsigned short C[] = {
0xc075,0xfd0d,0x1cf3,0x12b2,
0xc0d0,0xaa0d,0x7b89,0xd757,
0xc10a,0xeb84,0xb974,0x4c9b,
0xc131,0x6286,0x7195,0x0043,
0xc143,0x5255,0x892f,0xf34c,
0xc13e,0xce4b,0x6a11,0xe14a
};
/* log( sqrt( 2*pi ) ) */
static unsigned short LS2P[] = {
0x3fed,0x67f1,0xc864,0xbeb5
};
#define LS2PI *(double *)LS2P
#define MAXLGM 2.556348e305
#endif


/* Logarithm of gamma function */


// double lgam(x)
// double x;
double lgam(double x)
{
double p, q, u, w, z;
int i;

sgngam = 1;
#ifdef NANNS
if( isnan(x) )
       return(x);
#endif

#ifdef INFINITIES
if( !isfinite(x) )
       return(INFINITYN);
#endif

if( x < -34.0 )
       {
       q = -x;
       w = lgam(q); /* note this modifies sgngam! */
       p = floor(q);
       if( p == q )
               {
lgsing:
#ifdef INFINITIES
               mtherr( "lgam", SING );
               return (INFINITYN);
#else
               goto loverf;
#endif
               }
       i = (int)p;
       if( (i & 1) == 0 )
               sgngam = -1;
       else
               sgngam = 1;
       z = q - p;
       if( z > 0.5 )
               {
               p += 1.0;
               z = p - q;
               }
       z = q * sin( PI * z );
       if( z == 0.0 )
               goto lgsing;
/*      z = log(PI) - log( z ) - w;*/
       z = LOGPI - log( z ) - w;
       return( z );
       }

if( x < 13.0 )
       {
       z = 1.0;
       p = 0.0;
       u = x;
       while( u >= 3.0 )
               {
               p -= 1.0;
               u = x + p;
               z *= u;
               }
       while( u < 2.0 )
               {
               if( u == 0.0 )
                       goto lgsing;
               z /= u;
               p += 1.0;
               u = x + p;
               }
       if( z < 0.0 )
               {
               sgngam = -1;
               z = -z;
               }
       else
               sgngam = 1;
       if( u == 2.0 )
               return( log(z) );
       p -= 2.0;
       x = x + p;
       p = x * polevl( x, B, 5 ) / p1evl( x, C, 6);
       return( log(z) + p );
       }

if( x > MAXLGM )
       {
#ifdef INFINITIES
       return( sgngam * INFINITYN );
#else
loverf:
       mtherr( "lgam", OVERFLOW );
       return( sgngam * MAXNUM );
#endif
       }

q = ( x - 0.5 ) * log(x) - x + LS2PI;
if( x > 1.0e8 )
       return( q );

p = 1.0/(x*x);
if( x >= 1000.0 )
       q += ((   7.9365079365079365079365e-4 * p
               - 2.7777777777777777777778e-3) *p
               + 0.0833333333333333333333) / x;
else
       q += polevl( p, A, 4 ) / x;
return( q );
}

/*                                                      mtherr.c
 *
 *     Library common error handling routine
 *
 *
 *
 * SYNOPSIS:
 *
 * char *fctnam;
 * int code;
 * int mtherr();
 *
 * mtherr( fctnam, code );
 *
 *
 *
 * DESCRIPTION:
 *
 * This routine may be called to report one of the following
 * error conditions (in the include file mconf.h).
 *
 *   Mnemonic        Value          Significance
 *
 *    DOMAIN            1       argument domain error
 *    SING              2       function singularity
 *    OVERFLOW          3       overflow range error
 *    UNDERFLOW         4       underflow range error
 *    TLOSS             5       total loss of precision
 *    PLOSS             6       partial loss of precision
 *    EDOM             33       Unix domain error code
 *    ERANGE           34       Unix range error code
 *
 * The default version of the file prints the function name,
 * passed to it by the pointer fctnam, followed by the
 * error condition.  The display is directed to the standard
 * output device.  The routine then returns to the calling
 * program.  Users may wish to modify the program to abort by
 * calling exit() under severe error conditions such as domain
 * errors.
 *
 * Since all error conditions pass control to this function,
 * the display may be easily changed, eliminated, or directed
 * to an error logging device.
 *
 * SEE ALSO:
 *
 * mconf.h
 *
 */

/*
Cephes Math Library Release 2.0:  April, 1987
Copyright 1984, 1987 by Stephen L. Moshier
Direct inquiries to 30 Frost Street, Cambridge, MA 02140
*/

// int mtherr( name, code )
// char *name;
// int code;
int mtherr( char *name, int code )
{

/* Display string passed by calling program,
 * which is supposed to be the name of the
 * function in which the error occurred:
 */
//printf( "\n%s ", name );

/* Set global error message word */
merror = code;

/* Display error message defined
 * by the code argument.
 */
if( (code <= 0) || (code >= 7) )
       code = 0;
//printf( "%s error\n", ermsg[code] );

/* Return to calling
 * program
 */
return( 0 );
}

/*                                                      polevl.c
 *                                                     p1evl.c
 *
 *     Evaluate polynomial
 *
 *
 *
 * SYNOPSIS:
 *
 * int N;
 * double x, y, coef[N+1], polevl[];
 *
 * y = polevl( x, coef, N );
 *
 *
 *
 * DESCRIPTION:
 *
 * Evaluates polynomial of degree N:
 *
 *                     2          N
 * y  =  C  + C x + C x  +...+ C x
 *        0    1     2          N
 *
 * Coefficients are stored in reverse order:
 *
 * coef[0] = C  , ..., coef[N] = C  .
 *            N                   0
 *
 *  The function p1evl() assumes that coef[N] = 1.0 and is
 * omitted from the array.  Its calling arguments are
 * otherwise the same as polevl().
 *
 *
 * SPEED:
 *
 * In the interest of speed, there are no checks for out
 * of bounds arithmetic.  This routine is used by most of
 * the functions in the library.  Depending on available
 * equipment features, the user may wish to rewrite the
 * program in microcode or assembly language.
 *
 */


/*
Cephes Math Library Release 2.1:  December, 1988
Copyright 1984, 1987, 1988 by Stephen L. Moshier
Direct inquiries to 30 Frost Street, Cambridge, MA 02140
*/

// double polevl( x, coef, N )
// double x;
// double coef[];
// int N;
double polevl( double x, double coef[], int N )
{
double ans;
int i;
double *p;

p = coef;
ans = *p++;
i = N;

do
       ans = ans * x  +  *p++;
while( --i );

return( ans );
}

/*                                                      p1evl() */
/*                                          N
 * Evaluate polynomial when coefficient of x  is 1.0.
 * Otherwise same as polevl.
 */

// double p1evl( x, coef, N )
// double x;
// double coef[];
// int N;
double p1evl( double x, double coef[], int N )
{
double ans;
double *p;
int i;

p = coef;
ans = x + *p++;
i = N-1;

do
       ans = ans * x  + *p++;
while( --i );

return( ans );
}

/* error function in double precision */

double erf(double x)
{
    int k;
    double w, t, y;
    static double a[65] = {
        5.958930743e-11, -1.13739022964e-9, 
        1.466005199839e-8, -1.635035446196e-7, 
        1.6461004480962e-6, -1.492559551950604e-5, 
        1.2055331122299265e-4, -8.548326981129666e-4, 
        0.00522397762482322257, -0.0268661706450773342, 
        0.11283791670954881569, -0.37612638903183748117, 
        1.12837916709551257377, 
        2.372510631e-11, -4.5493253732e-10, 
        5.90362766598e-9, -6.642090827576e-8, 
        6.7595634268133e-7, -6.21188515924e-6, 
        5.10388300970969e-5, -3.7015410692956173e-4, 
        0.00233307631218880978, -0.0125498847718219221, 
        0.05657061146827041994, -0.2137966477645600658, 
        0.84270079294971486929, 
        9.49905026e-12, -1.8310229805e-10, 
        2.39463074e-9, -2.721444369609e-8, 
        2.8045522331686e-7, -2.61830022482897e-6, 
        2.195455056768781e-5, -1.6358986921372656e-4, 
        0.00107052153564110318, -0.00608284718113590151, 
        0.02986978465246258244, -0.13055593046562267625, 
        0.67493323603965504676, 
        3.82722073e-12, -7.421598602e-11, 
        9.793057408e-10, -1.126008898854e-8, 
        1.1775134830784e-7, -1.1199275838265e-6, 
        9.62023443095201e-6, -7.404402135070773e-5, 
        5.0689993654144881e-4, -0.00307553051439272889, 
        0.01668977892553165586, -0.08548534594781312114, 
        0.56909076642393639985, 
        1.55296588e-12, -3.032205868e-11, 
        4.0424830707e-10, -4.71135111493e-9, 
        5.011915876293e-8, -4.8722516178974e-7, 
        4.30683284629395e-6, -3.445026145385764e-5, 
        2.4879276133931664e-4, -0.00162940941748079288, 
        0.00988786373932350462, -0.05962426839442303805, 
        0.49766113250947636708
    };
    static double b[65] = {
        -2.9734388465e-10, 2.69776334046e-9, 
        -6.40788827665e-9, -1.6678201321e-8, 
        -2.1854388148686e-7, 2.66246030457984e-6, 
        1.612722157047886e-5, -2.5616361025506629e-4, 
        1.5380842432375365e-4, 0.00815533022524927908, 
        -0.01402283663896319337, -0.19746892495383021487, 
        0.71511720328842845913, 
        -1.951073787e-11, -3.2302692214e-10, 
        5.22461866919e-9, 3.42940918551e-9, 
        -3.5772874310272e-7, 1.9999935792654e-7, 
        2.687044575042908e-5, -1.1843240273775776e-4, 
        -8.0991728956032271e-4, 0.00661062970502241174, 
        0.00909530922354827295, -0.2016007277849101314, 
        0.51169696718727644908, 
        3.147682272e-11, -4.8465972408e-10, 
        6.3675740242e-10, 3.377623323271e-8, 
        -1.5451139637086e-7, -2.03340624738438e-6, 
        1.947204525295057e-5, 2.854147231653228e-5, 
        -0.00101565063152200272, 0.00271187003520095655, 
        0.02328095035422810727, -0.16725021123116877197, 
        0.32490054966649436974, 
        2.31936337e-11, -6.303206648e-11, 
        -2.64888267434e-9, 2.050708040581e-8, 
        1.1371857327578e-7, -2.11211337219663e-6, 
        3.68797328322935e-6, 9.823686253424796e-5, 
        -6.5860243990455368e-4, -7.5285814895230877e-4, 
        0.02585434424202960464, -0.11637092784486193258, 
        0.18267336775296612024, 
        -3.67789363e-12, 2.0876046746e-10, 
        -1.93319027226e-9, -4.35953392472e-9, 
        1.8006992266137e-7, -7.8441223763969e-7, 
        -6.75407647949153e-6, 8.428418334440096e-5, 
        -1.7604388937031815e-4, -0.0023972961143507161, 
        0.0206412902387602297, -0.06905562880005864105, 
        0.09084526782065478489
    };

    w = x < 0 ? -x : x;
    if (w < 2.2) {
        t = w * w;
        k = (int) t;
        t -= k;
        k *= 13;
        y = ((((((((((((a[k] * t + a[k + 1]) * t + 
            a[k + 2]) * t + a[k + 3]) * t + a[k + 4]) * t + 
            a[k + 5]) * t + a[k + 6]) * t + a[k + 7]) * t + 
            a[k + 8]) * t + a[k + 9]) * t + a[k + 10]) * t + 
            a[k + 11]) * t + a[k + 12]) * w;
    } else if (w < 6.9) {
        k = (int) w;
        t = w - k;
        k = 13 * (k - 2);
        y = (((((((((((b[k] * t + b[k + 1]) * t + 
            b[k + 2]) * t + b[k + 3]) * t + b[k + 4]) * t + 
            b[k + 5]) * t + b[k + 6]) * t + b[k + 7]) * t + 
            b[k + 8]) * t + b[k + 9]) * t + b[k + 10]) * t + 
            b[k + 11]) * t + b[k + 12];
        y *= y;
        y *= y;
        y *= y;
        y = 1 - y * y;
    } else {
        y = 1;
    }
    return x < 0 ? -y : y;
}


/* error function in double precision */

double erfc(double x)
{
    double t, u, y;

    t = 3.97886080735226 / (fabs(x) + 3.97886080735226);
    u = t - 0.5;
    y = (((((((((0.00127109764952614092 * u + 1.19314022838340944e-4) * u - 
        0.003963850973605135) * u - 8.70779635317295828e-4) * u + 
        0.00773672528313526668) * u + 0.00383335126264887303) * u - 
        0.0127223813782122755) * u - 0.0133823644533460069) * u + 
        0.0161315329733252248) * u + 0.0390976845588484035) * u + 
        0.00249367200053503304;
    y = ((((((((((((y * u - 0.0838864557023001992) * u - 
        0.119463959964325415) * u + 0.0166207924969367356) * u + 
        0.357524274449531043) * u + 0.805276408752910567) * u + 
        1.18902982909273333) * u + 1.37040217682338167) * u + 
        1.31314653831023098) * u + 1.07925515155856677) * u + 
        0.774368199119538609) * u + 0.490165080585318424) * u + 
        0.275374741597376782) * t * exp(-x * x);
    return x < 0 ? 2 - y : y;
}


