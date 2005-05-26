/*
 *  GQView
 *  (C) 2005 John Ellis
 *
 * This software is released under the GNU General Public License (GNU GPL).
 * Please read the included file COPYING for more information.
 * This software comes with no warranty of any kind, use at your own risk!
 *
 *
 * Code to add support for Canon CR2 and CRW files, version 0.2
 *
 * Developed by Daniel M. German, dmgerman at uvic.ca 
 *
 * you can find the sources for this patch at http://turingmachine.org/~dmg/libdcraw/gqview/
 *
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif


#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <glib.h>

#include "intl.h"

#include "format_canon.h"
#include "format_raw.h"


#if 0
  #define CANON_DEBUG
#endif

#ifdef CANON_DEBUG
int canonEnableDebug = 0;
/* This should be really a stack, but I am too lazy to implement */
#define DEBUG_ENABLE (canonEnableDebug = 0)
#define DEBUG_DISABLE (canonEnableDebug = 1)
/* It would be nice if these functions indented according to depth in the stack, but I am too lazy to implement */

#define DEBUG_ENTRY(a) (canonEnableDebug || fprintf(stderr, "Entering function: %s [%s:%d]\n", a, __FILE__, __LINE__))
#define DEBUG_EXIT(a) (canonEnableDebug || fprintf(stderr, "Exiting function: %s [%s:%d]\n", a, __FILE__, __LINE__))
#define DEBUG_1(a) (canonEnableDebug || fprintf(stderr, a " [%s:%d]\n", __FILE__, __LINE__))
#define DEBUG_2(a,b) (canonEnableDebug || fprintf(stderr, a " [%s:%d]\n",b,  __FILE__, __LINE__))
#define DEBUG_3(a,b,c) (canonEnableDebug || fprintf(stderr, a " [%s:%d]\n",b, c,  __FILE__, __LINE__))

#else
#define DEBUG_ENABLE
#define DEBUG_DISABLE 
#define DEBUG_ENTRY(a)
#define DEBUG_EXIT(a)

#define DEBUG_1(a) 
#define DEBUG_2(a,b)
#define DEBUG_3(a,b,c)
#endif


/* canon_read_int4 


The problem with gqview is that sometimes the data is to be read from
a file, and sometimes it is in memory. This function tries to isolate
the rest of the code from having to deal with both cases

This function reads a 4 byte unsigned integer, and fixes its endianism.

If fd >= 0 then the value is read from the corresponding file descriptor
   
   in that case, if offset is > 0, then the value is read from that offset

   otherwise it is read from the current file pointer 

if fd < 0 then the value is read from the memory pointed by data + offset


offset is a pointer to the actual offset of the file.

sizeInt can be 2 or 4 (it is the number of bytes to read)

RETURNS true is no error, false if it can't read the value


*/
static int canon_read_int(unsigned int *offset, const void *data, int sizeInt, unsigned int *value )
{
  DEBUG_DISABLE;

  DEBUG_ENTRY("canon_read_int");
  /* Verify values before we do anything */
  if (sizeInt != 2 && sizeInt != 4) return FALSE;
  if (offset == NULL) return FALSE;
  if (*offset <= 0) return FALSE;
  if (data == NULL) return FALSE;
  if (value == NULL) return FALSE;

  if (sizeInt == 4) {
    *value = GUINT32_FROM_LE(*(guint32*)(data + *offset));      
    *offset +=4;
    DEBUG_3("Read 4 bytes %d %x", *value, *value);
  } else {
    *value = GUINT16_FROM_LE(*(guint32*)(data + *offset));
    *offset +=2;
    DEBUG_3("Read 2 bytes %d %x", *value, *value);
  }

  DEBUG_EXIT("canon_read_int");

  DEBUG_ENABLE;
  return TRUE;
}

#define CANON_HEADER_SIZE                   26

/*

 The CR2 format is really a TIFF format. It is nicely documented in the TIFF V 6.0 document available from adobe.

  The CR2 file contains two thumbnails, one tiny and one decent sized. The record Id of the latter is 0x0111.

  The photo info is also available, in EXIF, and it looks like I don't need to do anything! Yeah!

*/

static int canon_cr2_process_directory(void *data, int offsetIFD, guint *jpegLocation, guint *exifLocation) 
{
  unsigned int offset;
  int returnValue = FALSE;

  DEBUG_ENTRY("canon_cr2_process_directory");

  /* The directory is a link list, after an array of records, the next 4 byptes point to the offset of the next directory.

  All offsets are absolution within the file (in CRWs the offsets are relative ).

  */

  while (offsetIFD != 0 && offsetIFD != 0xFFFF) {
    int countEntries=0;
    int i;
    /* Read directory, we start by reading number of entries in the directory */

    offset = offsetIFD;
    if (!canon_read_int(&offset, data, 2, &countEntries)) {
      goto return_only;
    }
    DEBUG_2("Number of entries: %d\n", countEntries);

    for (i=0;i<countEntries;i++) {
      /* read each entry */

      int recordId;
#if 0
      int format;
      int size;
#endif

      /* read record type */
      if (!canon_read_int(&offset, data, 2, &recordId)) {
	goto return_only;
      }

      /* Did we find the JPEG */
      if (recordId == 0x0111) { 
	DEBUG_1("This is the record to find**********************\n");
	offset +=6;
	if (!canon_read_int(&offset, data, 4, jpegLocation)) {
	  goto return_only;
	}
	DEBUG_3("JPEG Location %d 0x%x\n", *jpegLocation, *jpegLocation);
	/* We don't want to keep reading, because there is another
	   0x0111 record at the end that contains the raw data */
	returnValue = TRUE;
	goto return_only;
      } else {
	/* advance pointer by skipping rest of record */
	offset += 10;
      }
    }
    /* The next 4 bytes are the offset of next directory, if zero we are done
       
     */
    if (!canon_read_int(&offset, data, 4, &offsetIFD)) {
      goto return_only;
    }
    DEBUG_3("Value of NEXT offsetIFD: %d 0x%x\n", offsetIFD, offsetIFD);
  }

  returnValue = TRUE;
  DEBUG_1("Going to return true");

 return_only:
  DEBUG_EXIT("canon_cr2_process_directory");

  return TRUE;


}


static int format_raw_test_canon_cr2(void *data, const guint len,
				     guint *image_offset, guint *exif_offset)
{
#if 0
  char signature[4];
  unsigned int offset = 4;
#endif
  int offsetIFD;
  int returnValue = FALSE;
  void *jpgInDataOffset;

  DEBUG_ENTRY("format_raw_test_canon_cr2");

  /* Verify signature */
  if (memcmp(data, "\x49\x49\x2a\00", 4) != 0) {
    DEBUG_1("This is not a CR2");
    goto return_only;
  }

  /* Get address of first directory */
  offsetIFD = GUINT32_FROM_LE(*(guint32*)(data + 4));


  DEBUG_2("Value of offsetIFD: %d\n", offsetIFD);

  returnValue = canon_cr2_process_directory(data, offsetIFD, image_offset, exif_offset);

  if (returnValue) {
    jpgInDataOffset = data + *image_offset;

    /* Make sure we really got a JPEG */

    if (memcmp(jpgInDataOffset, "\xff\xd8",2) != 0) {
      /* It is not at the JPEG! */
      DEBUG_2("THis is not a jpeg after all: there are the first 4 bytes 0x%x ", (int)jpgInDataOffset);
      returnValue = FALSE;
    }
  }

return_only:
  DEBUG_EXIT("format_raw_test_canon_cr2");

  return returnValue;
}


gint format_raw_test_canon(const void *data, const guint len,
			   guint *image_offset, guint *exif_offset)
{


  /* There are at least 2 types of Canon raw files. CRW and CR2 

  CRW files have a proprietary format. 

  HEADER
  Heap
    RAW   data
    JPEG  data
    PHoto data

  HEADER_LENGTH            32  bytes
   int2     byteOrder; Always II (MM Motorola ---big endian, II Intel --little endian)
   int4     length;    Should be 26 
   char     identifier[8];type HEAP, subtype heap  CCDR
   int2     version;
   int2     subversion;
   char     unused[14]; 
  */

  int returnValue = FALSE;
  int heapHeaderOffset = 0;
  int heapRecordsCount = 0;
#if 0
  guint32 rawInt4;
  guint16 rawInt2;
#endif
  int i;
  unsigned int currentOffset;
  /* File has to be little endian, first two bytes II */

  if (len < 100) 
    return FALSE;

  if (format_raw_test_canon_cr2((void *)data, len, image_offset, exif_offset)) {
    return TRUE;
  }

  if (memcmp("II", data, 2) != 0) {
    return FALSE;
  }
  /* NO DEBUG BEFORE THIS POINT, we want to debug only Canon */
  
  DEBUG_ENTRY("format_raw_test_canon");

  DEBUG_2("Length of buffer read %u", len);

  DEBUG_2("CRW header length Data %d", GUINT32_FROM_LE(*(guint32*)(data + 2)));

  /* the length has to be CANON_HEADER_SIZE  */
  if (GUINT32_FROM_LE(*(guint32*)(data + 2)) != CANON_HEADER_SIZE) {
    DEBUG_1("It is not the right size");
    goto return_only;
  }
  
  if (!memcmp("HEAPCCDR", data+6, 8) == 0) {
    DEBUG_1("This file is not a Canon CRW raw photo");
    goto return_only;

   }
   
  /* Ok, so now we know that this is a CRW file */

  /* The heap is a strange data structure. It is recursive, so a record
    can contain a heap itself. That is indeed the case for the photo information
    reecord. Luckily the first heap contains the jpeg, so we don't need to do
    any recursive processing. 

    Its "header" is a the end. The header is a sequence of records,
     and the data of each record is at the beginning of the heap

   +-----------------+
   | data raw        |
   +-----------------+
   | data jpeg       |
   +-----------------+
   | data photo info |
   +-----------------+
   |header of heap   |
   | # records       |   it should be 3
   |      raw info   |
   |      jpeg info  |
   |      photo info |
   +-----------------+

   The header contains 
      number of records: 2 bytes
      for each record (10 bytes long)
          type:    2 bytes
          length:  4 bytes 
          offset:  4 bytes 
	  
     In some records the length and offset are actually data,
     but none for the ones in the first heap.
     
     the offset is with respect to the beginning of the heap, not the
     beginning of the file. That allows heaps to be "movable"

   For the purpose of finding the JPEG, all we need is to scan the fist heap,
   which contains the following record types:

    0x2005 Record RAW data
    0x2007 Record JPEG data
    0x300a Record with photo info

  */


  if (len < 0x10000) {
    DEBUG_2("We have a problem, the length is too small %d ", len);
    goto return_only;
  }
  currentOffset = len-4;


  /* The last 4 bytes have the offset of the header of the heap */
  if (!canon_read_int(&currentOffset, data, 4, &heapHeaderOffset)) 
    goto return_only;
  
  /* The heapoffset has to be adjusted to the actual file size, the header is CANON_HEADER_SIZE bytes long */
  heapHeaderOffset += CANON_HEADER_SIZE;
  DEBUG_2("heap header Offset %d ", heapHeaderOffset);
  
  /* Just check, it does not hurt, we don't want to crash */
  if (heapHeaderOffset > len) 
    goto return_only;

  currentOffset =   heapHeaderOffset;
  /* Let us read the number of records in the heap */
  if (!canon_read_int(&currentOffset, data, 2, &heapRecordsCount))
    goto return_only;
  
  DEBUG_2("heap record count %d ", heapRecordsCount);
    
  if (heapRecordsCount != 3) {
    /* In all the cameras I have seen, this is always 3
       if not, something is wrong, so just quit */
    goto return_only;
  }
    
  for (i=0;i<3;i++) {
    int recordType;
    int recordOffset;
    int recordLength;
    const void *jpgInDataOffset;
    /* Read each record, to find jpg, it should be second */
    
    if (!canon_read_int(&currentOffset, data, 2, &recordType))
      goto return_only;
    
    DEBUG_2("record type 0x%x ", recordType);
    
    if (recordType != 0x2007) {
      /* Go to the next record, don't waste time, 
	 but first, eat 8 bytes from header */
      currentOffset += 8;
      continue; /* Nah, wrong record, go to next */
    }
    /* Bingo, we are at the JPEG record */
    
    /* Read length */
    if (!canon_read_int(&currentOffset, data, 4, &recordLength))
      goto return_only;
    
    DEBUG_2("record length %d ", recordLength);
    
    /* Read offset */
    
    if (!canon_read_int(&currentOffset, data, 4, &recordOffset))
      goto return_only;
    
    DEBUG_2("record offset 0x%d ", recordOffset);
    
    /* Great, we now know where the JPEG is! 
       it is CANON_HEADER_SIZE (size of CRW header) + recordOffset 
    */
    
    *image_offset =  CANON_HEADER_SIZE + recordOffset;
    DEBUG_2("image offset %d ", *image_offset);
    
    /* keep checking for potential errors */
    if (*image_offset > len) {
      goto return_only;
    }
    /* Get the JPEG is */
    
    jpgInDataOffset = data + *image_offset;

    if (memcmp(jpgInDataOffset, "\xff\xd8\xff\xdb",4) != 0) {
      /* It is not at the JPEG! */
      DEBUG_2("THis is not a jpeg after all: there are the first 4 bytes 0x%x ", (int)jpgInDataOffset);
      goto return_only;
    }
    returnValue = TRUE;
    goto return_only;
  }
 /* undo whatever we need in case of an error*/
  DEBUG_1("We scan all records, but nothing was found!!!!!!!!!!!!!!!!!!");


  /* At this point we are returning */
return_only:
  if (returnValue) {
    DEBUG_1("****We got an embedded  JPEG for a canon CRW");

  }

  DEBUG_EXIT("format_raw_test_canon");
  return returnValue;

#undef DEBUG_2
#undef DEBUG
#undef DEBUG_ENTRY
#undef DEBUG_EXIT

}


