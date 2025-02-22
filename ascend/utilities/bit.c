/*
 *  Bit List Manipulation Routines
 *  by Tom Epperly
 *  Version: $Revision: 1.8 $
 *  Version control file: $RCSfile: bit.c,v $
 *  Date last modified: $Date: 1997/09/08 18:07:31 $
 *  Last modified by: $Author: ballan $
 *
 *  This file is part of the Ascend Language Interpreter.
 *
 *  Copyright (C) 1990, 1993, 1994 Thomas Guthrie Epperly
 *
 *  The Ascend Language Interpreter is free software; you can redistribute
 *  it and/or modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  The Ascend Language Interpreter is distributed in hope that it will be
 *  useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "bit.h"

#include <stdio.h>
#include <assert.h>
#include <ascend/general/platform.h>

#include <ascend/general/ascMalloc.h>
#include <ascend/general/panic.h>

#define BLENGTH(bl) ((bl)->length)

typedef unsigned char byte;


struct BitList *CreateBList(unsigned long int len){
  struct BitList *result;
  unsigned long num_bytes;
  char *ptr;
  num_bytes = len >> 3;		/* divide by 8 */
  if(len & 0x07)num_bytes++;
  //CONSOLE_DEBUG("num_bytes = %lu",num_bytes);
  result = 
    (struct BitList *)ascmalloc((unsigned)(sizeof(struct BitList)+num_bytes));
  result->length = len;
  /* clear the memory */
  ptr = (char *)((asc_intptr_t)result+(asc_intptr_t)sizeof(struct BitList));
  ascbzero(ptr,(int)num_bytes);
  AssertAllocatedMemory((char *)result,sizeof(struct BitList)+num_bytes);
  return result;
}


struct BitList *CreateFBList(unsigned long int len){
  struct BitList *result;
  unsigned long c;
  result = CreateBList(len);
  AssertContainedMemory(result,sizeof(struct BitList));
  /* the following should be memset, plus cleanup of unused bits */
  for(c=0;c<len;SetBit(result,c++));
  return result;
}


struct BitList *ExpandBList(struct BitList *bl, unsigned long int len){
  unsigned orig_len,old_bytes,new_bytes;
  struct BitList *result;
  AssertContainedMemory(bl,sizeof(struct BitList));
  orig_len = BLength(bl);
  assert(len>=orig_len);
  old_bytes = orig_len >> 3;
  if (orig_len & 0x07) old_bytes++;
  new_bytes = len >> 3;
  if (len & 0x07) new_bytes++;
  if (new_bytes > old_bytes) {
    result =
      (struct BitList *)
	ascrealloc((char *)bl,(unsigned)(new_bytes+sizeof(struct BitList)));
    ascbzero((char *)((asc_intptr_t)result+sizeof(struct BitList)+
		      (asc_intptr_t)old_bytes),(int)(new_bytes-old_bytes));
  }
  else result = bl;
  AssertAllocatedMemory(result,sizeof(struct BitList)+new_bytes);
  result->length = len;
  return result;
}


struct BitList *ExpandFBList(struct BitList *bl, unsigned long int len){
  unsigned long oldlen;
  AssertContainedMemory(bl,sizeof(struct BitList));
  oldlen = BLength(bl);
  bl = ExpandBList(bl,len);
  while(oldlen<len) SetBit(bl,oldlen++);
  AssertContainedMemory(bl,sizeof(struct BitList));
  return bl;
}


void DestroyBList(struct BitList *bl){
  AssertContainedMemory(bl,sizeof(struct BitList));
  bl->length = 0;
  ascfree((char *)bl);
}


struct BitList *CopyBList(CONST struct BitList *bl){
  struct BitList *copy;
  unsigned long num_bytes;
  num_bytes = BLENGTH(bl) >> 3;	/* divide by 8 */
  if (BLENGTH(bl) & 0x07) num_bytes++;
  AssertAllocatedMemory(bl,sizeof(struct BitList)+num_bytes);
  copy =
    (struct BitList *)ascmalloc((unsigned)(sizeof(struct BitList)+num_bytes));
  ascbcopy((char *)bl,(char *)copy,(int)(sizeof(struct BitList)+num_bytes));
  AssertAllocatedMemory(bl,sizeof(struct BitList)+num_bytes);
  return copy;
}


void OverwriteBList(CONST struct BitList *bl, struct BitList *target){
  unsigned long num_bytes;

  assert(bl!=target);
  assert(bl!=NULL);
  assert(target!=NULL);

  if (BLENGTH(bl) != BLENGTH(target)) {
    ASC_PANIC("src and target bitlists of uneven size");
  }
  num_bytes = BLENGTH(bl) >> 3;	/* divide by 8- the assumed bits/char */
  if (BLENGTH(bl) & 0x07) num_bytes++;

  AssertAllocatedMemory(bl,sizeof(struct BitList)+num_bytes);
  AssertAllocatedMemory(target,sizeof(struct BitList)+num_bytes);

  ascbcopy((char *)bl,(char *)target,(int)(sizeof(struct BitList)+num_bytes));
}


unsigned long BitListBytes(CONST struct BitList *bl){
  unsigned long num_bytes;
  if (bl==NULL) return 0;
  num_bytes = BLENGTH(bl) >> 3; /* divide by 8 */
  if (BLENGTH(bl) & 0x07) num_bytes++;
  return (sizeof(struct BitList)+num_bytes);
}


void SetBit(struct BitList *bl, unsigned long int pos){
  byte *ptr;
  unsigned bit;
  asc_assert(pos < bl->length);
  AssertContainedMemory(bl,sizeof(struct BitList));
  ptr = (byte *)((asc_intptr_t)bl+sizeof(struct BitList)+(pos >> 3));
  AssertContainedIn(bl,ptr);
  bit = pos & 0x07;
  *ptr = *ptr | (byte)(1 << bit);
}


void ClearBit(struct BitList *bl, unsigned long int pos){
  byte *ptr;
  unsigned bit;
  AssertContainedMemory(bl,sizeof(struct BitList));
  asc_assert(pos < bl->length);
  ptr = (byte *)((asc_intptr_t)bl+sizeof(struct BitList)+(pos >> 3));
  AssertContainedIn(bl,ptr);
  bit = pos & 0x07;
  *ptr = *ptr & (byte)(~(1 << bit));
}


void CondSetBit(struct BitList *bl, unsigned long int pos, int cond){
  byte *ptr;
  unsigned bit;
  AssertContainedMemory(bl,sizeof(struct BitList));
  asc_assert(pos < bl->length);
  ptr = (byte *)((asc_intptr_t)bl+sizeof(struct BitList)+(pos >> 3));
  AssertContainedIn(bl,ptr);
  bit = pos & 0x07;
  if(cond)
    *ptr = *ptr | (byte)(1 << bit);
  else
    *ptr = *ptr & (byte)(~(1 << bit));
}


int ReadBit(CONST struct BitList *bl, unsigned long int pos){
  byte *ptr;
  unsigned bit;
  AssertContainedMemory(bl,sizeof(struct BitList));
  asc_assert(pos < bl->length);
  ptr = (byte *)((asc_intptr_t)bl+sizeof(struct BitList)+(pos >> 3));
  AssertContainedIn(bl,ptr);
  bit = pos & 0x07;
  //CONSOLE_DEBUG("--->reading pos = %lu (byte %lu, bit %u)",pos,(pos>>3),bit);
  //CONSOLE_DEBUG("value = %d",(*ptr & (1<<bit)));
  return (*ptr & (1 << bit));
}


void IntersectBLists(struct BitList *bl1, CONST struct BitList *bl2){
  byte *ptr1;
  CONST byte *ptr2;
  unsigned long num_bytes;
  AssertContainedMemory(bl1,sizeof(struct BitList));
  AssertContainedMemory(bl2,sizeof(struct BitList));
  if (BLENGTH(bl1)!=BLENGTH(bl2)) {
    FPRINTF(ASCERR,"Bad bit list intersection\n");
    return;
  }
  ptr1 = (byte *)((asc_intptr_t)bl1+sizeof(struct BitList));
  ptr2 = (byte *)((asc_intptr_t)bl2+sizeof(struct BitList));
  num_bytes = BLENGTH(bl1) >> 3; /* divided by 8 */
  if (BLENGTH(bl1) & 0x07) num_bytes++;
  while(num_bytes-->0) {
    AssertContainedIn(bl1,ptr1);
    AssertContainedIn(bl2,ptr2);
    *ptr1 = *ptr1 & *ptr2;
    ptr1++;
    ptr2++;
  }
}


void UnionBLists(struct BitList *bl1, CONST struct BitList *bl2){
  byte *ptr1;
  CONST byte *ptr2;
  unsigned long num_bytes;
  AssertContainedMemory(bl1,sizeof(struct BitList));
  AssertContainedMemory(bl2,sizeof(struct BitList));
  if (BLENGTH(bl1)!=BLENGTH(bl2)) {
    FPRINTF(ASCERR,"Bad bit list union\n");
    return;
  }
  ptr1 = (byte *)((asc_intptr_t)bl1+sizeof(struct BitList));
  ptr2 = (byte *)((asc_intptr_t)bl2+sizeof(struct BitList));
  num_bytes = BLENGTH(bl1) >> 3; /* divided by 8 */
  if (BLENGTH(bl1) & 0x07) num_bytes++;
  while(num_bytes-->0) {
    *ptr1 = *ptr1 | *ptr2;
    AssertContainedIn(bl1,ptr1);
    AssertContainedIn(bl2,ptr2);
    ptr1++;
    ptr2++;
  }
}


unsigned long BLengthF(CONST struct BitList *bl){
  AssertContainedMemory(bl,sizeof(struct BitList));
  return bl->length;
}


int BitListEmpty(CONST struct BitList *bl){
  unsigned long num_bytes;
  unsigned char *ptr;
  AssertContainedMemory(bl,sizeof(struct BitList));
  num_bytes = bl->length >> 3;
  if (bl->length & 0x07) num_bytes++;
  ptr = (unsigned char *)((asc_intptr_t)bl+
			  (unsigned long)sizeof(struct BitList));
  while(num_bytes--){
    AssertContainedIn(bl,ptr);
    if (*(ptr++)) return 0;
  }
  return 1;
}


int CompBList(struct BitList *b1, struct BitList *b2){
  unsigned long c,len;
  len = BLength(b1);
  /* note, corrected the iteration limits here! -- JP */
  for(c=0;c<len;c++){
    if(ReadBit(b1,c)!=ReadBit(b2,c)) {
    return 0;
    }
  }
  return 1;
}

#if 0
/* for debugging FirstNonZeroBit */
static void printbits(const struct BitList *bl){
  unsigned long i;
  ASC_FPRINTF(stderr,"bits: [");
  for(i=0;i<bl->length;i++){
    ASC_FPRINTF(stderr,"%c%s",ReadBit(bl,i)?'1':'.', (i+1)%8==0?"|":"" );
  }
  ASC_FPRINTF(stderr,"]\n");
}
#endif

unsigned long FirstNonZeroBit(CONST struct BitList *bl){
  unsigned long num_bytes,c,b;
  unsigned char *ptr,ch;

  //printbits(bl);

  num_bytes = bl->length >> 3;
  if (bl->length & 0x07) num_bytes++;
  AssertAllocatedMemory(bl,sizeof(struct BitList)+num_bytes);
  ptr = (unsigned char *)bl+(unsigned long)sizeof(struct BitList);

  c = num_bytes;

  /* find the first non-zero byte */
  for(c=0;c<num_bytes;c++,ptr++){
	if(*ptr)break;
  }
  if(c==num_bytes)return bl->length+1;
  /* now get the bit position within that byte */
  ch = *ptr;
  c <<= 3; /* multiply by 8 */
  for(b=0;b<8;b++){
	if(ch & (1<<b))break;
  }
  return c+b;
}
