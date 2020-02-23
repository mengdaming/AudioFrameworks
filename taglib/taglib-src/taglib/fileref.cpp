/***************************************************************************
 copyright            : (C) 2002 - 2008 by Scott Wheeler
 email                : wheeler@kde.org
 
 copyright            : (C) 2010 by Alex Novichkov
 email                : novichko@atnet.ru
 (added APE file support)
 ***************************************************************************/

/***************************************************************************
 *   This library is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU Lesser General Public License version   *
 *   2.1 as published by the Free Software Foundation.                     *
 *                                                                         *
 *   This library is distributed in the hope that it will be useful, but   *
 *   WITHOUT ANY WARRANTY; without even the implied warranty of            *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU     *
 *   Lesser General Public License for more details.                       *
 *                                                                         *
 *   You should have received a copy of the GNU Lesser General Public      *
 *   License along with this library; if not, write to the Free Software   *
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA         *
 *   02110-1301  USA                                                       *
 *                                                                         *
 *   Alternatively, this file is available under the Mozilla Public        *
 *   License Version 1.1.  You may obtain a copy of the License at         *
 *   http://www.mozilla.org/MPL/                                           *
 ***************************************************************************/

#include <tfile.h>
#include <tstring.h>
#include <tdebug.h>
#include <tsmartptr.h>

#include "fileref.h"
#include "asffile.h"
#include "mpegfile.h"
#include "vorbisfile.h"
#include "flacfile.h"
#include "oggflacfile.h"
#include "mpcfile.h"
#include "mp4file.h"
#include "wavpackfile.h"
#include "speexfile.h"
#include "opusfile.h"
#include "trueaudiofile.h"
#include "aifffile.h"
#include "wavfile.h"
#include "apefile.h"
#include "modfile.h"
#include "s3mfile.h"
#include "itfile.h"
#include "xmfile.h"
#include "dsffile.h"

using namespace TagLib;

namespace
    {
    typedef List<const FileRef::FileTypeResolver *> ResolverList;
    ResolverList fileTypeResolvers;
    
    // Templatized internal functions. T should be String or IOStream*.
    
    template <typename T>
    inline FileName toFileName(T arg)
    {
        debug("FileRef::toFileName<T>(): This version should never be called.");
        return FileName(L"");
    }
    
    template <>
    inline FileName toFileName<IOStream *>(IOStream *arg)
    {
        return arg->name();
    }
    
    template <>
    inline FileName toFileName<FileName>(FileName arg)
    {
        return arg;
    }
    
    template <typename T>
    inline File *resolveFileType(T arg, bool readProperties,
                                 AudioProperties::ReadStyle style)
    {
        debug("FileRef::resolveFileType<T>(): This version should never be called.");
        return 0;
    }
    
    template <>
    inline File *resolveFileType<IOStream *>(IOStream *arg, bool readProperties,
                                             AudioProperties::ReadStyle style)
    {
        return 0;
    }
    
    template <>
    inline File *resolveFileType<FileName>(FileName arg, bool readProperties,
                                           AudioProperties::ReadStyle style)
    {
        ResolverList::ConstIterator it = fileTypeResolvers.begin();
        for(; it != fileTypeResolvers.end(); ++it) {
            File *file = (*it)->createFile(arg, readProperties, style);
            if(file)
                return file;
        }
        
        return 0;
    }
    
    template <typename T>
    File* createInternal(T arg, bool readAudioProperties,
                         AudioProperties::ReadStyle audioPropertiesStyle)
    {
        File *file = resolveFileType(arg, readAudioProperties, audioPropertiesStyle);
        if(file)
            return file;
        
#ifdef _WIN32
        const String s = toFileName(arg).toString();
#else
        const String s(toFileName(arg));
#endif
        
        String ext;
        const size_t pos = s.rfind(".");
        if(pos != String::npos())
            ext = s.substr(pos + 1).upper();
        
        // If this list is updated, the method defaultFileExtensions() should also be
        // updated.  However at some point that list should be created at the same time
        // that a default file type resolver is created.
        
        if(ext.isEmpty())
            return 0;
        
        if(ext == "MP3")
            return new MPEG::File(arg, ID3v2::FrameFactory::instance(), readAudioProperties, audioPropertiesStyle);
        if(ext == "OGG")
            return new Ogg::Vorbis::File(arg, readAudioProperties, audioPropertiesStyle);
        if(ext == "OGA") {
            /* .oga can be any audio in the Ogg container. First try FLAC, then Vorbis. */
            File *ogaFile = new Ogg::FLAC::File(arg, readAudioProperties, audioPropertiesStyle);
            if(ogaFile->isValid())
                return ogaFile;
            delete ogaFile;
            return new Ogg::Vorbis::File(arg, readAudioProperties, audioPropertiesStyle);
        }
        if(ext == "FLAC")
            return new FLAC::File(arg, readAudioProperties, audioPropertiesStyle);
        if(ext == "MPC")
            return new MPC::File(arg, readAudioProperties, audioPropertiesStyle);
        if(ext == "WV")
            return new WavPack::File(arg, readAudioProperties, audioPropertiesStyle);
        if(ext == "SPX")
            return new Ogg::Speex::File(arg, readAudioProperties, audioPropertiesStyle);
        if(ext == "OPUS")
            return new Ogg::Opus::File(arg, readAudioProperties, audioPropertiesStyle);
        if(ext == "TTA")
            return new TrueAudio::File(arg, readAudioProperties, audioPropertiesStyle);
        if(ext == "M4A" || ext == "M4R" || ext == "M4B" || ext == "M4P" || ext == "MP4" || ext == "3G2" || ext == "M4V")
            return new MP4::File(arg, readAudioProperties, audioPropertiesStyle);
        if(ext == "WMA" || ext == "ASF")
            return new ASF::File(arg, readAudioProperties, audioPropertiesStyle);
        if(ext == "AIF" || ext == "AIFF" || ext == "AFC" || ext == "AIFC")
            return new RIFF::AIFF::File(arg, readAudioProperties, audioPropertiesStyle);
        if(ext == "WAV")
            return new RIFF::WAV::File(arg, readAudioProperties, audioPropertiesStyle);
        if(ext == "APE")
            return new APE::File(arg, readAudioProperties, audioPropertiesStyle);
        // module, nst and wow are possible but uncommon extensions
        if(ext == "MOD" || ext == "MODULE" || ext == "NST" || ext == "WOW")
            return new Mod::File(arg, readAudioProperties, audioPropertiesStyle);
        if(ext == "S3M")
            return new S3M::File(arg, readAudioProperties, audioPropertiesStyle);
        if(ext == "IT")
            return new IT::File(arg, readAudioProperties, audioPropertiesStyle);
        if(ext == "XM")
            return new XM::File(arg, readAudioProperties, audioPropertiesStyle);
        if (ext == "DSF")
            return new DSF::File(arg, readAudioProperties, audioPropertiesStyle);
        
        return 0;
    }
    }

class FileRef::FileRefPrivate
{
public:
    FileRefPrivate() :
    file() {}
    
    FileRefPrivate(File *f) :
    file(f) {}
    
    SHARED_PTR<File> file;
};

////////////////////////////////////////////////////////////////////////////////
// public members
////////////////////////////////////////////////////////////////////////////////

FileRef::FileRef() :
d(new FileRefPrivate())
{
}

FileRef::FileRef(FileName fileName, bool readAudioProperties,
                 AudioProperties::ReadStyle audioPropertiesStyle) :
d(new FileRefPrivate(createInternal(fileName, readAudioProperties, audioPropertiesStyle)))
{
}

FileRef::FileRef(IOStream* stream, bool readAudioProperties, AudioProperties::ReadStyle audioPropertiesStyle) :
d(new FileRefPrivate(createInternal(stream, readAudioProperties, audioPropertiesStyle)))
{
}

FileRef::FileRef(File *file) :
d(new FileRefPrivate(file))
{
}

FileRef::FileRef(const FileRef &ref) :
d(new FileRefPrivate(*ref.d))
{
}

FileRef::~FileRef()
{
    delete d;
}

Tag *FileRef::tag() const
{
    if(isNull()) {
        debug("FileRef::tag() - Called without a valid file.");
        return 0;
    }
    return d->file->tag();
}

PropertyMap FileRef::properties() const
{
    if(isNull()) {
        debug("FileRef::properties() - Called without a valid file.");
        return PropertyMap();
    }
    
    return d->file->properties();
}

void FileRef::removeUnsupportedProperties(const StringList& properties)
{
    if(isNull()) {
        debug("FileRef::removeUnsupportedProperties() - Called without a valid file.");
        return;
    }
    
    d->file->removeUnsupportedProperties(properties);
}

PropertyMap FileRef::setProperties(const PropertyMap &properties)
{
    if(isNull()) {
        debug("FileRef::setProperties() - Called without a valid file.");
        return PropertyMap();
    }
    
    return d->file->setProperties(properties);
}

AudioProperties *FileRef::audioProperties() const
{
    if(isNull()) {
        debug("FileRef::audioProperties() - Called without a valid file.");
        return 0;
    }
    return d->file->audioProperties();
}

File *FileRef::file() const
{
    return d->file.get();
}

bool FileRef::save()
{
    if(isNull()) {
        debug("FileRef::save() - Called without a valid file.");
        return false;
    }
    return d->file->save();
}

const FileRef::FileTypeResolver *FileRef::addFileTypeResolver(const FileRef::FileTypeResolver *resolver) // static
{
    fileTypeResolvers.prepend(resolver);
    return resolver;
}

StringList FileRef::defaultFileExtensions()
{
    StringList l;
    
    l.append("ogg");
    l.append("flac");
    l.append("oga");
    l.append("mp3");
    l.append("mpc");
    l.append("wv");
    l.append("spx");
    l.append("tta");
    l.append("m4a");
    l.append("m4r");
    l.append("m4b");
    l.append("m4p");
    l.append("3g2");
    l.append("mp4");
    l.append("m4v");
    l.append("wma");
    l.append("asf");
    l.append("aif");
    l.append("aiff");
    l.append("wav");
    l.append("ape");
    l.append("mod");
    l.append("module"); // alias for "mod"
    l.append("nst"); // alias for "mod"
    l.append("wow"); // alias for "mod"
    l.append("s3m");
    l.append("it");
    l.append("xm");
    l.append("dsf");
    
    return l;
}

bool FileRef::isValid() const
{
    return (d->file && d->file->isValid());
}

bool FileRef::isNull() const
{
    return !isValid();
}

FileRef &FileRef::operator=(const FileRef &ref)
{
    FileRef(ref).swap(*this);
    return *this;
}

void FileRef::swap(FileRef &ref)
{
    using std::swap;
    
    swap(d, ref.d);
}

bool FileRef::operator==(const FileRef &ref) const
{
    return (ref.d->file == d->file);
}

bool FileRef::operator!=(const FileRef &ref) const
{
    return (ref.d->file != d->file);
}