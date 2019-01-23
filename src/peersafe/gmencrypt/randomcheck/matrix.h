/* --------------------------------------------------------------------------

Origin:     http://www.DiceLock.org 
Programmer: Angel Ferré
Date:       September 2007
File:       matrix.h - vers.1.0.0

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


#ifndef MATRIX_H_
#define MATRIX_H_

//#include "randTest.h"
//#include "randCheck.h"

//一比特
typedef struct {
	unsigned char b : 1; //defines ONE bit
}OneBit;

void       perform_elementary_row_operations(int,int,int,int,OneBit**);
int        find_unit_element_and_swap(int,int,int,int, OneBit**);
int        swap_rows(int, int, int, OneBit**);
int        determine_rank(int, int, int, OneBit**);
int        computeRank(int,int,OneBit**);
void       define_matrix(int,int,int,OneBit**);
OneBit**   create_matrix(int, int);
void       def_matrix(OneBit*, int, int, OneBit**, int);
void       delete_matrix(int, OneBit**);

#endif /*MATRIX_H_*/
