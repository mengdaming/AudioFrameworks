# Find system
UNAME	 = $(shell uname)

VERSION	  = 5

CXXOPTS	  = -I Shared -I Source/Shared -I Source/MACLib -c
LDOPTS	  = -lstdc++

DLLLDOPTS = -shared
CMDLDOPTS = 

ifneq ($(findstring MINGW,$(UNAME)),)
	SHAREDEXT  = dll

	DLLNAME	   = Source/MACDll.$(SHAREDEXT)
	LIBNAME	   = Source/libMAC.a
	CMDNAME	   = Source/MAC.exe

	CXXOPTS	  += -D PLATFORM_WINDOWS

	DLLLDOPTS += -Wl,--kill-at,--out-implib,$(LIBNAME)
	CMDLDOPTS += -municode -mconsole -Wl,--enable-stdcall-fixup
else
	SHAREDEXT  = so

	DLLNAME	   = libMAC.$(SHAREDEXT).$(VERSION)
	CMDNAME	   = mac

	CXXOPTS	  += -fPIC

#	Use the .dylib extension for shared objects on macOS.
	ifeq ($(UNAME),Darwin)
		SHAREDEXT = dylib

		DLLNAME	  = libMAC.$(VERSION).$(SHAREDEXT)
	endif

#	Set folders if not provided.
	ifndef prefix
		prefix = /usr/local
	endif

	ifndef bindir
		bindir = $(prefix)/bin
	endif

	ifndef libdir
		libdir = $(prefix)/lib
	endif

	ifndef includedir
		includedir = $(prefix)/include
	endif

#	Define PLATFORM_LINUX on all non-Apple platforms.
	ifeq ($(UNAME),Darwin)
		CXXOPTS += -D PLATFORM_APPLE
	else
		CXXOPTS += -D PLATFORM_LINUX
	endif
endif

SHAREDOBJECTS = Source/Shared/CharacterHelper.o Source/Shared/CircleBuffer.o Source/Shared/CRC.o Source/Shared/GlobalFunctions.o Source/Shared/StdLibFileIO.o Source/Shared/WinFileIO.o
LIBOBJECTS    = Source/MACLib/APECompress.o Source/MACLib/APECompressCore.o Source/MACLib/APECompressCreate.o Source/MACLib/APEDecompress.o Source/MACLib/APEHeader.o Source/MACLib/APEInfo.o Source/MACLib/APELink.o Source/MACLib/APESimple.o Source/MACLib/APETag.o Source/MACLib/BitArray.o Source/MACLib/MACLib.o Source/MACLib/MACProgressHelper.o Source/MACLib/MD5.o Source/MACLib/NewPredictor.o Source/MACLib/NNFilter.o Source/MACLib/Prepare.o Source/MACLib/UnBitArray.o Source/MACLib/UnBitArrayBase.o Source/MACLib/WAVInputSource.o Source/MACLib/Old/Anti-Predictor.o Source/MACLib/Old/AntiPredictorExtraHigh.o Source/MACLib/Old/AntiPredictorFast.o Source/MACLib/Old/AntiPredictorHigh.o Source/MACLib/Old/AntiPredictorNormal.o Source/MACLib/Old/APEDecompressCore.o Source/MACLib/Old/APEDecompressOld.o Source/MACLib/Old/UnBitArrayOld.o Source/MACLib/Old/UnMAC.o
DLLOBJECTS    = Source/MACDll/MACDll.o
CMDOBJECTS    = Source/Console/Console.o

all: $(DLLNAME) $(CMDNAME)

clean:
	rm -f $(DLLNAME) $(LIBNAME) $(SHAREDOBJECTS) $(LIBOBJECTS) $(DLLOBJECTS) $(CMDNAME) $(CMDOBJECTS)

install:
	mkdir -p $(DESTDIR)$(bindir)
	cp $(CMDNAME) $(DESTDIR)$(bindir)

	mkdir -p $(DESTDIR)$(libdir)
	cp $(DLLNAME) $(DESTDIR)$(libdir)
	ln -sf $(DLLNAME) $(DESTDIR)$(libdir)/libMAC.$(SHAREDEXT)

	mkdir -p $(DESTDIR)$(includedir)/MAC
	cp Shared/*.h $(DESTDIR)$(includedir)/MAC

$(DLLNAME): $(SHAREDOBJECTS) $(LIBOBJECTS) $(DLLOBJECTS)
	$(CC) $(SHAREDOBJECTS) $(LIBOBJECTS) $(DLLOBJECTS) $(LDOPTS) $(LDFLAGS) $(DLLLDOPTS) -o $@

$(CMDNAME): $(DLLNAME) $(CMDOBJECTS)
	$(CC) $(SHAREDOBJECTS) $(CMDOBJECTS) $(DLLNAME) $(LDOPTS) $(LDFLAGS) $(CMDLDOPTS) -o $@

Source/Shared/%.o: Source/Shared/%.cpp
	$(CXX) $(CXXOPTS) $(CXXFLAGS) $< -o $@

Source/MACLib/%.o: Source/MACLib/%.cpp
	$(CXX) $(CXXOPTS) $(CXXFLAGS) $< -o $@

Source/MACLib/Old/%.o: Source/MACLib/Old/%.cpp
	$(CXX) $(CXXOPTS) $(CXXFLAGS) $< -o $@

Source/MACDll/%.o: Source/MACDll/%.cpp
	$(CXX) $(CXXOPTS) $(CXXFLAGS) $< -o $@

Source/Console/%.o: Source/Console/%.cpp
	$(CXX) $(CXXOPTS) $(CXXFLAGS) $< -o $@
