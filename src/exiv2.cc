
#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#ifdef HAVE_EXIV2

#include <exiv2/image.hpp>
#include <exiv2/exif.hpp>
#include <iostream>

extern "C" {

#include <glib.h> 
#include "exif.h"


struct _ExifData
{
	Exiv2::ExifData exifData;
	Exiv2::ExifData::const_iterator iter;
};


ExifData *exif_read(gchar *path, gint parse_color_profile)
{
	try {
		ExifData *exif = g_new0(ExifData, 1);
	
		Exiv2::Image::AutoPtr image = Exiv2::ImageFactory::open(path);
		g_assert (image.get() != 0);
		image->readMetadata();
		exif->exifData = image->exifData();
		return exif;
	}
	catch (Exiv2::AnyError& e) {
		std::cout << "Caught Exiv2 exception '" << e << "'\n";
		return 0;
	}
	
}

void exif_free(ExifData *exif)
{
}


gchar *exif_get_data_as_text(ExifData *exif, const gchar *key)
{
	return g_strdup(exif->exifData[key].toString().c_str());
}

gint exif_get_integer(ExifData *exif, const gchar *key, gint *value)
{
	return exif->exifData[key].toLong();
}

ExifRational *exif_get_rational(ExifData *exif, const gchar *key, gint *sign)
{
/*	Exiv2::Rational v = exif->exifData[key];
	ExifRational *ret = 
	return exif->exifData[key];
*/
}

double exif_rational_to_double(ExifRational *r, gint sign)
{
	if (!r || r->den == 0.0) return 0.0;

	if (sign) return (double)((int)r->num) / (double)((int)r->den);
	return (double)r->num / r->den;
}

ExifItem *exif_get_item(ExifData *exif, const gchar *key)
{
	Exiv2::Exifdatum *item = &exif->exifData[key];
	return (ExifItem *)item;
}

ExifItem *exif_get_first_item(ExifData *exif)
{
	exif->iter = exif->exifData.begin();
	if (exif->iter == exif->exifData.end()) return NULL;
	const Exiv2::Exifdatum *item = &*exif->iter;
	return (ExifItem *)item;
}

ExifItem *exif_get_next_item(ExifData *exif)
{
	exif->iter++;
	if (exif->iter == exif->exifData.end()) return NULL;
	const Exiv2::Exifdatum *item = &*exif->iter;
	return (ExifItem *)item;
}

const char *exif_item_get_tag_name(ExifItem *item)
{
	return ((Exiv2::Exifdatum *)item)->tagName().c_str();
}

guint exif_item_get_tag_id(ExifItem *item)
{
	return ((Exiv2::Exifdatum *)item)->idx();
}

guint exif_item_get_elements(ExifItem *item)
{
	return ((Exiv2::Exifdatum *)item)->count();
}

char *exif_item_get_data(ExifItem *item, guint *data_len)
{
}

const char *exif_item_get_description(ExifItem *item)
{
	return ((Exiv2::Exifdatum *)item)->tagLabel().c_str();
}

/*
invalidTypeId, unsignedByte, asciiString, unsignedShort,
  unsignedLong, unsignedRational, signedByte, undefined,
  signedShort, signedLong, signedRational, string,
  date, time, comment, directory,
  xmpText, xmpAlt, xmpBag, xmpSeq,
  langAlt, lastTypeId 

	EXIF_FORMAT_UNKNOWN		= 0,
	EXIF_FORMAT_BYTE_UNSIGNED	= 1,
	EXIF_FORMAT_STRING		= 2,
	EXIF_FORMAT_SHORT_UNSIGNED	= 3,
	EXIF_FORMAT_LONG_UNSIGNED	= 4,
	EXIF_FORMAT_RATIONAL_UNSIGNED	= 5,
	EXIF_FORMAT_BYTE		= 6,
	EXIF_FORMAT_UNDEFINED		= 7,
	EXIF_FORMAT_SHORT		= 8,
	EXIF_FORMAT_LONG		= 9,
	EXIF_FORMAT_RATIONAL		= 10,
	EXIF_FORMAT_FLOAT		= 11,
	EXIF_FORMAT_DOUBLE		= 12
*/


guint exif_item_get_format_id(ExifItem *item)
{
	return ((Exiv2::Exifdatum *)item)->typeId();
}
const char *exif_item_get_format_name(ExifItem *item, gint brief)
{
/*
	return exif_item_get_tag_name(item);
*/
}


gchar *exif_item_get_data_as_text(ExifItem *item)
{
	return g_strdup(((Exiv2::Exifdatum *)item)->toString().c_str());
}


gint exif_item_get_integer(ExifItem *item, gint *value)
{
}

ExifRational *exif_item_get_rational(ExifItem *item, gint *sign)
{
}

const gchar *exif_get_description_by_key(const gchar *key)
{
}

gint format_raw_img_exif_offsets_fd(int fd, const gchar *path,
				    unsigned char *header_data, const guint header_len,
				    guint *image_offset, guint *exif_offset)
{
	return 0;
}

}

#endif 
/* HAVE_EXIV2 */
