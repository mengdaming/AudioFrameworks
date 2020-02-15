#include "All.h"
#include "APEDecompress.h"
#include "APEInfo.h"
#include "Prepare.h"
#include "UnBitArray.h"
#include "NewPredictor.h"

namespace APE
{

#define DECODE_BLOCK_SIZE        4096

CAPEDecompress::CAPEDecompress(int * pErrorCode, CAPEInfo * pAPEInfo, int64 nStartBlock, int64 nFinishBlock)
{
    *pErrorCode = ERROR_SUCCESS;

    // open / analyze the file
    m_spAPEInfo.Assign(pAPEInfo);

    // version check (this implementation only works with 3.93 and later files)
    if (GetInfo(APE_INFO_FILE_VERSION) < 3930)
    {
        *pErrorCode = ERROR_UNDEFINED;
        return;
    }

    // get format information
    GetInfo(APE_INFO_WAVEFORMATEX, (intn) &m_wfeInput);
    m_nBlockAlign = GetInfo(APE_INFO_BLOCK_ALIGN);

    // initialize other stuff
    m_bDecompressorInitialized = false;
    m_nCurrentFrame = 0;
    m_nCurrentBlock = 0;
    m_nCurrentFrameBufferBlock = 0;
    m_nFrameBufferFinishedBlocks = 0;
    m_bErrorDecodingCurrentFrame = false;
    m_nErrorDecodingCurrentFrameOutputSilenceBlocks = 0;

    // set the "real" start and finish blocks
    m_nStartBlock = (nStartBlock < 0) ? 0 : ape_min(nStartBlock, (int)GetInfo(APE_INFO_TOTAL_BLOCKS));
	m_nFinishBlock = (nFinishBlock < 0) ? GetInfo(APE_INFO_TOTAL_BLOCKS) : ape_min(nFinishBlock, (int64) GetInfo(APE_INFO_TOTAL_BLOCKS));
    m_bIsRanged = (m_nStartBlock != 0) || (m_nFinishBlock != GetInfo(APE_INFO_TOTAL_BLOCKS));

	// channel data
	m_paryChannelData = new int [8];
}

CAPEDecompress::~CAPEDecompress()
{
	delete[] m_paryChannelData;
}

int CAPEDecompress::InitializeDecompressor()
{
    // check if we have anything to do
    if (m_bDecompressorInitialized)
        return ERROR_SUCCESS;

    // update the initialized flag
    m_bDecompressorInitialized = true;

	// check the block align
	if ((m_nBlockAlign <= 0) || (m_nBlockAlign > 32))
		return ERROR_INVALID_INPUT_FILE;

    // create a frame buffer
    m_cbFrameBuffer.CreateBuffer((intn(GetInfo(APE_INFO_BLOCKS_PER_FRAME)) + DECODE_BLOCK_SIZE) * m_nBlockAlign, m_nBlockAlign * 64);
    
    // create decoding components
    m_spUnBitArray.Assign((CUnBitArrayBase *) CreateUnBitArray(this, GetInfo(APE_INFO_FILE_VERSION)));
    if (m_spUnBitArray == NULL)
        return ERROR_UPSUPPORTED_FILE_VERSION;

	// create the predictors
	memset(m_aryPredictor, 0, sizeof(m_aryPredictor));
	intn nChannels = ape_min(ape_max(GetInfo(APE_INFO_CHANNELS), 2), 8);
    if (GetInfo(APE_INFO_FILE_VERSION) >= 3950)
    {
		for (intn nChannel = 0; nChannel < nChannels; nChannel++)
			m_aryPredictor[nChannel] = new CPredictorDecompress3950toCurrent(GetInfo(APE_INFO_COMPRESSION_LEVEL), GetInfo(APE_INFO_FILE_VERSION));
    }
    else
    {
		for (intn nChannel = 0; nChannel < nChannels; nChannel++)
			m_aryPredictor[nChannel] = new CPredictorDecompressNormal3930to3950(GetInfo(APE_INFO_COMPRESSION_LEVEL), GetInfo(APE_INFO_FILE_VERSION));
    }
    
    // seek to the beginning
    return Seek(0);
}

int CAPEDecompress::GetData(char * pBuffer, intn nBlocks, intn * pBlocksRetrieved)
{
    int nResult = ERROR_SUCCESS;
    if (pBlocksRetrieved) *pBlocksRetrieved = 0;
    
    // make sure we're initialized
    RETURN_ON_ERROR(InitializeDecompressor())
			
    // cap
    int64 nBlocksUntilFinish = m_nFinishBlock - m_nCurrentBlock;
    const int64 nBlocksToRetrieve = ape_min(nBlocks, nBlocksUntilFinish);
    
    // get the data
    unsigned char * pOutputBuffer = (unsigned char *) pBuffer;
    int64 nBlocksLeft = nBlocksToRetrieve; int64 nBlocksThisPass = 1;
    while ((nBlocksLeft > 0) && (nBlocksThisPass > 0))
    {
        // fill up the frame buffer
        int nDecodeRetVal = FillFrameBuffer();
        if (nDecodeRetVal != ERROR_SUCCESS)
            nResult = nDecodeRetVal;

        // analyze how much to remove from the buffer
        const int64 nFrameBufferBlocks = m_nFrameBufferFinishedBlocks;
        nBlocksThisPass = ape_min(nBlocksLeft, nFrameBufferBlocks);

        // remove as much as possible
        if (nBlocksThisPass > 0)
        {
            m_cbFrameBuffer.Get(pOutputBuffer, intn(nBlocksThisPass * m_nBlockAlign));
            pOutputBuffer += nBlocksThisPass * m_nBlockAlign;
            nBlocksLeft -= nBlocksThisPass;
            m_nFrameBufferFinishedBlocks -= intn(nBlocksThisPass);
        }
    }

    // calculate the blocks retrieved
	intn nBlocksRetrieved = intn(nBlocksToRetrieve - nBlocksLeft);

    // update position
    m_nCurrentBlock += nBlocksRetrieved;
    if (pBlocksRetrieved) *pBlocksRetrieved = nBlocksRetrieved;

    return nResult;
}

int CAPEDecompress::Seek(intn nBlockOffset)
{
    RETURN_ON_ERROR(InitializeDecompressor())

    // use the offset
    nBlockOffset += intn(m_nStartBlock);
    
    // cap (to prevent seeking too far)
    if (nBlockOffset >= m_nFinishBlock)
        nBlockOffset = intn(m_nFinishBlock - 1);
    if (nBlockOffset < m_nStartBlock)
        nBlockOffset = intn(m_nStartBlock);

    // seek to the perfect location
	intn nBaseFrame = nBlockOffset / GetInfo(APE_INFO_BLOCKS_PER_FRAME);
	intn nBlocksToSkip = nBlockOffset % GetInfo(APE_INFO_BLOCKS_PER_FRAME);
	intn nBytesToSkip = nBlocksToSkip * m_nBlockAlign;
        
    m_nCurrentBlock = nBaseFrame * GetInfo(APE_INFO_BLOCKS_PER_FRAME);
    m_nCurrentFrameBufferBlock = nBaseFrame * GetInfo(APE_INFO_BLOCKS_PER_FRAME);
    m_nCurrentFrame = nBaseFrame;
    m_nFrameBufferFinishedBlocks = 0;
    m_cbFrameBuffer.Empty();
    RETURN_ON_ERROR(SeekToFrame(m_nCurrentFrame));

    // skip necessary blocks
    CSmartPtr<char> spTempBuffer(new char [nBytesToSkip], true);
    if (spTempBuffer == NULL) return ERROR_INSUFFICIENT_MEMORY;
    
	intn nBlocksRetrieved = 0;
    GetData(spTempBuffer, nBlocksToSkip, &nBlocksRetrieved);
    if (nBlocksRetrieved != nBlocksToSkip)
        return ERROR_UNDEFINED;

    return ERROR_SUCCESS;
}

/*****************************************************************************************
Decodes blocks of data
*****************************************************************************************/
int CAPEDecompress::FillFrameBuffer()
{
    int nResult = ERROR_SUCCESS;

    // determine the maximum blocks we can decode
    // note that we won't do end capping because we can't use data
    // until EndFrame(...) successfully handles the frame
    // that means we may decode a little extra in end capping cases
    // but this allows robust error handling of bad frames

    // loop and decode data
	intn nBlocksLeft = m_cbFrameBuffer.MaxAdd() / m_nBlockAlign;
    while (nBlocksLeft > 0)
    {
        // output silence from previous error
        if (m_nErrorDecodingCurrentFrameOutputSilenceBlocks > 0)
        {
            // output silence
			intn nOutputSilenceBlocks = ape_min(m_nErrorDecodingCurrentFrameOutputSilenceBlocks, nBlocksLeft);
            unsigned char cSilence = (GetInfo(APE_INFO_BITS_PER_SAMPLE) == 8) ? 127 : 0;
            for (int z = 0; z < nOutputSilenceBlocks * m_nBlockAlign; z++)
            {
                *m_cbFrameBuffer.GetDirectWritePointer() = cSilence;
                m_cbFrameBuffer.UpdateAfterDirectWrite(1);
            }

            // decrement
            m_nErrorDecodingCurrentFrameOutputSilenceBlocks -= nOutputSilenceBlocks;
            nBlocksLeft -= nOutputSilenceBlocks;
            m_nFrameBufferFinishedBlocks += nOutputSilenceBlocks;
            m_nCurrentFrameBufferBlock += nOutputSilenceBlocks;
            if (nBlocksLeft <= 0)
                break;
        }

        // get frame size
		intn nFrameBlocks = GetInfo(APE_INFO_FRAME_BLOCKS, m_nCurrentFrame);
		if (nFrameBlocks < 0)
			break;

        // analyze
		intn nFrameOffsetBlocks = m_nCurrentFrameBufferBlock % GetInfo(APE_INFO_BLOCKS_PER_FRAME);
		intn nFrameBlocksLeft = nFrameBlocks - nFrameOffsetBlocks;
		intn nBlocksThisPass = ape_min(nFrameBlocksLeft, nBlocksLeft);

        // start the frame if we need to
        if (nFrameOffsetBlocks == 0)
            StartFrame();

        // decode data
        DecodeBlocksToFrameBuffer(nBlocksThisPass);
            
        // end the frame if we decoded all the blocks from the current frame
        bool bEndedFrame = false;
        if ((nFrameOffsetBlocks + nBlocksThisPass) >= nFrameBlocks)
        {
            EndFrame();
            bEndedFrame = true;
        }

        // handle errors (either mid-frame or from a CRC at the end of the frame)
        if (m_bErrorDecodingCurrentFrame)
        {
			intn nFrameBlocksDecoded = 0;
            if (bEndedFrame)
            {   
                // remove the frame buffer blocks that have been marked as good
                m_nFrameBufferFinishedBlocks -= GetInfo(APE_INFO_FRAME_BLOCKS, m_nCurrentFrame - 1);
                
                // assume that the frame buffer contains the correct number of blocks for the entire frame
                nFrameBlocksDecoded = GetInfo(APE_INFO_FRAME_BLOCKS, m_nCurrentFrame - 1);
            }
            else
            {
                // move to the next frame
                m_nCurrentFrame++;

                // calculate how many blocks were output before we errored
                nFrameBlocksDecoded = m_nCurrentFrameBufferBlock - (GetInfo(APE_INFO_BLOCKS_PER_FRAME) * (m_nCurrentFrame - 1));
            }

            // remove any decoded data for this frame from the buffer
			intn nFrameBytesDecoded = nFrameBlocksDecoded * m_nBlockAlign;
            m_cbFrameBuffer.RemoveTail(nFrameBytesDecoded);

            // seek to try to synchronize after an error
            if (m_nCurrentFrame < GetInfo(APE_INFO_TOTAL_FRAMES))
                SeekToFrame(m_nCurrentFrame);

            // reset our frame buffer position to the beginning of the frame
            m_nCurrentFrameBufferBlock = (m_nCurrentFrame - 1) * GetInfo(APE_INFO_BLOCKS_PER_FRAME);

            // output silence for the duration of the error frame (we can't just dump it to the
            // frame buffer here since the frame buffer may not be large enough to hold the
            // duration of the entire frame)
            m_nErrorDecodingCurrentFrameOutputSilenceBlocks += nFrameBlocks;

            // save the return value
            nResult = ERROR_INVALID_CHECKSUM;
        }

        // update the number of blocks that still fit in the buffer
        nBlocksLeft = m_cbFrameBuffer.MaxAdd() / m_nBlockAlign;
    }

    return nResult;
}

void CAPEDecompress::DecodeBlocksToFrameBuffer(intn nBlocks)
{
    // decode the samples
	intn nBlocksProcessed = 0;
	intn nFrameBufferBytes = m_cbFrameBuffer.MaxGet();

    try
    {
		if (m_wfeInput.nChannels > 2)
		{
			for (nBlocksProcessed = 0; nBlocksProcessed < nBlocks; nBlocksProcessed++)
			{
				for (int nChannel = 0; nChannel < m_wfeInput.nChannels; nChannel++)
				{
					int nValue = m_spUnBitArray->DecodeValueRange(m_aryBitArrayStates[nChannel]);
					int nValue2 = m_aryPredictor[nChannel]->DecompressValue(nValue, 0);
					m_paryChannelData[nChannel] = nValue2;
				}
				m_Prepare.Unprepare(m_paryChannelData, &m_wfeInput, m_cbFrameBuffer.GetDirectWritePointer());
				m_cbFrameBuffer.UpdateAfterDirectWrite(m_nBlockAlign);
			}
		}
        else if (m_wfeInput.nChannels == 2)
        {
            if ((m_nSpecialCodes & SPECIAL_FRAME_LEFT_SILENCE) && 
                (m_nSpecialCodes & SPECIAL_FRAME_RIGHT_SILENCE)) 
            {
                for (nBlocksProcessed = 0; nBlocksProcessed < nBlocks; nBlocksProcessed++)
                {
					int aryValues[2] = { 0, 0 };
					m_Prepare.Unprepare(aryValues, &m_wfeInput, m_cbFrameBuffer.GetDirectWritePointer());
                    m_cbFrameBuffer.UpdateAfterDirectWrite(m_nBlockAlign);
                }
            }
            else if (m_nSpecialCodes & SPECIAL_FRAME_PSEUDO_STEREO)
            {
                for (nBlocksProcessed = 0; nBlocksProcessed < nBlocks; nBlocksProcessed++)
                {
					int aryValues[2] = { m_aryPredictor[0]->DecompressValue(m_spUnBitArray->DecodeValueRange(m_aryBitArrayStates[0])), 0 };

                    m_Prepare.Unprepare(aryValues, &m_wfeInput, m_cbFrameBuffer.GetDirectWritePointer());
                    m_cbFrameBuffer.UpdateAfterDirectWrite(m_nBlockAlign);
                }
            }    
            else
            {
                if (m_spAPEInfo->GetInfo(APE_INFO_FILE_VERSION) >= 3950)
                {
                    for (nBlocksProcessed = 0; nBlocksProcessed < nBlocks; nBlocksProcessed++)
                    {
                        int nY = m_spUnBitArray->DecodeValueRange(m_aryBitArrayStates[1]);
                        int nX = m_spUnBitArray->DecodeValueRange(m_aryBitArrayStates[0]);
                        int Y = m_aryPredictor[1]->DecompressValue(nY, m_nLastX);
                        int X = m_aryPredictor[0]->DecompressValue(nX, Y);
                        m_nLastX = X;

						int aryValues[2] = { X, Y };
                        m_Prepare.Unprepare(aryValues, &m_wfeInput, m_cbFrameBuffer.GetDirectWritePointer());
                        m_cbFrameBuffer.UpdateAfterDirectWrite(m_nBlockAlign);
                    }
                }
                else
                {
                    for (nBlocksProcessed = 0; nBlocksProcessed < nBlocks; nBlocksProcessed++)
                    {
                        int X = m_aryPredictor[0]->DecompressValue(m_spUnBitArray->DecodeValueRange(m_aryBitArrayStates[0]));
                        int Y = m_aryPredictor[1]->DecompressValue(m_spUnBitArray->DecodeValueRange(m_aryBitArrayStates[1]));

						int aryValues[2] = { X, Y };
                        m_Prepare.Unprepare(aryValues, &m_wfeInput, m_cbFrameBuffer.GetDirectWritePointer());
                        m_cbFrameBuffer.UpdateAfterDirectWrite(m_nBlockAlign);
                    }
                }
            }
        }
		else if (m_wfeInput.nChannels == 1)
        {
            if (m_nSpecialCodes & SPECIAL_FRAME_MONO_SILENCE)
            {
                for (nBlocksProcessed = 0; nBlocksProcessed < nBlocks; nBlocksProcessed++)
                {
					int aryValues[2] = { 0, 0 };
					m_Prepare.Unprepare(aryValues, &m_wfeInput, m_cbFrameBuffer.GetDirectWritePointer());
                    m_cbFrameBuffer.UpdateAfterDirectWrite(m_nBlockAlign);
                }
            }
            else
            {
                for (nBlocksProcessed = 0; nBlocksProcessed < nBlocks; nBlocksProcessed++)
                {
					int aryValues[2] = { m_aryPredictor[0]->DecompressValue(m_spUnBitArray->DecodeValueRange(m_aryBitArrayStates[0])), 0 };
                    m_Prepare.Unprepare(aryValues, &m_wfeInput, m_cbFrameBuffer.GetDirectWritePointer());
                    m_cbFrameBuffer.UpdateAfterDirectWrite(m_nBlockAlign);
                }
            }
        }
    }
    catch(...)
    {
        m_bErrorDecodingCurrentFrame = true;
    }

    // get actual blocks that have been decoded and added to the frame buffer
	intn nActualBlocks = (m_cbFrameBuffer.MaxGet() - nFrameBufferBytes) / m_nBlockAlign;
	nActualBlocks = ape_max(nActualBlocks, 0);
    if (nBlocks != nActualBlocks)
        m_bErrorDecodingCurrentFrame = true;

    // update CRC
    m_nCRC = m_cbFrameBuffer.UpdateCRC(m_nCRC, nActualBlocks * m_nBlockAlign);

    // bump frame decode position
    m_nCurrentFrameBufferBlock += nActualBlocks;
}

void CAPEDecompress::StartFrame()
{
    m_nCRC = 0xFFFFFFFF;
    
    // get the frame header
    m_nStoredCRC = m_spUnBitArray->DecodeValue(DECODE_VALUE_METHOD_UNSIGNED_INT);
    m_bErrorDecodingCurrentFrame = false;
    m_nErrorDecodingCurrentFrameOutputSilenceBlocks = 0;

    // get any 'special' codes if the file uses them (for silence, false stereo, etc.)
    m_nSpecialCodes = 0;
    if (GET_USES_SPECIAL_FRAMES(m_spAPEInfo))
    {
        if (m_nStoredCRC & 0x80000000) 
        {
            m_nSpecialCodes = m_spUnBitArray->DecodeValue(DECODE_VALUE_METHOD_UNSIGNED_INT);
        }
        m_nStoredCRC &= 0x7FFFFFFF;
    }

	for (int z = 0; z < 8; z++)
	{
		if (m_aryPredictor[z] != NULL)
			m_aryPredictor[z]->Flush();
	}

	for (int z = 0; z < 8; z++)
	{
		m_spUnBitArray->FlushState(m_aryBitArrayStates[z]);
	}
    
    m_spUnBitArray->FlushBitArray();

    m_nLastX = 0;
}

void CAPEDecompress::EndFrame()
{
    m_nFrameBufferFinishedBlocks += GetInfo(APE_INFO_FRAME_BLOCKS, m_nCurrentFrame);
    m_nCurrentFrame++;

    // finalize
    m_spUnBitArray->Finalize();

    // check the CRC
    m_nCRC = m_nCRC ^ 0xFFFFFFFF;
    m_nCRC >>= 1;
    if (m_nCRC != m_nStoredCRC)
    {
        // error
        m_bErrorDecodingCurrentFrame = true;

        // We didn't use to check the CRC of the last frame in MAC 3.98 and earlier.  This caused some confusion for one
        // user that had a lot of 3.97 Extra High files that have CRC errors on the last frame.  They would verify
        // with old versions, but not with newer versions.  It's still unknown what corrupted the user's files but since
        // only the last frame was bad, it's likely to have been caused by a buggy tagger.
        //if ((m_nCurrentFrame >= GetInfo(APE_INFO_TOTAL_FRAMES)) && (GetInfo(APE_INFO_FILE_VERSION) < 3990))
        //    m_bErrorDecodingCurrentFrame = false;
    }
}

/*****************************************************************************************
Seek to the proper frame (if necessary) and do any alignment of the bit array
*****************************************************************************************/
int CAPEDecompress::SeekToFrame(intn nFrameIndex)
{
	intn nSeekRemainder = ((uintn)GetInfo(APE_INFO_SEEK_BYTE, nFrameIndex) - (uintn)GetInfo(APE_INFO_SEEK_BYTE, 0)) % 4;
    return m_spUnBitArray->FillAndResetBitArray((uintn)GetInfo(APE_INFO_SEEK_BYTE, nFrameIndex) - nSeekRemainder, nSeekRemainder * 8);
}

/*****************************************************************************************
Get information from the decompressor
*****************************************************************************************/
intn CAPEDecompress::GetInfo(APE_DECOMPRESS_FIELDS Field, intn nParam1, intn nParam2)
{
    intn nResult = 0;
    bool bHandled = true;

    switch (Field)
    {
    case APE_DECOMPRESS_CURRENT_BLOCK:
        nResult = intn(m_nCurrentBlock - m_nStartBlock);
        break;
    case APE_DECOMPRESS_CURRENT_MS:
    {
		intn nSampleRate = m_spAPEInfo->GetInfo(APE_INFO_SAMPLE_RATE, 0, 0);
        if (nSampleRate > 0)
            nResult = int((double(m_nCurrentBlock) * double(1000)) / double(nSampleRate));
        break;
    }
    case APE_DECOMPRESS_TOTAL_BLOCKS:
        nResult = intn(m_nFinishBlock - m_nStartBlock);
        break;
    case APE_DECOMPRESS_LENGTH_MS:
    {
		intn nSampleRate = m_spAPEInfo->GetInfo(APE_INFO_SAMPLE_RATE, 0, 0);
        if (nSampleRate > 0)
            nResult = int((double(m_nFinishBlock - m_nStartBlock) * double(1000)) / double(nSampleRate));
        break;
    }
    case APE_DECOMPRESS_CURRENT_BITRATE:
        nResult = GetInfo(APE_INFO_FRAME_BITRATE, m_nCurrentFrame);
        break;
    case APE_DECOMPRESS_CURRENT_FRAME:
        nResult = m_nCurrentFrame;
        break;
    case APE_DECOMPRESS_AVERAGE_BITRATE:
    {
        if (m_bIsRanged)
        {
            // figure the frame range
            const intn nBlocksPerFrame = GetInfo(APE_INFO_BLOCKS_PER_FRAME);
			intn nStartFrame = intn(m_nStartBlock / nBlocksPerFrame);
			intn nFinishFrame = intn(m_nFinishBlock + nBlocksPerFrame - 1) / nBlocksPerFrame;

            // get the number of bytes in the first and last frame
			intn nTotalBytes = (GetInfo(APE_INFO_FRAME_BYTES, nStartFrame) * (m_nStartBlock % nBlocksPerFrame)) / nBlocksPerFrame;
            if (nFinishFrame != nStartFrame)
                nTotalBytes += (GetInfo(APE_INFO_FRAME_BYTES, nFinishFrame) * (m_nFinishBlock % nBlocksPerFrame)) / nBlocksPerFrame;

            // get the number of bytes in between
            const intn nTotalFrames = GetInfo(APE_INFO_TOTAL_FRAMES);
            for (intn nFrame = nStartFrame + 1; (nFrame < nFinishFrame) && (nFrame < nTotalFrames); nFrame++)
                nTotalBytes += GetInfo(APE_INFO_FRAME_BYTES, nFrame);

            // figure the bitrate
			intn nTotalMS = intn((double(m_nFinishBlock - m_nStartBlock) * double(1000)) / double(GetInfo(APE_INFO_SAMPLE_RATE)));
            if (nTotalMS != 0)
                nResult = (nTotalBytes * 8) / nTotalMS;
        }
        else
        {
            nResult = GetInfo(APE_INFO_AVERAGE_BITRATE);
        }

        break;
    }
    default:
        bHandled = false;
    }

    if (!bHandled && m_bIsRanged)
    {
        bHandled = true;

        switch (Field)
        {
        case APE_INFO_WAV_HEADER_BYTES:
            nResult = sizeof(WAVE_HEADER);
            break;
        case APE_INFO_WAV_HEADER_DATA:
        {
            char * pBuffer = (char *) nParam1;
			intn nMaxBytes = nParam2;
            
            if ((APE::intn) sizeof(WAVE_HEADER) > nMaxBytes)
            {
                nResult = -1;
            }
            else
            {
                WAVEFORMATEX wfeFormat; GetInfo(APE_INFO_WAVEFORMATEX, (intn) &wfeFormat, 0);
                WAVE_HEADER WAVHeader; FillWaveHeader(&WAVHeader, 
                    intn(m_nFinishBlock - m_nStartBlock) * GetInfo(APE_INFO_BLOCK_ALIGN), 
                    &wfeFormat, 0);
                memcpy(pBuffer, &WAVHeader, sizeof(WAVE_HEADER));
                nResult = 0;
            }
            break;
        }
        case APE_INFO_WAV_TERMINATING_BYTES:
            nResult = 0;
            break;
        case APE_INFO_WAV_TERMINATING_DATA:
            nResult = 0;
            break;
        default:
            bHandled = false;
        }
    }

    if (!bHandled)
        nResult = m_spAPEInfo->GetInfo(Field, nParam1, nParam2);

    return nResult;
}

}

