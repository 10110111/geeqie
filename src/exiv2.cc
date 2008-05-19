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



extern "C" {
#include <glib.h> 

#include "main.h"
#include "exif.h"

#include "filefilter.h"
#include "ui_fileops.h"
}

struct _ExifData
{
	Exiv2::Image::AutoPtr image;
	Exiv2::Image::AutoPtr sidecar;
	Exiv2::ExifData::const_iterator exifIter; /* for exif_get_next_item */
	Exiv2::IptcData::const_iterator iptcIter; /* for exif_get_next_item */
#if EXIV2_TEST_VERSION(0,16,0)
	Exiv2::XmpData::const_iterator xmpIter; /* for exif_get_next_item */
#endif
	bool have_sidecar;

	/* the icc profile in jpeg is not technically exif - store it here */
	unsigned char *cp_data;
	guint cp_length;

	_ExifData(gchar *path, gchar *sidecar_path)
	{
		have_sidecar = false;
		cp_data = NULL;
		cp_length = 0;
		gchar *pathl = path_from_utf8(path);
		image = Exiv2::ImageFactory::open(pathl);
		g_free(pathl);
//		g_assert (image.get() != 0);
		image->readMetadata();

#if EXIV2_TEST_VERSION(0,16,0)
		DEBUG_2("xmp count %li", image->xmpData().count());
		if (sidecar_path && image->xmpData().empty())
			{
			gchar *sidecar_pathl = path_from_utf8(sidecar_path);
			sidecar = Exiv2::ImageFactory::open(sidecar_pathl);
			g_free(sidecar_pathl);
			sidecar->readMetadata();
			have_sidecar = sidecar->good();
			DEBUG_2("sidecar xmp count %li", sidecar->xmpData().count());
			}

#endif
#if EXIV2_TEST_VERSION(0,14,0)
		if (image->mimeType() == std::string("image/jpeg"))
			{
			/* try to get jpeg color profile */
			Exiv2::BasicIo &io = image->io();
			gint open = io.isopen();
			if (!open) io.open();
			unsigned char *mapped = (unsigned char*)io.mmap();
			if (mapped) exif_jpeg_parse_color(this, mapped, io.size());
			io.munmap();
			if (!open) io.close();
			}
#endif		
	}
	
	~_ExifData()
	{
		if (cp_data) g_free(cp_data);
	}
	
	void writeMetadata()
	{
		if (have_sidecar) sidecar->writeMetadata();
		image->writeMetadata();
	}
	
	Exiv2::ExifData &exifData ()
	{
		return image->exifData();
	}

	Exiv2::IptcData &iptcData ()
	{
		return image->iptcData();
	}

#if EXIV2_TEST_VERSION(0,16,0)
	Exiv2::XmpData &xmpData ()
	{
		return have_sidecar ? sidecar->xmpData() : image->xmpData();
	}
#endif

};

extern "C" {

ExifData *exif_read(gchar *path, gchar *sidecar_path)
{
	DEBUG_1("exif read %s, sidecar: %s", path, sidecar_path ? sidecar_path : "-");
	try {
		return new ExifData(path, sidecar_path);
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
	
	delete exif;
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
		if(data_len) *data_len = md->size();
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
		return g_strdup(((Exiv2::Metadatum *)item)->tagLabel().c_str());
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
		return g_strdup(metadatum->print().c_str());
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

		return g_strdup(str.str().c_str());
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

ExifRational *exif_item_get_rational(ExifItem *item, gint *sign)
{
	try {
		if (!item) return NULL;
		Exiv2::Rational v = ((Exiv2::Metadatum *)item)->toRational();
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
	if (exif->cp_data) g_free(exif->cp_data);
	exif->cp_data = cp_data;
	exif->cp_length =cp_length;
}

unsigned char *exif_get_color_profile(ExifData *exif, guint *data_len)
{
	if (exif->cp_data)
		{
		if (data_len) *data_len = exif->cp_length;
		return (unsigned char *) g_memdup(exif->cp_data, exif->cp_length);
		}
	ExifItem *prof_item = exif_get_item(exif, "Exif.Image.InterColorProfile");
	if (prof_item && exif_item_get_format_id(prof_item) == EXIF_FORMAT_UNDEFINED)
		return (unsigned char *) exif_item_get_data(prof_item, data_len);
	return NULL;
}



}

/* This is a dirty hack to support raw file preview, bassed on 
tiffparse.cpp from Exiv2 examples */

class RawFile {
	public:
    
	RawFile(int fd);
	~RawFile();
    
	const Exiv2::Value *find(uint16_t tag, uint16_t group);
    
	unsigned long preview_offset();
    
	private:
	int type;
	Exiv2::TiffComponent::AutoPtr rootDir;
	Exiv2::byte *map_data;
	size_t map_len;
	unsigned long offset;
};

using namespace Exiv2;

RawFile::RawFile(int fd) : map_data(NULL), map_len(0), offset(0)
{
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
	if (map_data && munmap(map_data, map_len) == -1)
		{
		log_printf("Failed to unmap file \n");
		}
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


extern "C" gint format_raw_img_exif_offsets_fd(int fd, const gchar *path,
				    unsigned char *header_data, const guint header_len,
				    guint *image_offset, guint *exif_offset)
{
	int success;
	unsigned long offset;

	/* given image pathname, first do simple (and fast) file extension test */
	if (!filter_file_class(path, FORMAT_CLASS_RAWIMAGE)) return 0;

	try {
		RawFile rf(fd);
		offset = rf.preview_offset();
		DEBUG_1("%s: offset %lu", path, offset);
	}
	catch (Exiv2::AnyError& e) {
		std::cout << "Caught Exiv2 exception '" << e << "'\n";
		return 0;
	}

	if (image_offset && offset > 0)
		{
		*image_offset = offset;
		if ((unsigned long) lseek(fd, *image_offset, SEEK_SET) != *image_offset)
			{
			log_printf("Failed to seek to embedded image\n");

			*image_offset = 0;
			if (*exif_offset) *exif_offset = 0;
			success = FALSE;
			}
		}

	return offset > 0;
}


#endif 
/* HAVE_EXIV2 */
