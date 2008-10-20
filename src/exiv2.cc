/*
 * Geeqie
 * Copyright (C) 2008 The Geeqie Team
 *
 * Author: Vladimir Nadvornik
 *
 * This software is released under the GNU General Public License (GNU GPL).
 * Please read the included file COPYING for more information.
 * This software comes with no warranty of any kind, use at your own risk!
 */

#include "config.h"

#ifdef HAVE_EXIV2

#include <exiv2/image.hpp>
#include <exiv2/exif.hpp>
#include <iostream>

// EXIV2_TEST_VERSION is defined in Exiv2 0.15 and newer.
#ifndef EXIV2_TEST_VERSION
# define EXIV2_TEST_VERSION(major,minor,patch) \
	( EXIV2_VERSION >= EXIV2_MAKE_VERSION(major,minor,patch) )
#endif


#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

#if !EXIV2_TEST_VERSION(0,17,90)
#include <exiv2/tiffparser.hpp>
#include <exiv2/tiffcomposite.hpp>
#include <exiv2/tiffvisitor.hpp>
#include <exiv2/tiffimage.hpp>
#include <exiv2/cr2image.hpp>
#include <exiv2/crwimage.hpp>
#if EXIV2_TEST_VERSION(0,16,0)
#include <exiv2/orfimage.hpp>
#endif
#if EXIV2_TEST_VERSION(0,13,0)
#include <exiv2/rafimage.hpp>
#endif
#include <exiv2/futils.hpp>
#else
#include <exiv2/preview.hpp>
#endif

#if EXIV2_TEST_VERSION(0,17,0)
#include <exiv2/convert.hpp>
#include <exiv2/xmpsidecar.hpp>
#endif


extern "C" {
#include <glib.h>

#include "main.h"
#include "exif.h"

#include "filefilter.h"
#include "ui_fileops.h"
}

struct _ExifData
{
	Exiv2::ExifData::const_iterator exifIter; /* for exif_get_next_item */
	Exiv2::IptcData::const_iterator iptcIter; /* for exif_get_next_item */
#if EXIV2_TEST_VERSION(0,16,0)
	Exiv2::XmpData::const_iterator xmpIter; /* for exif_get_next_item */
#endif

	virtual ~_ExifData()
	{
	}
	
	virtual void writeMetadata()
	{
		g_critical("Unsupported method of writing metadata");
	}

	virtual ExifData *original()
	{
		return NULL;
	}

	virtual Exiv2::Image *image() = 0;
	
	virtual Exiv2::ExifData &exifData() = 0;

	virtual Exiv2::IptcData &iptcData() = 0;

#if EXIV2_TEST_VERSION(0,16,0)
	virtual Exiv2::XmpData &xmpData() = 0;
#endif

	virtual void add_jpeg_color_profile(unsigned char *cp_data, guint cp_length) = 0;

	virtual guchar *get_jpeg_color_profile(guint *data_len) = 0;
};

// This allows read-only access to the original metadata
struct _ExifDataOriginal : public _ExifData
{
protected:
	Exiv2::Image::AutoPtr image_;

	/* the icc profile in jpeg is not technically exif - store it here */
	unsigned char *cp_data_;
	guint cp_length_;

public:
	_ExifDataOriginal(Exiv2::Image::AutoPtr image)
	{
		cp_data_ = NULL;
		cp_length_ = 0;
		image_ = image;
	}

	_ExifDataOriginal(gchar *path)
	{
		cp_data_ = NULL;
		cp_length_ = 0;
		gchar *pathl = path_from_utf8(path);
		image_ = Exiv2::ImageFactory::open(pathl);
		g_free(pathl);
//		g_assert (image.get() != 0);
		image_->readMetadata();

#if EXIV2_TEST_VERSION(0,16,0)
		if (image_->mimeType() == "application/rdf+xml")
			{
			//Exiv2 sidecar converts xmp to exif and iptc, we don't want it.
			image_->clearExifData();
			image_->clearIptcData();
			}
#endif

#if EXIV2_TEST_VERSION(0,14,0)
		if (image_->mimeType() == "image/jpeg")
			{
			/* try to get jpeg color profile */
			Exiv2::BasicIo &io = image_->io();
			gint open = io.isopen();
			if (!open) io.open();
			unsigned char *mapped = (unsigned char*)io.mmap();
			if (mapped) exif_jpeg_parse_color(this, mapped, io.size());
			io.munmap();
			if (!open) io.close();
			}
#endif
	}
	
	virtual ~_ExifDataOriginal()
	{
		if (cp_data_) g_free(cp_data_);
	}
	
	virtual Exiv2::Image *image()
	{
		return image_.get();
	}
	
	virtual Exiv2::ExifData &exifData ()
	{
		return image_->exifData();
	}

	virtual Exiv2::IptcData &iptcData ()
	{
		return image_->iptcData();
	}

#if EXIV2_TEST_VERSION(0,16,0)
	virtual Exiv2::XmpData &xmpData ()
	{
		return image_->xmpData();
	}
#endif

	virtual void add_jpeg_color_profile(unsigned char *cp_data, guint cp_length)
	{
		if (cp_data_) g_free(cp_data_);
		cp_data_ = cp_data;
		cp_length_ = cp_length;
	}

	virtual guchar *get_jpeg_color_profile(guint *data_len)
	{
		if (cp_data_)
		{
			if (data_len) *data_len = cp_length_;
			return (unsigned char *) g_memdup(cp_data_, cp_length_);
		}
		return NULL;
	}
};

// This allows read-write access to the metadata
struct _ExifDataProcessed : public _ExifData
{
protected:
	_ExifDataOriginal *imageData_;
	_ExifDataOriginal *sidecarData_;

	Exiv2::ExifData exifData_;
	Exiv2::IptcData iptcData_;
#if EXIV2_TEST_VERSION(0,16,0)
	Exiv2::XmpData xmpData_;
#endif

public:
	_ExifDataProcessed(gchar *path, gchar *sidecar_path)
	{
		imageData_ = new _ExifDataOriginal(path);
		sidecarData_ = NULL;
#if EXIV2_TEST_VERSION(0,16,0)
		xmpData_ = imageData_->xmpData();
		DEBUG_2("xmp count %li", xmpData_.count());
		if (sidecar_path && xmpData_.empty())
			{
			sidecarData_ = new _ExifDataOriginal(sidecar_path);
			xmpData_ = sidecarData_->xmpData();
			}
#endif
		exifData_ = imageData_->exifData();
		iptcData_ = imageData_->iptcData();
#if EXIV2_TEST_VERSION(0,17,0)
		syncExifWithXmp(exifData_, xmpData_);
#endif
	}

	virtual ~_ExifDataProcessed()
	{
		if (imageData_) delete imageData_;
		if (sidecarData_) delete sidecarData_;
	}

	virtual ExifData *original()
	{
		return imageData_;
	}

	virtual void writeMetadata()
	{
#if EXIV2_TEST_VERSION(0,17,0)
		syncExifWithXmp(exifData_, xmpData_);
		copyXmpToIptc(xmpData_, iptcData_); //FIXME it should be configurable
#endif
		if (sidecarData_) 
			{
			sidecarData_->image()->setXmpData(xmpData_);
			//Exiv2 sidecar converts xmp to exif and iptc, we don't want it.
			sidecarData_->image()->clearExifData();
			sidecarData_->image()->clearIptcData();
			sidecarData_->image()->writeMetadata();
			}
		else
			{
			try 	{
				imageData_->image()->setExifData(exifData_);
				imageData_->image()->setIptcData(iptcData_);
				imageData_->image()->setXmpData(xmpData_);
				imageData_->image()->writeMetadata();
				} 
			catch (Exiv2::AnyError& e) 
				{
				// can't write into the image, create sidecar
#if EXIV2_TEST_VERSION(0,17,0)
				gchar *base = remove_extension_from_path(imageData_->image()->io().path().c_str());
				gchar *pathl = g_strconcat(base, ".xmp", NULL);

				Exiv2::Image::AutoPtr sidecar = Exiv2::ImageFactory::create(Exiv2::ImageType::xmp, pathl);
				
				g_free(base);
				g_free(pathl);
				
				sidecarData_ = new _ExifDataOriginal(sidecar);
				sidecarData_->image()->setXmpData(xmpData_);
				sidecarData_->image()->writeMetadata();
#endif
				}
			}
	}
	
	virtual Exiv2::Image *image()
	{
		return imageData_->image();
	}
	
	virtual Exiv2::ExifData &exifData ()
	{
		return exifData_;
	}

	virtual Exiv2::IptcData &iptcData ()
	{
		return iptcData_;
	}

#if EXIV2_TEST_VERSION(0,16,0)
	virtual Exiv2::XmpData &xmpData ()
	{
		return xmpData_;
	}
#endif

	virtual void add_jpeg_color_profile(unsigned char *cp_data, guint cp_length)
	{
		imageData_->add_jpeg_color_profile(cp_data, cp_length);
	}

	virtual guchar *get_jpeg_color_profile(guint *data_len)
	{
		return imageData_->get_jpeg_color_profile(data_len);
	}
};




extern "C" {

ExifData *exif_read(gchar *path, gchar *sidecar_path)
{
	DEBUG_1("exif read %s, sidecar: %s", path, sidecar_path ? sidecar_path : "-");
	try {
		return new _ExifDataProcessed(path, sidecar_path);
	}
	catch (Exiv2::AnyError& e) {
		std::cout << "Caught Exiv2 exception '" << e << "'\n";
		return NULL;
	}
	
}

int exif_write(ExifData *exif)
{
	try {
		exif->writeMetadata();
		return 1;
	}
	catch (Exiv2::AnyError& e) {
		std::cout << "Caught Exiv2 exception '" << e << "'\n";
		return 0;
	}
	
}


void exif_free(ExifData *exif)
{
	if (!exif) return;
	g_assert(dynamic_cast<_ExifDataProcessed *>(exif)); // this should not be called on ExifDataOriginal
	delete exif;
}

ExifData *exif_get_original(ExifData *exif)
{
	return exif->original();
}


ExifItem *exif_get_item(ExifData *exif, const gchar *key)
{
	try {
		Exiv2::Metadatum *item = NULL;
		try {
			Exiv2::ExifKey ekey(key);
			Exiv2::ExifData::iterator pos = exif->exifData().findKey(ekey);
			if (pos == exif->exifData().end()) return NULL;
			item = &*pos;
		}
		catch (Exiv2::AnyError& e) {
			try {
				Exiv2::IptcKey ekey(key);
				Exiv2::IptcData::iterator pos = exif->iptcData().findKey(ekey);
				if (pos == exif->iptcData().end()) return NULL;
				item = &*pos;
			}
			catch (Exiv2::AnyError& e) {
#if EXIV2_TEST_VERSION(0,16,0)
				Exiv2::XmpKey ekey(key);
				Exiv2::XmpData::iterator pos = exif->xmpData().findKey(ekey);
				if (pos == exif->xmpData().end()) return NULL;
				item = &*pos;
#endif
			}
		}
		return (ExifItem *)item;
	}
	catch (Exiv2::AnyError& e) {
		std::cout << "Caught Exiv2 exception '" << e << "'\n";
		return NULL;
	}
}

ExifItem *exif_add_item(ExifData *exif, const gchar *key)
{
	try {
		Exiv2::Metadatum *item = NULL;
		try {
			Exiv2::ExifKey ekey(key);
			exif->exifData().add(ekey, NULL);
			Exiv2::ExifData::iterator pos = exif->exifData().end(); // a hack, there should be a better way to get the currently added item
			pos--;
			item = &*pos;
		}
		catch (Exiv2::AnyError& e) {
			try {
				Exiv2::IptcKey ekey(key);
				exif->iptcData().add(ekey, NULL);
				Exiv2::IptcData::iterator pos = exif->iptcData().end();
				pos--;
				item = &*pos;
			}
			catch (Exiv2::AnyError& e) {
#if EXIV2_TEST_VERSION(0,16,0)
				Exiv2::XmpKey ekey(key);
				exif->xmpData().add(ekey, NULL);
				Exiv2::XmpData::iterator pos = exif->xmpData().end();
				pos--;
				item = &*pos;
#endif
			}
		}
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
		exif->exifIter = exif->exifData().begin();
		exif->iptcIter = exif->iptcData().begin();
#if EXIV2_TEST_VERSION(0,16,0)
		exif->xmpIter = exif->xmpData().begin();
#endif
		if (exif->exifIter != exif->exifData().end())
			{
			const Exiv2::Metadatum *item = &*exif->exifIter;
			exif->exifIter++;
			return (ExifItem *)item;
			}
		if (exif->iptcIter != exif->iptcData().end())
			{
			const Exiv2::Metadatum *item = &*exif->iptcIter;
			exif->iptcIter++;
			return (ExifItem *)item;
			}
#if EXIV2_TEST_VERSION(0,16,0)
		if (exif->xmpIter != exif->xmpData().end())
			{
			const Exiv2::Metadatum *item = &*exif->xmpIter;
			exif->xmpIter++;
			return (ExifItem *)item;
			}
#endif
		return NULL;
			
	}
	catch (Exiv2::AnyError& e) {
		std::cout << "Caught Exiv2 exception '" << e << "'\n";
		return NULL;
	}
}

ExifItem *exif_get_next_item(ExifData *exif)
{
	try {
		if (exif->exifIter != exif->exifData().end())
			{
			const Exiv2::Metadatum *item = &*exif->exifIter;
			exif->exifIter++;
			return (ExifItem *)item;
		}
		if (exif->iptcIter != exif->iptcData().end())
			{
			const Exiv2::Metadatum *item = &*exif->iptcIter;
			exif->iptcIter++;
			return (ExifItem *)item;
		}
#if EXIV2_TEST_VERSION(0,16,0)
		if (exif->xmpIter != exif->xmpData().end())
			{
			const Exiv2::Metadatum *item = &*exif->xmpIter;
			exif->xmpIter++;
			return (ExifItem *)item;
		}
#endif
		return NULL;
	}
	catch (Exiv2::AnyError& e) {
		std::cout << "Caught Exiv2 exception '" << e << "'\n";
		return NULL;
	}
}

char *exif_item_get_tag_name(ExifItem *item)
{
	try {
		if (!item) return NULL;
		return g_strdup(((Exiv2::Metadatum *)item)->key().c_str());
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
		return ((Exiv2::Metadatum *)item)->tag();
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
		return ((Exiv2::Metadatum *)item)->count();
	}
	catch (Exiv2::AnyError& e) {
		std::cout << "Caught Exiv2 exception '" << e << "'\n";
		return 0;
	}
}

char *exif_item_get_data(ExifItem *item, guint *data_len)
{
	try {
		if (!item) return 0;
		Exiv2::Metadatum *md = (Exiv2::Metadatum *)item;
		if (data_len) *data_len = md->size();
		char *data = (char *)g_malloc(md->size());
		long res = md->copy((Exiv2::byte *)data, Exiv2::littleEndian /* should not matter */);
		g_assert(res == md->size());
		return data;
	}
	catch (Exiv2::AnyError& e) {
		std::cout << "Caught Exiv2 exception '" << e << "'\n";
		return NULL;
	}
}

char *exif_item_get_description(ExifItem *item)
{
	try {
		if (!item) return NULL;
		return g_locale_to_utf8(((Exiv2::Metadatum *)item)->tagLabel().c_str(), -1, NULL, NULL, NULL);
	}
	catch (std::exception& e) {
//		std::cout << "Caught Exiv2 exception '" << e << "'\n";
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
		guint id = ((Exiv2::Metadatum *)item)->typeId();
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
		return ((Exiv2::Metadatum *)item)->typeName();
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
		Exiv2::Metadatum *metadatum = (Exiv2::Metadatum *)item;
#if EXIV2_TEST_VERSION(0,17,0)
		return g_locale_to_utf8(metadatum->print().c_str(), -1, NULL, NULL, NULL);
#else
		std::stringstream str;
		Exiv2::Exifdatum *exifdatum;
		Exiv2::Iptcdatum *iptcdatum;
#if EXIV2_TEST_VERSION(0,16,0)
		Exiv2::Xmpdatum *xmpdatum;
#endif
		if ((exifdatum = dynamic_cast<Exiv2::Exifdatum *>(metadatum)))
			str << *exifdatum;
		else if ((iptcdatum = dynamic_cast<Exiv2::Iptcdatum *>(metadatum)))
			str << *iptcdatum;
#if EXIV2_TEST_VERSION(0,16,0)
		else if ((xmpdatum = dynamic_cast<Exiv2::Xmpdatum *>(metadatum)))
			str << *xmpdatum;
#endif

		return g_locale_to_utf8(str.str().c_str(), -1, NULL, NULL, NULL);
#endif
	}
	catch (Exiv2::AnyError& e) {
		return NULL;
	}
}

gchar *exif_item_get_string(ExifItem *item, int idx)
{
	try {
		if (!item) return NULL;
		Exiv2::Metadatum *em = (Exiv2::Metadatum *)item;
#if EXIV2_TEST_VERSION(0,16,0)
		std::string str = em->toString(idx);
#else
		std::string str = em->toString(); // FIXME
#endif
		if (idx == 0 && str == "") str = em->toString();
		if (str.length() > 5 && str.substr(0, 5) == "lang=")
			{
			std::string::size_type pos = str.find_first_of(' ');
			if (pos != std::string::npos) str = str.substr(pos+1);
			}

//		return g_locale_to_utf8(str.c_str(), -1, NULL, NULL, NULL); // FIXME
		return g_strdup(str.c_str());
	}
	catch (Exiv2::AnyError& e) {
		return NULL;
	}
}


gint exif_item_get_integer(ExifItem *item, gint *value)
{
	try {
		if (!item) return 0;
		*value = ((Exiv2::Metadatum *)item)->toLong();
		return 1;
	}
	catch (Exiv2::AnyError& e) {
		std::cout << "Caught Exiv2 exception '" << e << "'\n";
		return 0;
	}
}

ExifRational *exif_item_get_rational(ExifItem *item, gint *sign, guint n)
{
	try {
		if (!item) return NULL;
		if (n >= exif_item_get_elements(item)) return NULL;
		Exiv2::Rational v = ((Exiv2::Metadatum *)item)->toRational(n);
		static ExifRational ret;
		ret.num = v.first;
		ret.den = v.second;
		if (sign) *sign = (((Exiv2::Metadatum *)item)->typeId() == Exiv2::signedRational);
		return &ret;
	}
	catch (Exiv2::AnyError& e) {
		std::cout << "Caught Exiv2 exception '" << e << "'\n";
		return NULL;
	}
}

gchar *exif_get_tag_description_by_key(const gchar *key)
{
	try {
		Exiv2::ExifKey ekey(key);
		return g_locale_to_utf8(Exiv2::ExifTags::tagLabel(ekey.tag(), ekey.ifdId ()), -1, NULL, NULL, NULL);
	}
	catch (Exiv2::AnyError& e) {
		std::cout << "Caught Exiv2 exception '" << e << "'\n";
		return NULL;
	}
}

int exif_item_set_string(ExifItem *item, const char *str)
{
	try {
		if (!item) return 0;
		((Exiv2::Metadatum *)item)->setValue(std::string(str));
		return 1;
		}
	catch (Exiv2::AnyError& e) {
		return 0;
	}
}

int exif_item_delete(ExifData *exif, ExifItem *item)
{
	try {
		if (!item) return 0;
		for (Exiv2::ExifData::iterator i = exif->exifData().begin(); i != exif->exifData().end(); ++i) {
			if (((Exiv2::Metadatum *)item) == &*i) {
				i = exif->exifData().erase(i);
				return 1;
			}
		}
		for (Exiv2::IptcData::iterator i = exif->iptcData().begin(); i != exif->iptcData().end(); ++i) {
			if (((Exiv2::Metadatum *)item) == &*i) {
				i = exif->iptcData().erase(i);
				return 1;
			}
		}
#if EXIV2_TEST_VERSION(0,16,0)
		for (Exiv2::XmpData::iterator i = exif->xmpData().begin(); i != exif->xmpData().end(); ++i) {
			if (((Exiv2::Metadatum *)item) == &*i) {
				i = exif->xmpData().erase(i);
				return 1;
			}
		}
#endif
		return 0;
	}
	catch (Exiv2::AnyError& e) {
		return 0;
	}
}

void exif_add_jpeg_color_profile(ExifData *exif, unsigned char *cp_data, guint cp_length)
{
	exif->add_jpeg_color_profile(cp_data, cp_length);
}

guchar *exif_get_color_profile(ExifData *exif, guint *data_len)
{
	guchar *ret = exif->get_jpeg_color_profile(data_len);
	if (ret) return ret;

	ExifItem *prof_item = exif_get_item(exif, "Exif.Image.InterColorProfile");
	if (prof_item && exif_item_get_format_id(prof_item) == EXIF_FORMAT_UNDEFINED)
		ret = (guchar *)exif_item_get_data(prof_item, data_len);
	return ret;
}

#if EXIV2_TEST_VERSION(0,17,90)

guchar *exif_get_preview(ExifData *exif, guint *data_len, gint requested_width, gint requested_height)
{
	if (!exif) return NULL;

	const char* path = exif->image()->io().path().c_str();
	/* given image pathname, first do simple (and fast) file extension test */
	gboolean is_raw = filter_file_class(path, FORMAT_CLASS_RAWIMAGE);
	
	if (!is_raw && requested_width == 0) return NULL;

	try {

		Exiv2::PreviewManager pm(*exif->image());

		Exiv2::PreviewPropertiesList list = pm.getPreviewProperties();

		if (!list.empty())
			{
			Exiv2::PreviewPropertiesList::iterator pos;
			Exiv2::PreviewPropertiesList::iterator last = --list.end();
			
			if (requested_width == 0)
				{
				pos = last; // the largest
				}
			else
				{
				pos = list.begin();
				while (pos != last)
					{
					if (pos->width_ >= (uint32_t)requested_width &&
					    pos->height_ >= (uint32_t)requested_height) break;
					++pos;
					}
				
				// we are not interested in smaller thumbnails in normal image formats - we can use full image instead
				if (!is_raw) 
					{
					if (pos->width_ < (uint32_t)requested_width || pos->height_ < (uint32_t)requested_height) return NULL; 
					}
				}

			Exiv2::PreviewImage image = pm.getPreviewImage(*pos);

			Exiv2::DataBuf buf = image.copy();
			std::pair<Exiv2::byte*, long> p = buf.release();

			*data_len = p.second;
			return p.first;
			}
		return NULL;
	}
	catch (Exiv2::AnyError& e) {
		std::cout << "Caught Exiv2 exception '" << e << "'\n";
		return NULL;
	}
}

void exif_free_preview(guchar *buf)
{
	delete[] (Exiv2::byte*)buf;
}
#endif

}
#if !EXIV2_TEST_VERSION(0,17,90)

/* This is a dirty hack to support raw file preview, bassed on
tiffparse.cpp from Exiv2 examples */

class RawFile {
	public:

	RawFile(Exiv2::BasicIo &io);
	~RawFile();

	const Exiv2::Value *find(uint16_t tag, uint16_t group);

	unsigned long preview_offset();

	private:
	int type;
	Exiv2::TiffComponent::AutoPtr rootDir;
	Exiv2::BasicIo &io_;
	const Exiv2::byte *map_data;
	size_t map_len;
	unsigned long offset;
};

typedef struct _UnmapData UnmapData;
struct _UnmapData
{
	guchar *ptr;
	guchar *map_data;
	size_t map_len;
};

static GList *exif_unmap_list = 0;

extern "C" guchar *exif_get_preview(ExifData *exif, guint *data_len, gint requested_width, gint requested_height)
{
	unsigned long offset;

	if (!exif) return NULL;
	const char* path = exif->image()->io().path().c_str();

	/* given image pathname, first do simple (and fast) file extension test */
	if (!filter_file_class(path, FORMAT_CLASS_RAWIMAGE)) return NULL;

	try {
		struct stat st;
		guchar *map_data;
		size_t map_len;
		UnmapData *ud;
		int fd;
		
		RawFile rf(exif->image()->io());
		offset = rf.preview_offset();
		DEBUG_1("%s: offset %lu", path, offset);
		
		fd = open(path, O_RDONLY);
		if (fd == -1)
			{
			return NULL;
			}

		if (fstat(fd, &st) == -1)
			{
			close(fd);
			return NULL;
			}
		map_len = st.st_size;
		map_data = (guchar *) mmap(0, map_len, PROT_READ, MAP_PRIVATE, fd, 0);
		close(fd);
		if (map_data == MAP_FAILED)
			{
			return NULL;
			}
		*data_len = map_len - offset;
		ud = g_new(UnmapData, 1);
		ud->ptr = map_data + offset;
		ud->map_data = map_data;
		ud->map_len = map_len;
		
		exif_unmap_list = g_list_prepend(exif_unmap_list, ud);
		return ud->ptr;
		
	}
	catch (Exiv2::AnyError& e) {
		std::cout << "Caught Exiv2 exception '" << e << "'\n";
	}
	return NULL;

}

void exif_free_preview(guchar *buf)
{
	GList *work = exif_unmap_list;
	
	while (work)
		{
		UnmapData *ud = (UnmapData *)work->data;
		if (ud->ptr == buf)
			{
			munmap(ud->map_data, ud->map_len);
			exif_unmap_list = g_list_remove_link(exif_unmap_list, work);
			g_free(ud);
			return;
			}
		work = work->next;
		}
	g_assert_not_reached();
}

using namespace Exiv2;

RawFile::RawFile(BasicIo &io) : io_(io), map_data(NULL), map_len(0), offset(0)
{
/*
	struct stat st;
	if (fstat(fd, &st) == -1)
		{
		throw Error(14);
		}
	map_len = st.st_size;
	map_data = (Exiv2::byte *) mmap(0, map_len, PROT_READ, MAP_PRIVATE, fd, 0);
	if (map_data == MAP_FAILED)
		{
		throw Error(14);
		}
*/
        if (io.open() != 0) {
            throw Error(9, io.path(), strError());
        }
        
        map_data = io.mmap();
        map_len = io.size();


	type = Exiv2::ImageFactory::getType(map_data, map_len);

#if EXIV2_TEST_VERSION(0,16,0)
	TiffHeaderBase *tiffHeader = NULL;
#else
	TiffHeade2 *tiffHeader = NULL;
#endif
	Cr2Header *cr2Header = NULL;

	switch (type) {
		case Exiv2::ImageType::tiff:
			tiffHeader = new TiffHeade2();
			break;
		case Exiv2::ImageType::cr2:
			cr2Header = new Cr2Header();
			break;
#if EXIV2_TEST_VERSION(0,16,0)
		case Exiv2::ImageType::orf:
			tiffHeader = new OrfHeader();
			break;
#endif
#if EXIV2_TEST_VERSION(0,13,0)
		case Exiv2::ImageType::raf:
			if (map_len < 84 + 4) throw Error(14);
			offset = getULong(map_data + 84, bigEndian);
			return;
#endif
		case Exiv2::ImageType::crw:
			{
			// Parse the image, starting with a CIFF header component
			Exiv2::CiffHeader::AutoPtr parseTree(new Exiv2::CiffHeader);
			parseTree->read(map_data, map_len);
			CiffComponent *entry = parseTree->findComponent(0x2007, 0);
			if (entry) offset =  entry->pData() - map_data;
			return;
			}

		default:
			throw Error(3, "RAW");
	}

	// process tiff-like formats

	TiffCompFactoryFct createFct = TiffCreator::create;

	rootDir = createFct(Tag::root, Group::none);
	if (0 == rootDir.get()) {
		throw Error(1, "No root element defined in TIFF structure");
	}
	
	if (tiffHeader)
		{
		if (!tiffHeader->read(map_data, map_len)) throw Error(3, "TIFF");
#if EXIV2_TEST_VERSION(0,16,0)
		rootDir->setStart(map_data + tiffHeader->offset());
#else
		rootDir->setStart(map_data + tiffHeader->ifdOffset());
#endif
		}
		
	if (cr2Header)
		{
		rootDir->setStart(map_data + cr2Header->offset());
		}
	
	TiffRwState::AutoPtr state(new TiffRwState(tiffHeader ? tiffHeader->byteOrder() : littleEndian, 0, createFct));

	TiffReader reader(map_data,
			  map_len,
			  rootDir.get(),
			  state);

	rootDir->accept(reader);
	
	if (tiffHeader)
		delete tiffHeader;
	if (cr2Header)
		delete cr2Header;
}

RawFile::~RawFile(void)
{
	io_.munmap();
	io_.close();
}

const Value * RawFile::find(uint16_t tag, uint16_t group)
{
	TiffFinder finder(tag, group);
	rootDir->accept(finder);
	TiffEntryBase* te = dynamic_cast<TiffEntryBase*>(finder.result());
	if (te)
		{
		DEBUG_1("(tag: %04x %04x) ", tag, group);
		return te->pValue();
		}
	else
		return NULL;
}

unsigned long RawFile::preview_offset(void)
{
	const Value *val;
	if (offset) return offset;
	
	if (type == Exiv2::ImageType::cr2)
		{
		val = find(0x111, Group::ifd0);
		if (val) return val->toLong();

		return 0;
		}
	
	val = find(0x201, Group::sub0_0);
	if (val) return val->toLong();

	val = find(0x201, Group::ifd0);
	if (val) return val->toLong();

	val = find(0x201, Group::ignr); // for PEF files, originally it was probably ifd2
	if (val) return val->toLong();

	val = find(0x111, Group::sub0_1); // dng
	if (val) return val->toLong();

	return 0;
}


#endif


#endif
/* HAVE_EXIV2 */
/* vim: set shiftwidth=8 softtabstop=0 cindent cinoptions={1s: */
