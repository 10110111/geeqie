#ifndef JPEG_PARSER_H
#define JPEG_PARSER_H

#define JPEG_MARKER		0xFF
#define JPEG_MARKER_SOI		0xD8
#define JPEG_MARKER_EOI		0xD9
#define JPEG_MARKER_APP1	0xE1
#define JPEG_MARKER_APP2	0xE2

/* jpeg container format:
     all data markers start with 0XFF
     2 byte long file start and end markers: 0xFFD8(SOI) and 0XFFD9(EOI)
     4 byte long data segment markers in format: 0xFFTTSSSSNNN...
       FF:   1 byte standard marker identifier
       TT:   1 byte data type
       SSSS: 2 bytes in Motorola byte alignment for length of the data.
	     This value includes these 2 bytes in the count, making actual
	     length of NN... == SSSS - 2.
       NNN.: the data in this segment
 */

gboolean jpeg_segment_find(guchar *data, guint size,
			    guchar app_marker, const gchar *magic, guint magic_len,
			    guint *seg_offset, guint *seg_length);

#endif