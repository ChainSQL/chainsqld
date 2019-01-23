/* --------------------------------------------------------------------------

Origin:     http://www.DiceLock.org 
Programmer: Angel Ferré
Date:       September 2007
File:       config.h - vers.1.0.0

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


/*
 * config.h
 */

#ifndef CONFIG_H
#define	CONFIG_H

/*
 *  EDIT THE FOLLOWING DEFINE BEFORE COMPILING
 *  TO SET THE COMPILATION ENVIRONMENT.
 *   (Select: MSDOS, WINDOWS31, UNIX)
 */

//#define	UNIX 1

#define	WINDOWS31 1 

/*
 * AUTO DEFINES (DON'T TOUCH!) 
 */
typedef unsigned char	BYTE;
typedef unsigned short	USHORT;
typedef unsigned int	UINT;
typedef unsigned long	ULONG;
typedef USHORT		DIGIT;	/* 16-bit word */
typedef ULONG		DBLWORD;  /* 32-bit word */
typedef char		*CSTR;
typedef unsigned char	*BSTR;
typedef unsigned short int	UINT16;
typedef unsigned long int	UINT32;

#define NULLPTR ((void *) 0)

#ifdef UNIX
#ifndef BIG_ENDIAN
#define	BIG_ENDIAN
#endif
#undef	DEC
#define	_far
#define	HILOW
#define	HILO
#define	TRYSOFIRST
#define	CHECKTIMEOUT
#define	REGDEMO
#define	EXPORT
#endif

#ifdef WINDOWS31
#define	LITTLE_ENDIAN
#define	TRYSOFIRST
#define	CHECKTIMEOUT
#define	REGDEMO
#define	EXPORT	_far _pascal _export
#endif

/*
#if PROTOTYPES
#define _PARAMS(list) list
#else
#define _PARAMS(list) ()
#endif
*/

#define SACSVERSION	1
#endif
