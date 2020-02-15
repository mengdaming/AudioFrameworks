#pragma once

namespace APE
{

template <class TYPE> class CRollBuffer
{
public:
    CRollBuffer()
    {
        m_pData = NULL;
        m_pCurrent = NULL;
    }

    ~CRollBuffer()
    {
        SAFE_ARRAY_DELETE(m_pData);
    }

    int Create(intn nWindowElements, intn nHistoryElements)
    {
        SAFE_ARRAY_DELETE(m_pData)
        m_nWindowElements = nWindowElements;
        m_nHistoryElements = nHistoryElements;

        m_pData = new TYPE[m_nWindowElements + m_nHistoryElements];
        if (m_pData == NULL)
            return ERROR_INSUFFICIENT_MEMORY;

        Flush();
        return 0;
    }

    void Flush()
    {
        ZeroMemory(m_pData, (m_nHistoryElements + 1) * sizeof(TYPE));
        m_pCurrent = &m_pData[m_nHistoryElements];
    }

    void Roll()
    {
        memcpy(&m_pData[0], &m_pCurrent[-m_nHistoryElements], m_nHistoryElements * sizeof(TYPE));
        m_pCurrent = &m_pData[m_nHistoryElements];
    }

    __forceinline void IncrementSafe()
    {
        m_pCurrent++;
        if (m_pCurrent == &m_pData[m_nWindowElements + m_nHistoryElements])
            Roll();
    }

    __forceinline void IncrementFast()
    {
        m_pCurrent++;
    }

    __forceinline TYPE & operator[](const intn nIndex) const
    {
        return m_pCurrent[nIndex];
    }

protected:
    TYPE * m_pData;
    TYPE * m_pCurrent;
	intn m_nHistoryElements;
	intn m_nWindowElements;
};

template <class TYPE, int WINDOW_ELEMENTS, int HISTORY_ELEMENTS> class CRollBufferFast
{
public:
    CRollBufferFast()
    {
        m_pData = new TYPE[WINDOW_ELEMENTS + HISTORY_ELEMENTS];
        Flush();
    }

    ~CRollBufferFast()
    {
        SAFE_ARRAY_DELETE(m_pData);
    }

    void Flush()
    {
        ZeroMemory(m_pData, (HISTORY_ELEMENTS + 1) * sizeof(TYPE));
        m_pCurrent = &m_pData[HISTORY_ELEMENTS];
    }

    void Roll()
    {
        memcpy(&m_pData[0], &m_pCurrent[-HISTORY_ELEMENTS], HISTORY_ELEMENTS * sizeof(TYPE));
        m_pCurrent = &m_pData[HISTORY_ELEMENTS];
    }

    __forceinline void IncrementSafe()
    {
        m_pCurrent++;
        if (m_pCurrent == &m_pData[WINDOW_ELEMENTS + HISTORY_ELEMENTS])
            Roll();
    }

    __forceinline void IncrementFast()
    {
        m_pCurrent++;
    }

    __forceinline TYPE & operator[](const int nIndex) const
    {
        return m_pCurrent[nIndex];
    }

protected:
    TYPE * m_pData;
    TYPE * m_pCurrent;
};

}
