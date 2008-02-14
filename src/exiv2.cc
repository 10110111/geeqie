
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

}

struct _ExifData
{
	Exiv2::ExifData exifData;
	Exiv2::ExifData::const_iterator iter;

	_ExifData(gchar *path, gint parse_color_profile)
	{
		Exiv2::Image::AutoPtr image = Exiv2::ImageFactory::open(path);
		g_assert (image.get() != 0);
		image->readMetadata();
		exifData = image->exifData();
	}

};

extern "C" {

ExifData *exif_read(gchar *path, gint parse_color_profile)
{
	printf("exif %s\n", path);
	try {
		return new ExifData(path, parse_color_profile);
	}
	catch (Exiv2::AnyError& e) {
		std::cout << "Caught Exiv2 exception '" << e << "'\n";
		return 0;
	}
	
}

void exif_free(ExifData *exif)
{
	
	delete exif;
}

ExifItem *exif_get_item(ExifData *exif, const gchar *key)
{
	try {
		Exiv2::ExifKey ekey(key);
		Exiv2::ExifData::iterator pos = exif->exifData.findKey(ekey);
		if (pos == exif->exifData.end()) return NULL;
		Exiv2::Exifdatum *item = &*pos;
		return (ExifItem *)item;
	}
	catch (Exiv2::AnyError& e) {
		std::cout << "Caught Exiv2 exception '" << e << "'\n";
		return NULL;
	}
}


ExifItem *exif_get_first_item(ExifData *exif)
{
	try {
		exif->iter = exif->exifData.begin();
		if (exif->iter == exif->exifData.end()) return NULL;
		const Exiv2::Exifdatum *item = &*exif->iter;
		return (ExifItem *)item;
	}
	catch (Exiv2::AnyError& e) {
		std::cout << "Caught Exiv2 exception '" << e << "'\n";
		return NULL;
	}
}

ExifItem *exif_get_next_item(ExifData *exif)
{
	try {
		exif->iter++;
		if (exif->iter == exif->exifData.end()) return NULL;
		const Exiv2::Exifdatum *item = &*exif->iter;
		return (ExifItem *)item;
	}
	catch (Exiv2::AnyError& e) {
		std::cout << "Caught Exiv2 exception '" << e << "'\n";
		return NULL;
	}
}

const char *exif_item_get_tag_name(ExifItem *item)
{
	try {
		if (!item) return NULL;
		return ((Exiv2::Exifdatum *)item)->key().c_str();
	}
	catch (Exiv2::AnyError& e) {
		std::cout << "Caught Exiv2 exception '" << e << "'\n";
		return NULL;
	}
}

guint exif_item_get_tag_id(ExifItem *item)
{
	try {
		if (!item) return 0;
		return ((Exiv2::Exifdatum *)item)->tag();
	}
	catch (Exiv2::AnyError& e) {
		std::cout << "Caught Exiv2 exception '" << e << "'\n";
		return 0;
	}
}

guint exif_item_get_elements(ExifItem *item)
{
	try {
		if (!item) return 0;
		return ((Exiv2::Exifdatum *)item)->count();
	}
	catch (Exiv2::AnyError& e) {
		std::cout << "Caught Exiv2 exception '" << e << "'\n";
		return NULL;
	}
}

char *exif_item_get_data(ExifItem *item, guint *data_len)
{
}

char *exif_item_get_description(ExifItem *item)
{
	try {
		if (!item) return NULL;
		return g_strdup(((Exiv2::Exifdatum *)item)->tagLabel().c_str());
	}
	catch (Exiv2::AnyError& e) {
		std::cout << "Caught Exiv2 exception '" << e << "'\n";
		return NULL;
	}
}

/*
invalidTypeId, unsignedByte, asciiString, unsignedShort,
  unsignedLong, unsignedRational, signedByte, undefined,
  signedShort, signedLong, signedRational, string,
  date, time, comment, directory,
  xmpText, xmpAlt, xmpBag, xmpSeq,
  langAlt, lastTypeId 
*/

static guint format_id_trans_tbl [] = {
	EXIF_FORMAT_UNKNOWN,
	EXIF_FORMAT_BYTE_UNSIGNED,
	EXIF_FORMAT_STRING,
	EXIF_FORMAT_SHORT_UNSIGNED,
	EXIF_FORMAT_LONG_UNSIGNED,
	EXIF_FORMAT_RATIONAL_UNSIGNED,
	EXIF_FORMAT_BYTE,
	EXIF_FORMAT_UNDEFINED,
	EXIF_FORMAT_SHORT,
	EXIF_FORMAT_LONG,
	EXIF_FORMAT_RATIONAL,
	EXIF_FORMAT_STRING,
	EXIF_FORMAT_STRING,
	EXIF_FORMAT_STRING,
	EXIF_FORMAT_UNDEFINED,
	EXIF_FORMAT_STRING,
	EXIF_FORMAT_STRING,
	EXIF_FORMAT_STRING,
	EXIF_FORMAT_STRING
	};
	
	

guint exif_item_get_format_id(ExifItem *item)
{
	try {
		if (!item) return EXIF_FORMAT_UNKNOWN;
		guint id = ((Exiv2::Exifdatum *)item)->typeId();
		if (id >= (sizeof(format_id_trans_tbl) / sizeof(format_id_trans_tbl[0])) ) return EXIF_FORMAT_UNKNOWN;
		return format_id_trans_tbl[id];
	}
	catch (Exiv2::AnyError& e) {
		std::cout << "Caught Exiv2 exception '" << e << "'\n";
		return EXIF_FORMAT_UNKNOWN;
	}
}

const char *exif_item_get_format_name(ExifItem *item, gint brief)
{
	try {
		if (!item) return NULL;
		return ((Exiv2::Exifdatum *)item)->typeName();
	}
	catch (Exiv2::AnyError& e) {
		std::cout << "Caught Exiv2 exception '" << e << "'\n";
		return NULL;
	}
}


gchar *exif_item_get_data_as_text(ExifItem *item)
{
	try {
		if (!item) return NULL;
		std::stringstream str;
		str << *((Exiv2::Exifdatum *)item);
		return g_strdup(str.str().c_str());
	}
	catch (Exiv2::AnyError& e) {
		return NULL;
	}
}


gint exif_item_get_integer(ExifItem *item, gint *value)
{
	try {
		if (!item) return 0;
		return ((Exiv2::Exifdatum *)item)->toLong();
	}
	catch (Exiv2::AnyError& e) {
		std::cout << "Caught Exiv2 exception '" << e << "'\n";
		return 0;
	}
}

ExifRational *exif_item_get_rational(ExifItem *item, gint *sign)
{
	try {
		if (!item) return NULL;
		Exiv2::Rational v = ((Exiv2::Exifdatum *)item)->toRational();
		static ExifRational ret;
		ret.num = v.first;
		ret.den = v.second;
		return &ret;
	}
	catch (Exiv2::AnyError& e) {
		std::cout << "Caught Exiv2 exception '" << e << "'\n";
		return NULL;
	}
}

const gchar *exif_get_tag_description_by_key(const gchar *key)
{
	try {
		Exiv2::ExifKey ekey(key);
		return Exiv2::ExifTags::tagLabel(ekey.tag(), ekey.ifdId ());
	}
	catch (Exiv2::AnyError& e) {
		std::cout << "Caught Exiv2 exception '" << e << "'\n";
		return NULL;
	}
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
