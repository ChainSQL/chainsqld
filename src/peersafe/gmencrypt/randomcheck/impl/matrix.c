/* --------------------------------------------------------------------------

Origin:     http://www.DiceLock.org 
Programmer: Angel Ferr茅
Date:       September 2007
File:       matrix.c - vers.1.0.0

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
#include <stdlib.h>

//#include <gmencrypt/randomcheck/randTest.h>
//#include <gmencrypt/randomcheck/randCheck.h>
#include <peersafe/gmencrypt/randomcheck/matrix.h>

#define MIN(x,y) ((x) > (y) ? (y) : (x))

int computeRank(int M, int Q, OneBit** matrix)
{
   int i;
   int rank;
   int m = MIN(M,Q);

   /* FORWARD APPLICATION OF ELEMENTARY ROW OPERATIONS */ 
      
   for(i = 0; i < m-1; i++) {
      if (matrix[i][i].b == 1) 
         perform_elementary_row_operations(0, i, M, Q, matrix);
      else { 	/* matrix[i][i] = 0 */
         if (find_unit_element_and_swap(0, i, M, Q, matrix) == 1) 
            perform_elementary_row_operations(0, i, M, Q, matrix);
      }
   }

   /* BACKWARD APPLICATION OF ELEMENTARY ROW OPERATIONS */ 
   for(i = m-1; i > 0; i--) {
      if (matrix[i][i].b == 1)
         perform_elementary_row_operations(1, i, M, Q, matrix);
      else { 	/* matrix[i][i] = 0 */
         if (find_unit_element_and_swap(1, i, M, Q, matrix) == 1) 
            perform_elementary_row_operations(1, i, M, Q, matrix);
      }
   } 
   #ifdef BACK
   fprintf(output, "\n");
   display_matrix(M,Q, matrix);
   #endif
   rank = determine_rank(m,M,Q,matrix);
   return rank;
}

void perform_elementary_row_operations(int flag, int i, int M, int Q, 
			               OneBit** A)
{
  int j,k;

  switch(flag)
  { 
     case 0: for(j = i+1; j < M;  j++)
                if (A[j][i].b == 1) 
                   for(k = i; k < Q; k++) 
                      A[j][k].b = (A[j][k].b + A[i][k].b) % 2;
             break;
        
     case 1: for(j = i-1; j >= 0;  j--)
                if (A[j][i].b == 1)
                   for(k = 0; k < Q; k++)
                       A[j][k].b = (A[j][k].b + A[i][k].b) % 2;
             break;
  }
  return;
}

int find_unit_element_and_swap(int flag, int i, int M, int Q, OneBit** A)
{ 
  int index;
  int row_op = 0;

  switch(flag) 
  {
    case 0:  index = i+1;
             while ((index < M) && (A[index][i].b == 0)) 
                index++;
             if (index < M) 
                row_op = swap_rows(i,index,Q,A);
             break;
    case 1:
             index = i-1;
	     while ((index >= 0) && (A[index][i].b == 0)) 
	       index--;
	     if (index >= 0) 
                row_op = swap_rows(i,index,Q,A);
             break;
  }
  return row_op;
}

int swap_rows(int i, int index, int Q, OneBit** A)
{
  int p;
  OneBit temp;

  for(p = 0; p < Q; p++) {
     temp.b = A[i][p].b;
     A[i][p].b = A[index][p].b;
     A[index][p].b = temp.b;
  }
  return 1;
}

int determine_rank(int m, int M, int Q, OneBit** A)
{
   int i, j, rank, allZeroes;

   /* DETERMINE RANK, THAT IS, COUNT THE NUMBER OF NONZERO ROWS */

   rank = m;
   for(i = 0; i < M; i++) {
      allZeroes = 1; 
      for(j=0; j < Q; j++) {
         if (A[i][j].b == 1) {
            allZeroes = 0;
            break;
         }
      }
      if (allZeroes == 1) rank--;
   } 
   return rank;
}

OneBit** create_matrix(int M, int Q)
{
  int i;
  int j;
  OneBit **matrix;

  if ((matrix = (OneBit**) calloc(M, sizeof(OneBit *))) == NULL) 
  {
     return matrix;	
  }
  else 
  {
     for (i = 0; i < M; i++) 
	 {
        if ((matrix[i] = (OneBit*)calloc(Q, sizeof(OneBit))) == NULL) 
		{
			//释放已分配的内存空间
			for(j=0; j<i; j++)
			{
				free(matrix[j]);
			}

			free(matrix);

        	return NULL;
        }
     }

     return matrix;
  }
}

void def_matrix(OneBit* stream, int M, int Q, OneBit** m,int k)
{
  int   i,j;
  for (i = 0; i < M; i++) 
     for (j = 0; j < Q; j++) { 
         m[i][j].b = stream[k*(M*Q)+j+i*M].b;
     }
  return;
}

void delete_matrix(int M, OneBit** matrix)
{
  int i;
  for (i = 0; i < M; i++)
    free(matrix[i]);
  free(matrix);
}
