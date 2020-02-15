/***************************************************************************************
MAC Console Frontend (MAC.exe)

Pretty simple and straightforward console front end.  If somebody ever wants to add 
more functionality like tagging, auto-verify, etc., that'd be excellent.

Copyrighted (c) 2000 - 2020 Matthew T. Ashland.  All Rights Reserved.
***************************************************************************************/
#include "All.h"
#include <stdio.h>
#include "GlobalFunctions.h"
#include "MACLib.h"
#include "CharacterHelper.h"
using namespace APE;

// defines
#define COMPRESS_MODE		0
#define DECOMPRESS_MODE		1
#define VERIFY_MODE			2
#define CONVERT_MODE		3
#define UNDEFINED_MODE		-1

// use 8 bit character functions on non-Windows platforms
#ifndef PLATFORM_WINDOWS
	#define _tmain(argc, argv) main(argc, argv)
	#define _tcscpy_s(dst, num, src) strcpy(dst, src)
	#define _tcsncpy_s(dst, num, src, count) strncpy(dst, src, count)
	#define _tcsnicmp(str1, str2, count) strncasecmp(str1, str2, count)
	#define _ftprintf fprintf
	#define _ttoi(str) atoi(str)
	#define TCHAR char
	#define _T(x) x
#endif

// global variables
TICK_COUNT_TYPE g_nInitialTickCount = 0;

/***************************************************************************************
Displays the proper usage for MAC.exe
***************************************************************************************/
static void DisplayProperUsage(FILE * pFile)
{
	_ftprintf(pFile, _T("Proper Usage: [EXE] [Input File] [Output File] [Mode]\n\n"));

	_ftprintf(pFile, _T("Modes: \n"));
	_ftprintf(pFile, _T("    Compress (fast): '-c1000'\n"));
	_ftprintf(pFile, _T("    Compress (normal): '-c2000'\n"));
	_ftprintf(pFile, _T("    Compress (high): '-c3000'\n"));
	_ftprintf(pFile, _T("    Compress (extra high): '-c4000'\n"));
	_ftprintf(pFile, _T("    Compress (insane): '-c5000'\n"));
	_ftprintf(pFile, _T("    Decompress: '-d'\n"));
	_ftprintf(pFile, _T("    Verify: '-v'\n"));
	_ftprintf(pFile, _T("    Convert: '-nXXXX'\n\n"));

	_ftprintf(pFile, _T("Examples:\n"));
	_ftprintf(pFile, _T("    Compress: mac.exe \"Metallica - One.wav\" \"Metallica - One.ape\" -c2000\n"));
	_ftprintf(pFile, _T("    Decompress: mac.exe \"Metallica - One.ape\" \"Metallica - One.wav\" -d\n"));
	_ftprintf(pFile, _T("    Verify: mac.exe \"Metallica - One.ape\" -v\n"));
	_ftprintf(pFile, _T("    (note: int filenames must be put inside of quotations)\n"));
}

/***************************************************************************************
Progress callback
***************************************************************************************/
static void CALLBACK ProgressCallback(int nPercentageDone)
{
    // get the current tick count
	TICK_COUNT_TYPE  nTickCount;
	TICK_COUNT_READ(nTickCount);

	// calculate the progress
	double dProgress = nPercentageDone / 1.e5;											// [0...1]
	double dElapsed = (double) (nTickCount - g_nInitialTickCount) / TICK_COUNT_FREQ;	// seconds
	double dRemaining = dElapsed * ((1.0 / dProgress) - 1.0);							// seconds

	// output the progress
	_ftprintf(stderr, _T("Progress: %.1f%% (%.1f seconds remaining, %.1f seconds total)          \r"), 
		dProgress * 100, dRemaining, dElapsed);

	// don't forget to flush!
	fflush(stderr);
}

/***************************************************************************************
CtrlHandler callback
***************************************************************************************/
#ifdef PLATFORM_WINDOWS
static BOOL CALLBACK CtrlHandlerCallback(DWORD dwCtrlTyp)
{
	switch (dwCtrlTyp)
	{
	case CTRL_C_EVENT:
		_fputts(_T("\n\nCtrl+C: MAC has been interrupted !!!\n"), stderr);
		break;
	case CTRL_BREAK_EVENT:
		_fputts(_T("\n\nBreak: MAC has been interrupted !!!\n"), stderr);
		break;
	default:
		return FALSE;
	}

	fflush(stderr);
	ExitProcess(666);
	return TRUE;
}
#endif

/***************************************************************************************
Main (the main function)
***************************************************************************************/
int _tmain(int argc, TCHAR * argv[])
{
	// variable declares
	CSmartPtr<wchar_t> spInputFilename; CSmartPtr<wchar_t> spOutputFilename;
	int nRetVal = ERROR_UNDEFINED;
	int nMode = UNDEFINED_MODE;
	int nCompressionLevel = 0;
	int nPercentageDone;
	
	// initialize
	#ifdef PLATFORM_WINDOWS
		SetErrorMode(SetErrorMode(0x0003) | 0x0003);
		SetConsoleCtrlHandler(CtrlHandlerCallback, TRUE);
	#endif

	// output the header
	_ftprintf(stderr, CONSOLE_NAME);
	
	// make sure there are at least four arguments (could be more for EAC compatibility)
	if (argc < 3) 
	{
		DisplayProperUsage(stderr);
		exit(-1);
	}

	// store the filenames
	#ifdef PLATFORM_WINDOWS
		#ifdef _UNICODE
			spInputFilename.Assign(argv[1], TRUE, FALSE);
			spOutputFilename.Assign(argv[2], TRUE, FALSE);
		#else
			spInputFilename.Assign(CAPECharacterHelper::GetUTF16FromANSI(argv[1]), TRUE);
			spOutputFilename.Assign(CAPECharacterHelper::GetUTF16FromANSI(argv[2]), TRUE);
		#endif
	#else
		spInputFilename.Assign(CAPECharacterHelper::GetUTF16FromUTF8((str_utf8*) argv[1]), TRUE);
		spOutputFilename.Assign(CAPECharacterHelper::GetUTF16FromUTF8((str_utf8*) argv[2]), TRUE);
	#endif

	// verify that the input file exists
	if (!FileExists(spInputFilename))
	{
		_ftprintf(stderr, _T("Input File Not Found...\n\n"));
		exit(-1);
	}

	// if the output file equals '-v', then use this as the next argument
	TCHAR cMode[256];
	_tcsncpy_s(cMode, 256, argv[2], 255);

	if (_tcsnicmp(cMode, _T("-v"), 2) != 0)
	{
		// verify is the only mode that doesn't use at least the third argument
		if (argc < 4) 
		{
			DisplayProperUsage(stderr);
			exit(-1);
		}

		// check for and skip if necessary the -b XXXXXX arguments (3,4)
		_tcsncpy_s(cMode, 256, argv[3], 255);
	}

	// get the mode
	nMode = UNDEFINED_MODE;
	if (_tcsnicmp(cMode, _T("-c"), 2) == 0)
		nMode = COMPRESS_MODE;
	else if (_tcsnicmp(cMode, _T("-d"), 2) == 0)
		nMode = DECOMPRESS_MODE;
	else if (_tcsnicmp(cMode, _T("-v"), 2) == 0)
		nMode = VERIFY_MODE;
	else if (_tcsnicmp(cMode, _T("-n"), 2) == 0)
		nMode = CONVERT_MODE;

	// error check the mode
	if (nMode == UNDEFINED_MODE) 
	{
		DisplayProperUsage(stderr);
		exit(-1);
	}

	// get and error check the compression level
	if (nMode == COMPRESS_MODE || nMode == CONVERT_MODE) 
	{
		nCompressionLevel = _ttoi(&cMode[2]);
		if (nCompressionLevel != 1000 && nCompressionLevel != 2000 && 
			nCompressionLevel != 3000 && nCompressionLevel != 4000 &&
			nCompressionLevel != 5000) 
		{
			DisplayProperUsage(stderr);
			return -1;
		}
	}

	// set the initial tick count
	TICK_COUNT_READ(g_nInitialTickCount);
	
	// process
	int nKillFlag = 0;
	if (nMode == COMPRESS_MODE) 
	{
		TCHAR cCompressionLevel[16];
		if (nCompressionLevel == 1000) { _tcscpy_s(cCompressionLevel, 16, _T("fast")); }
		if (nCompressionLevel == 2000) { _tcscpy_s(cCompressionLevel, 16, _T("normal")); }
		if (nCompressionLevel == 3000) { _tcscpy_s(cCompressionLevel, 16, _T("high")); }
		if (nCompressionLevel == 4000) { _tcscpy_s(cCompressionLevel, 16, _T("extra high")); }
		if (nCompressionLevel == 5000) { _tcscpy_s(cCompressionLevel, 16, _T("insane")); }

		_ftprintf(stderr, _T("Compressing (%s)...\n"), cCompressionLevel);
		nRetVal = CompressFileW(spInputFilename, spOutputFilename, nCompressionLevel, &nPercentageDone, ProgressCallback, &nKillFlag);
	}
	else if (nMode == DECOMPRESS_MODE) 
	{
		_ftprintf(stderr, _T("Decompressing...\n"));
		nRetVal = DecompressFileW(spInputFilename, spOutputFilename, &nPercentageDone, ProgressCallback, &nKillFlag);
	}	
	else if (nMode == VERIFY_MODE) 
	{
		_ftprintf(stderr, _T("Verifying...\n"));
		nRetVal = VerifyFileW(spInputFilename, &nPercentageDone, ProgressCallback, &nKillFlag);
	}	
	else if (nMode == CONVERT_MODE) 
	{
		_ftprintf(stderr, _T("Converting...\n"));
		nRetVal = ConvertFileW(spInputFilename, spOutputFilename, nCompressionLevel, &nPercentageDone, ProgressCallback, &nKillFlag);
	}

	if (nRetVal == ERROR_SUCCESS) 
		_ftprintf(stderr, _T("\nSuccess...\n"));
	else 
		_ftprintf(stderr, _T("\nError: %i\n"), nRetVal);

	return nRetVal;
}
