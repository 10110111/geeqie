
#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#ifdef HAVE_EXIV2

#include <exiv2/image.hpp>
#include <exiv2/exif.hpp>
#include <iostream>

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
#include <exiv2/orfimage.hpp>
#include <exiv2/rafimage.hpp>
#include <exiv2/futils.hpp>



extern "C" {
#include <glib.h> 
#include "exif.h"

}

struct _ExifData
{
	Exiv2::Image::AutoPtr image;
	Exiv2::ExifData::const_iterator exifIter; /* for exif_get_next_item */
	Exiv2::IptcData::const_iterator iptcIter; /* for exif_get_next_item */
	Exiv2::XmpData::const_iterator xmpIter; /* for exif_get_next_item */

	_ExifData(gchar *path, gint parse_color_profile)
	{
		image = Exiv2::ImageFactory::open(path);
//		g_assert (image.get() != 0);
		image->readMetadata();
	}
	
	void writeMetadata()
	{
		image->writeMetadata();
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
		Exiv2::Metadatum *item;
		try {
			Exiv2::ExifKey ekey(key);
			Exiv2::ExifData::iterator pos = exif->image->exifData().findKey(ekey);
			if (pos == exif->image->exifData().end()) return NULL;
			item = &*pos;
		}
		catch (Exiv2::AnyError& e) {
			try {
				Exiv2::IptcKey ekey(key);
				Exiv2::IptcData::iterator pos = exif->image->iptcData().findKey(ekey);
				if (pos == exif->image->iptcData().end()) return NULL;
				item = &*pos;
			}
			catch (Exiv2::AnyError& e) {
				Exiv2::XmpKey ekey(key);
				Exiv2::XmpData::iterator pos = exif->image->xmpData().findKey(ekey);
				if (pos == exif->image->xmpData().end()) return NULL;
				item = &*pos;
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
		Exiv2::Metadatum *item;
		try {
			Exiv2::ExifKey ekey(key);
			exif->image->exifData().add(ekey, NULL);
			Exiv2::ExifData::iterator pos = exif->image->exifData().end(); // a hack, there should be a better way to get the currently added item
			pos--;
			item = &*pos;
		}
		catch (Exiv2::AnyError& e) {
			try {
				Exiv2::IptcKey ekey(key);
				exif->image->iptcData().add(ekey, NULL);
				Exiv2::IptcData::iterator pos = exif->image->iptcData().end();
				pos--;
				item = &*pos;
			}
			catch (Exiv2::AnyError& e) {
				Exiv2::XmpKey ekey(key);
				exif->image->xmpData().add(ekey, NULL);
				Exiv2::XmpData::iterator pos = exif->image->xmpData().end();
				pos--;
				item = &*pos;
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
		exif->exifIter = exif->image->exifData().begin();
		exif->iptcIter = exif->image->iptcData().begin();
		exif->xmpIter = exif->image->xmpData().begin();
		if (exif->exifIter != exif->image->exifData().end()) 
			{
			const Exiv2::Metadatum *item = &*exif->exifIter;
			exif->exifIter++;
			return (ExifItem *)item;
			}
		if (exif->iptcIter != exif->image->iptcData().end()) 
			{
			const Exiv2::Metadatum *item = &*exif->iptcIter;
			exif->iptcIter++;
			return (ExifItem *)item;
			}
		if (exif->xmpIter != exif->image->xmpData().end()) 
			{
			const Exiv2::Metadatum *item = &*exif->xmpIter;
			exif->xmpIter++;
			return (ExifItem *)item;
			}
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
		if (exif->exifIter != exif->image->exifData().end())
			{
			const Exiv2::Metadatum *item = &*exif->exifIter;
			exif->exifIter++;
			return (ExifItem *)item;
		}
		if (exif->iptcIter != exif->image->iptcData().end())
			{
			const Exiv2::Metadatum *item = &*exif->iptcIter;
			exif->iptcIter++;
			return (ExifItem *)item;
		}
		if (exif->xmpIter != exif->image->xmpData().end())
			{
			const Exiv2::Metadatum *item = &*exif->xmpIter;
			exif->xmpIter++;
			return (ExifItem *)item;
		}
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
//		std::stringstream str;  // does not work with Exiv2::Metadatum because operator<< is not virtual
//		str << *((Exiv2::Metadatum *)item);
//		return g_strdup(str.str().c_str());
		return g_strdup(((Exiv2::Metadatum *)item)->toString().c_str());
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
		for (Exiv2::ExifData::iterator i = exif->image->exifData().begin(); i != exif->image->exifData().end(); ++i) {
			if (((Exiv2::Metadatum *)item) == &*i) {
				i = exif->image->exifData().erase(i);
				return 1;
			}
		}
		for (Exiv2::IptcData::iterator i = exif->image->iptcData().begin(); i != exif->image->iptcData().end(); ++i) {
			if (((Exiv2::Metadatum *)item) == &*i) {
				i = exif->image->iptcData().erase(i);
				return 1;
			}
		}
		for (Exiv2::XmpData::iterator i = exif->image->xmpData().begin(); i != exif->image->xmpData().end(); ++i) {
			if (((Exiv2::Metadatum *)item) == &*i) {
				i = exif->image->xmpData().erase(i);
				return 1;
			}
		}
		
		return 0;
	}
	catch (Exiv2::AnyError& e) {
		return 0;
	}
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

	TiffHeaderBase *tiffHeader;
	switch (type) {
		case Exiv2::ImageType::tiff:
			tiffHeader = new TiffHeade2();
			break;
		case Exiv2::ImageType::cr2:
			tiffHeader = new Cr2Header();
			break;
		case Exiv2::ImageType::orf:
			tiffHeader = new OrfHeader();
			break;
		case Exiv2::ImageType::raf:
		        if (map_len < 84 + 4) throw Error(14);
    			offset = getULong(map_data + 84, bigEndian);
			return;

		default:
			throw Error(3, "RAW");
	}

	// process tiff-like formats
	if (!tiffHeader->read(map_data, map_len)) throw Error(3, "TIFF");

	TiffCompFactoryFct createFct = TiffCreator::create;

	rootDir = createFct(Tag::root, Group::none);
	if (0 == rootDir.get()) {
    		throw Error(1, "No root element defined in TIFF structure");
	}
	rootDir->setStart(map_data + tiffHeader->offset());

	TiffRwState::AutoPtr state(
    		new TiffRwState(tiffHeader->byteOrder(), 0, createFct));

	TiffReader reader(map_data,
                      map_len,
                      rootDir.get(),
                      state);

	rootDir->accept(reader);
	
	delete tiffHeader;
}

RawFile::~RawFile()
{
	if (map_data && munmap(map_data, map_len) == -1)
		{
		printf("Failed to unmap file \n");
		}
}

const Value * RawFile::find(uint16_t tag, uint16_t group)
{
	printf("%04x %04x\n", tag, group);
	TiffFinder finder(tag, group);
	rootDir->accept(finder);
	TiffEntryBase* te = dynamic_cast<TiffEntryBase*>(finder.result());
	if (te)
		return te->pValue();
	else
		return NULL;
}

unsigned long RawFile::preview_offset()
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


const static char *raw_ext_list[] = { ".cr2", ".nef", ".pef", ".arw", NULL };

extern "C" gint format_raw_img_exif_offsets_fd(int fd, const gchar *path,
				    unsigned char *header_data, const guint header_len,
				    guint *image_offset, guint *exif_offset)
{
	int success;
	unsigned long offset;

	/* given image pathname, first do simple (and fast) file extension test */
/*	if (path)
		{
		const gchar *ext;
		gint match = FALSE;
		gint i;

		ext = strrchr(path, '.');
		if (!ext) return FALSE;
		
		for (i = 0; raw_ext_list[i]; i++) 
			{
			if (strcasecmp(raw_ext_list[i], ext) == 0)
				{
				match = TRUE;
				break;
				}
			}

		if (!match) return FALSE;

		}
*/
	try {
		RawFile rf(fd);
		offset = rf.preview_offset();
	}
	catch (Exiv2::AnyError& e) {
		std::cout << "Caught Exiv2 exception '" << e << "'\n";
		return 0;
	}

	if (image_offset)
		{
		*image_offset = offset;
		if (lseek(fd, *image_offset, SEEK_SET) != *image_offset)
			{
			printf("Failed to seek to embedded image\n");

			*image_offset = 0;
			if (*exif_offset) *exif_offset = 0;
			success = FALSE;
			}
		}

	return offset > 0;
}


#endif 
/* HAVE_EXIV2 */
