#include "All.h"
#include "APECompressCore.h"
#include "BitArray.h"
#include "Prepare.h"
#include "NewPredictor.h"

namespace APE
{
	
CAPECompressCore::CAPECompressCore(CIO * pIO, const WAVEFORMATEX * pwfeInput, intn nMaxFrameBlocks, intn nCompressionLevel)
{
	m_nMaxFrameBlocks = nMaxFrameBlocks;
    m_spBitArray.Assign(new CBitArray(pIO));
	intn nChannels = ape_max(pwfeInput->nChannels, 2);
	m_spData.Assign(new int[m_nMaxFrameBlocks * nChannels], true);
    m_spTempData.Assign(new int [nMaxFrameBlocks], true);
    m_spPrepare.Assign(new CPrepare);
	memset(m_aryPredictors, 0, 8 * sizeof(IPredictorCompress*));
	for (int nChannel = 0; nChannel < nChannels; nChannel++)
		m_aryPredictors[nChannel] = new CPredictorCompressNormal(nCompressionLevel);

    memcpy(&m_wfeInput, pwfeInput, sizeof(WAVEFORMATEX));
    m_nPeakLevel = 0;
}

CAPECompressCore::~CAPECompressCore()
{
	for (int z = 0; z < 8; z++)
	{
		if (m_aryPredictors[z] != NULL)
			delete m_aryPredictors[z];
	}
}

int CAPECompressCore::EncodeFrame(const void * pInputData, intn nInputBytes)
{
    // variables
    const intn nInputBlocks = nInputBytes / m_wfeInput.nBlockAlign;
    int nSpecialCodes = 0;

    // always start a new frame on a byte boundary
    m_spBitArray->AdvanceToByteBoundary();
    
    // do the preparation stage
    RETURN_ON_ERROR(Prepare(pInputData, (int) nInputBytes, &nSpecialCodes))

	for (int z = 0; z < 8; z++)
	{
		if (m_aryPredictors[z] != NULL)
			m_aryPredictors[z]->Flush();
		m_spBitArray->FlushState(m_aryBitArrayStates[z]);
	}

    m_spBitArray->FlushBitArray();

    if (m_wfeInput.nChannels == 2) 
    {
        bool bEncodeX = true;
        bool bEncodeY = true;
        
        if ((nSpecialCodes & SPECIAL_FRAME_LEFT_SILENCE) && 
            (nSpecialCodes & SPECIAL_FRAME_RIGHT_SILENCE)) 
        {
            bEncodeX = false;
            bEncodeY = false;
        }
        
        if (nSpecialCodes & SPECIAL_FRAME_PSEUDO_STEREO) 
        {
            bEncodeY = false;
        }
        
        if (bEncodeX && bEncodeY)
        {
            int nLastX = 0;
            for (int z = 0; z < nInputBlocks; z++)
            {
                m_spBitArray->EncodeValue(m_aryPredictors[1]->CompressValue(m_spData[m_nMaxFrameBlocks + z], nLastX), m_aryBitArrayStates[1]);
                m_spBitArray->EncodeValue(m_aryPredictors[0]->CompressValue(m_spData[z], m_spData[m_nMaxFrameBlocks + z]), m_aryBitArrayStates[0]);
                
                nLastX = m_spData[z];
            }
        }
        else if (bEncodeX) 
        {
            for (int z = 0; z < nInputBlocks; z++)
            {
                RETURN_ON_ERROR(m_spBitArray->EncodeValue(m_aryPredictors[0]->CompressValue(m_spData[z]), m_aryBitArrayStates[0]))
            }
        }
        else if (bEncodeY) 
        {
            for (int z = 0; z < nInputBlocks; z++)
            {
                RETURN_ON_ERROR(m_spBitArray->EncodeValue(m_aryPredictors[1]->CompressValue(m_spData[m_nMaxFrameBlocks + z]), m_aryBitArrayStates[1]))
            }
        }
    }
    else if (m_wfeInput.nChannels == 1) 
    {
        if (!(nSpecialCodes & SPECIAL_FRAME_MONO_SILENCE))
        {
            for (int z = 0; z < nInputBlocks; z++)
            {
                RETURN_ON_ERROR(m_spBitArray->EncodeValue(m_aryPredictors[0]->CompressValue(m_spData[z]), m_aryBitArrayStates[0]))
            }
        }
    }   
	else if (m_wfeInput.nChannels > 2)
	{
		for (int z = 0; z < nInputBlocks; z++)
		{
			for (int nChannel = 0; nChannel < m_wfeInput.nChannels; nChannel++)
			{
				m_spBitArray->EncodeValue(m_aryPredictors[nChannel]->CompressValue(m_spData[(nChannel * m_nMaxFrameBlocks) + z]), m_aryBitArrayStates[nChannel]);
			}
		}
	}

    m_spBitArray->Finalize();

    // return success
    return 0;
}

int CAPECompressCore::Prepare(const void * pInputData, int nInputBytes, int * pSpecialCodes)
{
    // variable declares
    *pSpecialCodes = 0;
    unsigned int nCRC = 0;
    
    // do the preparation
    RETURN_ON_ERROR(m_spPrepare->Prepare((unsigned char *) pInputData, nInputBytes, &m_wfeInput, m_spData, m_nMaxFrameBlocks,
        &nCRC, pSpecialCodes, &m_nPeakLevel))
    
    // store the CRC
    RETURN_ON_ERROR(m_spBitArray->EncodeUnsignedLong(nCRC))
    
    // store any special codes
    if (*pSpecialCodes != 0) 
    {
        RETURN_ON_ERROR(m_spBitArray->EncodeUnsignedLong(*pSpecialCodes))
    }
    
    return 0;
}

}

