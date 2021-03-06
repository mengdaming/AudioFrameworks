#pragma once

#include "IO.h"

namespace APE
{

/*************************************************************************************
CInputSource - base input format class (allows multiple format support)
*************************************************************************************/
class CInputSource
{
public:
    // construction / destruction
    CInputSource(CIO * pIO, WAVEFORMATEX * pwfeSource, int * pTotalBlocks, int64 * pHeaderBytes, int64 * pTerminatingBytes, int * pErrorCode = NULL) { }
    CInputSource(const wchar_t * pSourceName, WAVEFORMATEX * pwfeSource, int * pTotalBlocks, int64 * pHeaderBytes, int64 * pTerminatingBytes, int * pErrorCode = NULL) { }
    virtual ~CInputSource() { }
    
    // get data
    virtual int GetData(unsigned char * pBuffer, int nBlocks, int * pBlocksRetrieved) = 0;
    
    // get header / terminating data
    virtual int GetHeaderData(unsigned char * pBuffer) = 0;
    virtual int GetTerminatingData(unsigned char * pBuffer) = 0;
};

/*************************************************************************************
CWAVInputSource - wraps working with WAV files (could be extended to any format)
*************************************************************************************/
class CWAVInputSource : public CInputSource
{
public:
    // construction / destruction
    CWAVInputSource(CIO * pIO, WAVEFORMATEX * pwfeSource, int * pTotalBlocks, int64 * pHeaderBytes, int64 * pTerminatingBytes, int * pErrorCode = NULL);
    CWAVInputSource(const wchar_t * pSourceName, WAVEFORMATEX * pwfeSource, int * pTotalBlocks, int64 * pHeaderBytes, int64 * pTerminatingBytes, int * pErrorCode = NULL);
    ~CWAVInputSource();
    
    // get data
    int GetData(unsigned char * pBuffer, int nBlocks, int * pBlocksRetrieved);
    
    // get header / terminating data
    int GetHeaderData(unsigned char * pBuffer);
    int GetTerminatingData(unsigned char * pBuffer);

private:
    int AnalyzeSource();

    CSmartPtr<CIO> m_spIO;    
    WAVEFORMATEX m_wfeSource;
    int64 m_nHeaderBytes;
	int64 m_nDataBytes;
    int64 m_nTerminatingBytes;
    int64 m_nFileBytes;
    bool m_bIsValid;
};

/*************************************************************************************
Input souce creation
*************************************************************************************/
CInputSource * CreateInputSource(const wchar_t * pSourceName, WAVEFORMATEX * pwfeSource, int * pTotalBlocks, int64 * pHeaderBytes, int64 * pTerminatingBytes, int * pErrorCode = NULL);

}
