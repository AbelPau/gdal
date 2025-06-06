/******************************************************************************
 *
 * Project:  MiraMonRaster driver
 * Purpose:  Implements MMRRasterAttributeTable class.
 * Author:   Abel Pau
 * 
 ******************************************************************************
 * Copyright (c) 2025, Xavier Pons
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

//#include "cpl_port.h"
#include "miramondataset.h"
#include "miramon_atributte_table.h"

#include <algorithm>

/************************************************************************/
/*                     MMRRasterAttributeTable()                        */
/************************************************************************/

MMRRasterAttributeTable::MMRRasterAttributeTable(MMRRasterBand *poBand,
                                                 const char *pszName)
    : hMMR(poBand->hMMR),
      poDT(poBand->hMMR->papoBand[poBand->nBand - 1]->poNode->GetNamedChild(
          pszName)),
      osName(pszName), nBand(poBand->nBand), eAccess(poBand->GetAccess()),
      nRows(0), bLinearBinning(false), dfRow0Min(0.0), dfBinSize(0.0),
      eTableType(GRTT_THEMATIC)
{
    if (poDT != nullptr)
    {
        nRows = poDT->GetIntField("numRows");

        // Scan under table for columns.
        for (MMREntry *poDTChild = poDT->GetChild(); poDTChild != nullptr;
             poDTChild = poDTChild->GetNext())
        {
            if (EQUAL(poDTChild->GetType(), "Edsc_BinFunction"))
            {
                const double dfMax = poDTChild->GetDoubleField("maxLimit");
                const double dfMin = poDTChild->GetDoubleField("minLimit");
                const int nBinCount = poDTChild->GetIntField("numBins");

                if (nBinCount == nRows && dfMax != dfMin && nBinCount > 1)
                {
                    // Can't call SetLinearBinning since it will re-write
                    // which we might not have permission to do.
                    bLinearBinning = true;
                    dfRow0Min = dfMin;
                    dfBinSize = (dfMax - dfMin) / (nBinCount - 1);
                }
            }

            if (EQUAL(poDTChild->GetType(), "Edsc_BinFunction840"))
            {
                const char *pszValue =
                    poDTChild->GetStringField("binFunction.type.string");
                if (pszValue && EQUAL(pszValue, "BFUnique"))
                {
                    AddColumn("BinValues", GFT_Real, GFU_MinMax, 0, 0,
                              poDTChild, true);
                }
            }

            if (!EQUAL(poDTChild->GetType(), "Edsc_Column"))
                continue;

            const int nOffset = poDTChild->GetIntField("columnDataPtr");
            const char *pszType = poDTChild->GetStringField("dataType");
            GDALRATFieldUsage eUsage = GFU_Generic;
            bool bConvertColors = false;

            if (pszType == nullptr || nOffset == 0)
                continue;

            GDALRATFieldType eType;
            if (EQUAL(pszType, "real"))
                eType = GFT_Real;
            else if (EQUAL(pszType, "string"))
                eType = GFT_String;
            else if (STARTS_WITH_CI(pszType, "int"))
                eType = GFT_Integer;
            else
                continue;

            if (EQUAL(poDTChild->GetName(), "Histogram"))
                eUsage = GFU_PixelCount;
            else if (EQUAL(poDTChild->GetName(), "Red"))
            {
                eUsage = GFU_Red;
                // Treat color columns as ints regardless
                // of how they are stored.
                bConvertColors = eType == GFT_Real;
                eType = GFT_Integer;
            }
            else if (EQUAL(poDTChild->GetName(), "Green"))
            {
                eUsage = GFU_Green;
                bConvertColors = eType == GFT_Real;
                eType = GFT_Integer;
            }
            else if (EQUAL(poDTChild->GetName(), "Blue"))
            {
                eUsage = GFU_Blue;
                bConvertColors = eType == GFT_Real;
                eType = GFT_Integer;
            }
            else if (EQUAL(poDTChild->GetName(), "Opacity"))
            {
                eUsage = GFU_Alpha;
                bConvertColors = eType == GFT_Real;
                eType = GFT_Integer;
            }
            else if (EQUAL(poDTChild->GetName(), "Class_Names"))
                eUsage = GFU_Name;

            if (eType == GFT_Real)
            {
                AddColumn(poDTChild->GetName(), GFT_Real, eUsage, nOffset,
                          sizeof(double), poDTChild);
            }
            else if (eType == GFT_String)
            {
                int nMaxNumChars = poDTChild->GetIntField("maxNumChars");
                if (nMaxNumChars <= 0)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Invalid nMaxNumChars = %d for column %s",
                             nMaxNumChars, poDTChild->GetName());
                    nMaxNumChars = 1;
                }
                AddColumn(poDTChild->GetName(), GFT_String, eUsage, nOffset,
                          nMaxNumChars, poDTChild);
            }
            else if (eType == GFT_Integer)
            {
                int nSize = sizeof(GInt32);
                if (bConvertColors)
                    nSize = sizeof(double);
                AddColumn(poDTChild->GetName(), GFT_Integer, eUsage, nOffset,
                          nSize, poDTChild, false, bConvertColors);
            }
        }
    }
}

/************************************************************************/
/*                    ~MMRRasterAttributeTable()                        */
/************************************************************************/

MMRRasterAttributeTable::~MMRRasterAttributeTable()
{
}

/************************************************************************/
/*                              Clone()                                 */
/************************************************************************/

GDALRasterAttributeTable *MMRRasterAttributeTable::Clone() const
{
    if ((GetRowCount() * GetColumnCount()) > RAT_MAX_ELEM_FOR_CLONE)
        return nullptr;

    GDALDefaultRasterAttributeTable *poRAT =
        new GDALDefaultRasterAttributeTable();

    for (int iCol = 0; iCol < static_cast<int>(aoFields.size()); iCol++)
    {
        poRAT->CreateColumn(aoFields[iCol].sName, aoFields[iCol].eType,
                            aoFields[iCol].eUsage);
        poRAT->SetRowCount(nRows);

        if (aoFields[iCol].eType == GFT_Integer)
        {
            int *panColData =
                static_cast<int *>(VSI_MALLOC2_VERBOSE(sizeof(int), nRows));
            if (panColData == nullptr)
            {
                delete poRAT;
                return nullptr;
            }

            if (((GDALDefaultRasterAttributeTable *)this)
                    ->ValuesIO(GF_Read, iCol, 0, nRows, panColData) != CE_None)
            {
                CPLFree(panColData);
                delete poRAT;
                return nullptr;
            }

            for (int iRow = 0; iRow < nRows; iRow++)
            {
                poRAT->SetValue(iRow, iCol, panColData[iRow]);
            }
            CPLFree(panColData);
        }
        if (aoFields[iCol].eType == GFT_Real)
        {
            double *padfColData = static_cast<double *>(
                VSI_MALLOC2_VERBOSE(sizeof(double), nRows));
            if (padfColData == nullptr)
            {
                delete poRAT;
                return nullptr;
            }

            if (((GDALDefaultRasterAttributeTable *)this)
                    ->ValuesIO(GF_Read, iCol, 0, nRows, padfColData) != CE_None)
            {
                CPLFree(padfColData);
                delete poRAT;
                return nullptr;
            }

            for (int iRow = 0; iRow < nRows; iRow++)
            {
                poRAT->SetValue(iRow, iCol, padfColData[iRow]);
            }
            CPLFree(padfColData);
        }
        if (aoFields[iCol].eType == GFT_String)
        {
            char **papszColData = static_cast<char **>(
                VSI_MALLOC2_VERBOSE(sizeof(char *), nRows));
            if (papszColData == nullptr)
            {
                delete poRAT;
                return nullptr;
            }

            if (((GDALDefaultRasterAttributeTable *)this)
                    ->ValuesIO(GF_Read, iCol, 0, nRows, papszColData) !=
                CE_None)
            {
                CPLFree(papszColData);
                delete poRAT;
                return nullptr;
            }

            for (int iRow = 0; iRow < nRows; iRow++)
            {
                poRAT->SetValue(iRow, iCol, papszColData[iRow]);
                CPLFree(papszColData[iRow]);
            }
            CPLFree(papszColData);
        }
    }

    if (bLinearBinning)
        poRAT->SetLinearBinning(dfRow0Min, dfBinSize);

    poRAT->SetTableType(this->GetTableType());

    return poRAT;
}

/************************************************************************/
/*                          GetColumnCount()                            */
/************************************************************************/

int MMRRasterAttributeTable::GetColumnCount() const
{
    return static_cast<int>(aoFields.size());
}

/************************************************************************/
/*                          GetNameOfCol()                              */
/************************************************************************/

const char *MMRRasterAttributeTable::GetNameOfCol(int nCol) const
{
    if (nCol < 0 || nCol >= static_cast<int>(aoFields.size()))
        return nullptr;

    return aoFields[nCol].sName;
}

/************************************************************************/
/*                          GetUsageOfCol()                             */
/************************************************************************/

GDALRATFieldUsage MMRRasterAttributeTable::GetUsageOfCol(int nCol) const
{
    if (nCol < 0 || nCol >= static_cast<int>(aoFields.size()))
        return GFU_Generic;

    return aoFields[nCol].eUsage;
}

/************************************************************************/
/*                          GetTypeOfCol()                              */
/************************************************************************/

GDALRATFieldType MMRRasterAttributeTable::GetTypeOfCol(int nCol) const
{
    if (nCol < 0 || nCol >= static_cast<int>(aoFields.size()))
        return GFT_Integer;

    return aoFields[nCol].eType;
}

/************************************************************************/
/*                          GetColOfUsage()                             */
/************************************************************************/

int MMRRasterAttributeTable::GetColOfUsage(GDALRATFieldUsage eUsage) const
{
    for (unsigned int i = 0; i < aoFields.size(); i++)
    {
        if (aoFields[i].eUsage == eUsage)
            return i;
    }

    return -1;
}

/************************************************************************/
/*                          GetRowCount()                               */
/************************************************************************/

int MMRRasterAttributeTable::GetRowCount() const
{
    return nRows;
}

/************************************************************************/
/*                      GetValueAsString()                              */
/************************************************************************/

const char *MMRRasterAttributeTable::GetValueAsString(int iRow,
                                                      int iField) const
{
    // Get ValuesIO do do the work.
    char *apszStrList[1] = {nullptr};
    if (((MMRRasterAttributeTable *)this)
            ->ValuesIO(GF_Read, iField, iRow, 1, apszStrList) != CE_None)
    {
        return "";
    }

    ((MMRRasterAttributeTable *)this)->osWorkingResult = apszStrList[0];
    CPLFree(apszStrList[0]);

    return osWorkingResult;
}

/************************************************************************/
/*                        GetValueAsInt()                               */
/************************************************************************/

int MMRRasterAttributeTable::GetValueAsInt(int iRow, int iField) const
{
    // Get ValuesIO do do the work.
    int nValue = 0;
    if (((MMRRasterAttributeTable *)this)
            ->ValuesIO(GF_Read, iField, iRow, 1, &nValue) != CE_None)
    {
        return 0;
    }

    return nValue;
}

/************************************************************************/
/*                      GetValueAsDouble()                              */
/************************************************************************/

double MMRRasterAttributeTable::GetValueAsDouble(int iRow, int iField) const
{
    // Get ValuesIO do do the work.
    double dfValue = 0.0;
    if (((MMRRasterAttributeTable *)this)
            ->ValuesIO(GF_Read, iField, iRow, 1, &dfValue) != CE_None)
    {
        return 0.0;
    }

    return dfValue;
}

/************************************************************************/
/*                          SetValue()                                  */
/************************************************************************/

void MMRRasterAttributeTable::SetValue(int iRow, int iField,
                                       const char *pszValue)
{
    // Get ValuesIO do do the work.
    ValuesIO(GF_Write, iField, iRow, 1, (char **)&pszValue);
}

/************************************************************************/
/*                          SetValue()                                  */
/************************************************************************/

void MMRRasterAttributeTable::SetValue(int iRow, int iField, double dfValue)
{
    // Get ValuesIO do do the work.
    ValuesIO(GF_Write, iField, iRow, 1, &dfValue);
}

/************************************************************************/
/*                          SetValue()                                  */
/************************************************************************/

void MMRRasterAttributeTable::SetValue(int iRow, int iField, int nValue)
{
    // Get ValuesIO do do the work.
    ValuesIO(GF_Write, iField, iRow, 1, &nValue);
}

/************************************************************************/
/*                          ValuesIO()                                  */
/************************************************************************/

CPLErr MMRRasterAttributeTable::ValuesIO(GDALRWFlag eRWFlag, int iField,
                                         int iStartRow, int iLength,
                                         double *pdfData)
{
    if (eRWFlag == GF_Write && eAccess == GA_ReadOnly)
    {
        CPLError(CE_Failure, CPLE_NoWriteAccess,
                 "Dataset not open in update mode");
        return CE_Failure;
    }

    if (iField < 0 || iField >= static_cast<int>(aoFields.size()))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "iField (%d) out of range.",
                 iField);

        return CE_Failure;
    }

    if (iStartRow < 0 || iLength >= INT_MAX - iStartRow ||
        (iStartRow + iLength) > nRows)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "iStartRow (%d) + iLength(%d) out of range.", iStartRow,
                 iLength);

        return CE_Failure;
    }

    if (aoFields[iField].bConvertColors)
    {
        // Convert to/from float color field.
        int *panColData =
            static_cast<int *>(VSI_MALLOC2_VERBOSE(iLength, sizeof(int)));
        if (panColData == nullptr)
        {
            CPLFree(panColData);
            return CE_Failure;
        }

        if (eRWFlag == GF_Write)
        {
            for (int i = 0; i < iLength; i++)
                panColData[i] = static_cast<int>(pdfData[i]);
        }

        const CPLErr ret =
            ColorsIO(eRWFlag, iField, iStartRow, iLength, panColData);

        if (eRWFlag == GF_Read)
        {
            // Copy them back to doubles.
            for (int i = 0; i < iLength; i++)
                pdfData[i] = panColData[i];
        }

        CPLFree(panColData);
        return ret;
    }

    switch (aoFields[iField].eType)
    {
        case GFT_Integer:
        {
            // Allocate space for ints.
            int *panColData =
                static_cast<int *>(VSI_MALLOC2_VERBOSE(iLength, sizeof(int)));
            if (panColData == nullptr)
            {
                CPLFree(panColData);
                return CE_Failure;
            }

            if (eRWFlag == GF_Write)
            {
                // Copy the application supplied doubles to ints.
                for (int i = 0; i < iLength; i++)
                    panColData[i] = static_cast<int>(pdfData[i]);
            }

            // Do the ValuesIO as ints.
            const CPLErr eVal =
                ValuesIO(eRWFlag, iField, iStartRow, iLength, panColData);
            if (eVal != CE_None)
            {
                CPLFree(panColData);
                return eVal;
            }

            if (eRWFlag == GF_Read)
            {
                // Copy them back to doubles.
                for (int i = 0; i < iLength; i++)
                    pdfData[i] = panColData[i];
            }

            CPLFree(panColData);
        }
        break;
        case GFT_Real:
        {
            if ((eRWFlag == GF_Read) && aoFields[iField].bIsBinValues)
            {
                // Probably could change MMRReadBFUniqueBins to only read needed
                // rows.
                double *padfBinValues = MMRReadBFUniqueBins(
                    aoFields[iField].poColumn, iStartRow + iLength);
                if (padfBinValues == nullptr)
                    return CE_Failure;
                memcpy(pdfData, &padfBinValues[iStartRow],
                       sizeof(double) * iLength);
                CPLFree(padfBinValues);
            }
            else
            {
                if (VSIFSeekL(hMMR->fp,
                              aoFields[iField].nDataOffset +
                                  (static_cast<vsi_l_offset>(iStartRow) *
                                   aoFields[iField].nElementSize),
                              SEEK_SET) != 0)
                {
                    return CE_Failure;
                }

                if (eRWFlag == GF_Read)
                {
                    if (static_cast<int>(VSIFReadL(pdfData, sizeof(double),
                                                   iLength, hMMR->fp)) !=
                        iLength)
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "MMRRasterAttributeTable::ValuesIO: "
                                 "Cannot read values");
                        return CE_Failure;
                    }
#ifdef CPL_MSB
                    GDALSwapWords(pdfData, 8, iLength, 8);
#endif
                }
                else
                {
#ifdef CPL_MSB
                    GDALSwapWords(pdfData, 8, iLength, 8);
#endif
                    // Note: MMRAllocateSpace now called by CreateColumn so
                    // space should exist.
                    if (static_cast<int>(VSIFWriteL(pdfData, sizeof(double),
                                                    iLength, hMMR->fp)) !=
                        iLength)
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "MMRRasterAttributeTable::ValuesIO: "
                                 "Cannot write values");
                        return CE_Failure;
                    }
#ifdef CPL_MSB
                    // Swap back.
                    GDALSwapWords(pdfData, 8, iLength, 8);
#endif
                }
            }
        }
        break;
        case GFT_String:
        {
            // Allocate space for string pointers.
            char **papszColData = static_cast<char **>(
                VSI_MALLOC2_VERBOSE(iLength, sizeof(char *)));
            if (papszColData == nullptr)
            {
                return CE_Failure;
            }

            if (eRWFlag == GF_Write)
            {
                // Copy the application supplied doubles to strings.
                for (int i = 0; i < iLength; i++)
                {
                    osWorkingResult.Printf("%.16g", pdfData[i]);
                    papszColData[i] = CPLStrdup(osWorkingResult);
                }
            }

            // Do the ValuesIO as strings.
            const CPLErr eVal =
                ValuesIO(eRWFlag, iField, iStartRow, iLength, papszColData);
            if (eVal != CE_None)
            {
                if (eRWFlag == GF_Write)
                {
                    for (int i = 0; i < iLength; i++)
                        CPLFree(papszColData[i]);
                }
                CPLFree(papszColData);
                return eVal;
            }

            if (eRWFlag == GF_Read)
            {
                // Copy them back to doubles.
                for (int i = 0; i < iLength; i++)
                    pdfData[i] = CPLAtof(papszColData[i]);
            }

            // Either we allocated them for write, or they were allocated
            // by ValuesIO on read.
            for (int i = 0; i < iLength; i++)
                CPLFree(papszColData[i]);

            CPLFree(papszColData);
        }
        break;
    }

    return CE_None;
}

/************************************************************************/
/*                          ValuesIO()                                  */
/************************************************************************/

CPLErr MMRRasterAttributeTable::ValuesIO(GDALRWFlag eRWFlag, int iField,
                                         int iStartRow, int iLength,
                                         int *pnData)
{
    if (eRWFlag == GF_Write && eAccess == GA_ReadOnly)
    {
        CPLError(CE_Failure, CPLE_NoWriteAccess,
                 "Dataset not open in update mode");
        return CE_Failure;
    }

    if (iField < 0 || iField >= static_cast<int>(aoFields.size()))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "iField (%d) out of range.",
                 iField);

        return CE_Failure;
    }

    if (iStartRow < 0 || iLength >= INT_MAX - iStartRow ||
        (iStartRow + iLength) > nRows)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "iStartRow (%d) + iLength(%d) out of range.", iStartRow,
                 iLength);

        return CE_Failure;
    }

    if (aoFields[iField].bConvertColors)
    {
        // Convert to/from float color field.
        return ColorsIO(eRWFlag, iField, iStartRow, iLength, pnData);
    }

    switch (aoFields[iField].eType)
    {
        case GFT_Integer:
        {
            if (VSIFSeekL(hMMR->fp,
                          aoFields[iField].nDataOffset +
                              (static_cast<vsi_l_offset>(iStartRow) *
                               aoFields[iField].nElementSize),
                          SEEK_SET) != 0)
            {
                return CE_Failure;
            }
            GInt32 *panColData = static_cast<GInt32 *>(
                VSI_MALLOC2_VERBOSE(iLength, sizeof(GInt32)));
            if (panColData == nullptr)
            {
                return CE_Failure;
            }

            if (eRWFlag == GF_Read)
            {
                if (static_cast<int>(VSIFReadL(panColData, sizeof(GInt32),
                                               iLength, hMMR->fp)) != iLength)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "MMRRasterAttributeTable::ValuesIO: "
                             "Cannot read values");
                    CPLFree(panColData);
                    return CE_Failure;
                }
#ifdef CPL_MSB
                GDALSwapWords(panColData, 4, iLength, 4);
#endif
                // Now copy into application buffer. This extra step
                // may not be necessary if sizeof(int) == sizeof(GInt32).
                for (int i = 0; i < iLength; i++)
                    pnData[i] = panColData[i];
            }
            else
            {
                // Copy from application buffer.
                for (int i = 0; i < iLength; i++)
                    panColData[i] = pnData[i];

#ifdef CPL_MSB
                GDALSwapWords(panColData, 4, iLength, 4);
#endif
                // Note: MMRAllocateSpace now called by CreateColumn so space
                // should exist.
                if (static_cast<int>(VSIFWriteL(panColData, sizeof(GInt32),
                                                iLength, hMMR->fp)) != iLength)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "MMRRasterAttributeTable::ValuesIO: "
                             "Cannot write values");
                    CPLFree(panColData);
                    return CE_Failure;
                }
            }
            CPLFree(panColData);
        }
        break;
        case GFT_Real:
        {
            // Allocate space for doubles.
            double *padfColData = static_cast<double *>(
                VSI_MALLOC2_VERBOSE(iLength, sizeof(double)));
            if (padfColData == nullptr)
            {
                return CE_Failure;
            }

            if (eRWFlag == GF_Write)
            {
                // Copy the application supplied ints to doubles.
                for (int i = 0; i < iLength; i++)
                    padfColData[i] = pnData[i];
            }

            // Do the ValuesIO as doubles.
            const CPLErr eVal =
                ValuesIO(eRWFlag, iField, iStartRow, iLength, padfColData);
            if (eVal != CE_None)
            {
                CPLFree(padfColData);
                return eVal;
            }

            if (eRWFlag == GF_Read)
            {
                // Copy them back to ints.
                for (int i = 0; i < iLength; i++)
                    pnData[i] = static_cast<int>(padfColData[i]);
            }

            CPLFree(padfColData);
        }
        break;
        case GFT_String:
        {
            // Allocate space for string pointers.
            char **papszColData = static_cast<char **>(
                VSI_MALLOC2_VERBOSE(iLength, sizeof(char *)));
            if (papszColData == nullptr)
            {
                return CE_Failure;
            }

            if (eRWFlag == GF_Write)
            {
                // Copy the application supplied ints to strings.
                for (int i = 0; i < iLength; i++)
                {
                    osWorkingResult.Printf("%d", pnData[i]);
                    papszColData[i] = CPLStrdup(osWorkingResult);
                }
            }

            // Do the ValuesIO as strings.
            const CPLErr eVal =
                ValuesIO(eRWFlag, iField, iStartRow, iLength, papszColData);
            if (eVal != CE_None)
            {
                if (eRWFlag == GF_Write)
                {
                    for (int i = 0; i < iLength; i++)
                        CPLFree(papszColData[i]);
                }
                CPLFree(papszColData);
                return eVal;
            }

            if (eRWFlag == GF_Read)
            {
                // Copy them back to ints.
                for (int i = 0; i < iLength; i++)
                    pnData[i] = atoi(papszColData[i]);
            }

            // Either we allocated them for write, or they were allocated
            // by ValuesIO on read.
            for (int i = 0; i < iLength; i++)
                CPLFree(papszColData[i]);

            CPLFree(papszColData);
        }
        break;
    }

    return CE_None;
}

/************************************************************************/
/*                          ValuesIO()                                  */
/************************************************************************/

CPLErr MMRRasterAttributeTable::ValuesIO(GDALRWFlag eRWFlag, int iField,
                                         int iStartRow, int iLength,
                                         char **papszStrList)
{
    if (eRWFlag == GF_Write && eAccess == GA_ReadOnly)
    {
        CPLError(CE_Failure, CPLE_NoWriteAccess,
                 "Dataset not open in update mode");
        return CE_Failure;
    }

    if (iField < 0 || iField >= static_cast<int>(aoFields.size()))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "iField (%d) out of range.",
                 iField);

        return CE_Failure;
    }

    if (iStartRow < 0 || iLength >= INT_MAX - iStartRow ||
        (iStartRow + iLength) > nRows)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "iStartRow (%d) + iLength(%d) out of range.", iStartRow,
                 iLength);

        return CE_Failure;
    }

    if (aoFields[iField].bConvertColors)
    {
        // Convert to/from float color field.
        int *panColData =
            static_cast<int *>(VSI_MALLOC2_VERBOSE(iLength, sizeof(int)));
        if (panColData == nullptr)
        {
            CPLFree(panColData);
            return CE_Failure;
        }

        if (eRWFlag == GF_Write)
        {
            for (int i = 0; i < iLength; i++)
                panColData[i] = atoi(papszStrList[i]);
        }

        const CPLErr ret =
            ColorsIO(eRWFlag, iField, iStartRow, iLength, panColData);

        if (eRWFlag == GF_Read)
        {
            // Copy them back to strings.
            for (int i = 0; i < iLength; i++)
            {
                osWorkingResult.Printf("%d", panColData[i]);
                papszStrList[i] = CPLStrdup(osWorkingResult);
            }
        }

        CPLFree(panColData);
        return ret;
    }

    switch (aoFields[iField].eType)
    {
        case GFT_Integer:
        {
            // Allocate space for ints.
            int *panColData =
                static_cast<int *>(VSI_MALLOC2_VERBOSE(iLength, sizeof(int)));
            if (panColData == nullptr)
            {
                return CE_Failure;
            }

            if (eRWFlag == GF_Write)
            {
                // Convert user supplied strings to ints.
                for (int i = 0; i < iLength; i++)
                    panColData[i] = atoi(papszStrList[i]);
            }

            // Call values IO to read/write ints.
            const CPLErr eVal =
                ValuesIO(eRWFlag, iField, iStartRow, iLength, panColData);
            if (eVal != CE_None)
            {
                CPLFree(panColData);
                return eVal;
            }

            if (eRWFlag == GF_Read)
            {
                // Convert ints back to strings.
                for (int i = 0; i < iLength; i++)
                {
                    osWorkingResult.Printf("%d", panColData[i]);
                    papszStrList[i] = CPLStrdup(osWorkingResult);
                }
            }
            CPLFree(panColData);
        }
        break;
        case GFT_Real:
        {
            // Allocate space for doubles.
            double *padfColData = static_cast<double *>(
                VSI_MALLOC2_VERBOSE(iLength, sizeof(double)));
            if (padfColData == nullptr)
            {
                return CE_Failure;
            }

            if (eRWFlag == GF_Write)
            {
                // Convert user supplied strings to doubles.
                for (int i = 0; i < iLength; i++)
                    padfColData[i] = CPLAtof(papszStrList[i]);
            }

            // Call value IO to read/write doubles.
            const CPLErr eVal =
                ValuesIO(eRWFlag, iField, iStartRow, iLength, padfColData);
            if (eVal != CE_None)
            {
                CPLFree(padfColData);
                return eVal;
            }

            if (eRWFlag == GF_Read)
            {
                // Convert doubles back to strings.
                for (int i = 0; i < iLength; i++)
                {
                    osWorkingResult.Printf("%.16g", padfColData[i]);
                    papszStrList[i] = CPLStrdup(osWorkingResult);
                }
            }
            CPLFree(padfColData);
        }
        break;
        case GFT_String:
        {
            if (VSIFSeekL(hMMR->fp,
                          aoFields[iField].nDataOffset +
                              (static_cast<vsi_l_offset>(iStartRow) *
                               aoFields[iField].nElementSize),
                          SEEK_SET) != 0)
            {
                return CE_Failure;
            }
            char *pachColData = static_cast<char *>(
                VSI_MALLOC2_VERBOSE(iLength, aoFields[iField].nElementSize));
            if (pachColData == nullptr)
            {
                return CE_Failure;
            }

            if (eRWFlag == GF_Read)
            {
                if (static_cast<int>(VSIFReadL(pachColData,
                                               aoFields[iField].nElementSize,
                                               iLength, hMMR->fp)) != iLength)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "MMRRasterAttributeTable::ValuesIO: "
                             "Cannot read values");
                    CPLFree(pachColData);
                    return CE_Failure;
                }

                // Now copy into application buffer.
                for (int i = 0; i < iLength; i++)
                {
                    osWorkingResult.assign(
                        pachColData + aoFields[iField].nElementSize * i,
                        aoFields[iField].nElementSize);
                    papszStrList[i] = CPLStrdup(osWorkingResult);
                }
            }
            else
            {
                // We need to check that these strings will fit in the allocated
                // space.
                int nNewMaxChars = aoFields[iField].nElementSize;
                for (int i = 0; i < iLength; i++)
                {
                    const int nStringSize =
                        static_cast<int>(strlen(papszStrList[i])) + 1;
                    if (nStringSize > nNewMaxChars)
                        nNewMaxChars = nStringSize;
                }

                if (nNewMaxChars > aoFields[iField].nElementSize)
                {
                    // OK we have a problem: The allocated space is not big
                    // enough we need to re-allocate the space and update the
                    // pointers and copy across the old data.
                    const int nNewOffset =
                        MMRAllocateSpace(hMMR->papoBand[nBand - 1]->psInfo,
                                         nRows * nNewMaxChars);
                    char *pszBuffer = static_cast<char *>(VSIMalloc2(
                        aoFields[iField].nElementSize, sizeof(char)));
                    for (int i = 0; i < nRows; i++)
                    {
                        // Seek to the old place.
                        CPL_IGNORE_RET_VAL(
                            VSIFSeekL(hMMR->fp,
                                      aoFields[iField].nDataOffset +
                                          (static_cast<vsi_l_offset>(i) *
                                           aoFields[iField].nElementSize),
                                      SEEK_SET));
                        // Read in old data.
                        CPL_IGNORE_RET_VAL(
                            VSIFReadL(pszBuffer, aoFields[iField].nElementSize,
                                      1, hMMR->fp));
                        // Seek to new place.
                        bool bOK = VSIFSeekL(hMMR->fp,
                                             nNewOffset +
                                                 (static_cast<vsi_l_offset>(i) *
                                                  nNewMaxChars),
                                             SEEK_SET) == 0;
                        // Write data to new place.
                        bOK &=
                            VSIFWriteL(pszBuffer, aoFields[iField].nElementSize,
                                       1, hMMR->fp) == 1;
                        // Make sure there is a terminating null byte just to be
                        // safe.
                        const char cNullByte = '\0';
                        bOK &= VSIFWriteL(&cNullByte, sizeof(char), 1,
                                          hMMR->fp) == 1;
                        if (!bOK)
                        {
                            CPLFree(pszBuffer);
                            CPLFree(pachColData);
                            CPLError(CE_Failure, CPLE_AppDefined,
                                     "MMRRasterAttributeTable::ValuesIO: "
                                     "Cannot write values");
                            return CE_Failure;
                        }
                    }
                    // Update our data structures.
                    aoFields[iField].nElementSize = nNewMaxChars;
                    aoFields[iField].nDataOffset = nNewOffset;
                    // Update file.
                    aoFields[iField].poColumn->SetIntField("columnDataPtr",
                                                           nNewOffset);
                    aoFields[iField].poColumn->SetIntField("maxNumChars",
                                                           nNewMaxChars);

                    // Note: There isn't an MMRFreeSpace so we can't un-allocate
                    // the old space in the file.
                    CPLFree(pszBuffer);

                    // Re-allocate our buffer.
                    CPLFree(pachColData);
                    pachColData = static_cast<char *>(
                        VSI_MALLOC2_VERBOSE(iLength, nNewMaxChars));
                    if (pachColData == nullptr)
                    {
                        return CE_Failure;
                    }

                    // Lastly seek to the right place in the new space ready to
                    // write.
                    if (VSIFSeekL(hMMR->fp,
                                  nNewOffset +
                                      (static_cast<vsi_l_offset>(iStartRow) *
                                       nNewMaxChars),
                                  SEEK_SET) != 0)
                    {
                        VSIFree(pachColData);
                        return CE_Failure;
                    }
                }

                // Copy from application buffer.
                for (int i = 0; i < iLength; i++)
                    strcpy(&pachColData[nNewMaxChars * i], papszStrList[i]);

                // Note: MMRAllocateSpace now called by CreateColumn so space
                // should exist.
                if (static_cast<int>(VSIFWriteL(pachColData,
                                                aoFields[iField].nElementSize,
                                                iLength, hMMR->fp)) != iLength)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "MMRRasterAttributeTable::ValuesIO: "
                             "Cannot write values");
                    CPLFree(pachColData);
                    return CE_Failure;
                }
            }
            CPLFree(pachColData);
        }
        break;
    }

    return CE_None;
}

/************************************************************************/
/*                               ColorsIO()                              */
/************************************************************************/

// Handle the fact that MMR stores colours as floats, but we need to
// read them in as ints 0...255.
CPLErr MMRRasterAttributeTable::ColorsIO(GDALRWFlag eRWFlag, int iField,
                                         int iStartRow, int iLength,
                                         int *pnData)
{
    // Allocate space for doubles.
    double *padfData =
        static_cast<double *>(VSI_MALLOC2_VERBOSE(iLength, sizeof(double)));
    if (padfData == nullptr)
    {
        return CE_Failure;
    }

    if (eRWFlag == GF_Write)
    {
        // Copy the application supplied ints to doubles
        // and convert 0..255 to 0..1 in the same manner
        // as the color table.
        for (int i = 0; i < iLength; i++)
            padfData[i] = pnData[i] / 255.0;
    }

    if (VSIFSeekL(hMMR->fp,
                  aoFields[iField].nDataOffset +
                      (static_cast<vsi_l_offset>(iStartRow) *
                       aoFields[iField].nElementSize),
                  SEEK_SET) != 0)
    {
        CPLFree(padfData);
        return CE_Failure;
    }

    if (eRWFlag == GF_Read)
    {
        if (static_cast<int>(VSIFReadL(padfData, sizeof(double), iLength,
                                       hMMR->fp)) != iLength)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "MMRRasterAttributeTable::ColorsIO: Cannot read values");
            CPLFree(padfData);
            return CE_Failure;
        }
#ifdef CPL_MSB
        GDALSwapWords(padfData, 8, iLength, 8);
#endif
    }
    else
    {
#ifdef CPL_MSB
        GDALSwapWords(padfData, 8, iLength, 8);
#endif
        // Note: MMRAllocateSpace now called by CreateColumn so space should
        // exist.
        if (static_cast<int>(VSIFWriteL(padfData, sizeof(double), iLength,
                                        hMMR->fp)) != iLength)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "MMRRasterAttributeTable::ColorsIO: Cannot write values");
            CPLFree(padfData);
            return CE_Failure;
        }
    }

    if (eRWFlag == GF_Read)
    {
        // Copy them back to ints converting 0..1 to 0..255 in
        // the same manner as the color table.
        // TODO(schwehr): Symbolic constants for 255 and 256.
        for (int i = 0; i < iLength; i++)
            pnData[i] = std::min(255, static_cast<int>(padfData[i] * 256));
    }

    CPLFree(padfData);

    return CE_None;
}

/************************************************************************/
/*                       ChangesAreWrittenToFile()                      */
/************************************************************************/

int MMRRasterAttributeTable::ChangesAreWrittenToFile()
{
    return TRUE;
}

/************************************************************************/
/*                          SetRowCount()                               */
/************************************************************************/

void MMRRasterAttributeTable::SetRowCount(int iCount)
{
    if (eAccess == GA_ReadOnly)
    {
        CPLError(CE_Failure, CPLE_NoWriteAccess,
                 "Dataset not open in update mode");
        return;
    }

    if (iCount > nRows)
    {
        // Making the RAT larger - a bit hard.
        // We need to re-allocate space on disc.
        for (int iCol = 0; iCol < static_cast<int>(aoFields.size()); iCol++)
        {
            // New space.
            const int nNewOffset =
                MMRAllocateSpace(hMMR->papoBand[nBand - 1]->psInfo,
                                 iCount * aoFields[iCol].nElementSize);

            // Only need to bother if there are actually rows.
            if (nRows > 0)
            {
                // Temp buffer for this column.
                void *pData =
                    VSI_MALLOC2_VERBOSE(nRows, aoFields[iCol].nElementSize);
                if (pData == nullptr)
                {
                    return;
                }
                // Read old data.
                if (VSIFSeekL(hMMR->fp, aoFields[iCol].nDataOffset, SEEK_SET) !=
                        0 ||
                    static_cast<int>(VSIFReadL(pData,
                                               aoFields[iCol].nElementSize,
                                               nRows, hMMR->fp)) != nRows)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "MMRRasterAttributeTable::SetRowCount: "
                             "Cannot read values");
                    CPLFree(pData);
                    return;
                }

                // Write data - new space will be uninitialised.
                if (VSIFSeekL(hMMR->fp, nNewOffset, SEEK_SET) != 0 ||
                    static_cast<int>(VSIFWriteL(pData,
                                                aoFields[iCol].nElementSize,
                                                nRows, hMMR->fp)) != nRows)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "MMRRasterAttributeTable::SetRowCount: "
                             "Cannot write values");
                    CPLFree(pData);
                    return;
                }
                CPLFree(pData);
            }

            // Update our data structures.
            aoFields[iCol].nDataOffset = nNewOffset;
            // Update file.
            aoFields[iCol].poColumn->SetIntField("columnDataPtr", nNewOffset);
            aoFields[iCol].poColumn->SetIntField("numRows", iCount);
        }
    }
    else if (iCount < nRows)
    {
        // Update the numRows.
        for (int iCol = 0; iCol < static_cast<int>(aoFields.size()); iCol++)
        {
            aoFields[iCol].poColumn->SetIntField("numRows", iCount);
        }
    }

    nRows = iCount;

    if (poDT != nullptr && EQUAL(poDT->GetType(), "Edsc_Table"))
    {
        poDT->SetIntField("numrows", iCount);
    }
}

/************************************************************************/
/*                          GetRowOfValue()                             */
/************************************************************************/
int MMRRasterAttributeTable::GetRowOfValue(double dfValue) const
{
    // Handle case of regular binning.
    if (bLinearBinning)
    {
        const int iBin =
            static_cast<int>(floor((dfValue - dfRow0Min) / dfBinSize));
        if (iBin < 0 || iBin >= nRows)
            return -1;
        return iBin;
    }
    // Do we have any information?
    int nMinCol = GetColOfUsage(GFU_Min);
    if (nMinCol == -1)
        nMinCol = GetColOfUsage(GFU_MinMax);
    int nMaxCol = GetColOfUsage(GFU_Max);
    if (nMaxCol == -1)
        nMaxCol = GetColOfUsage(GFU_MinMax);
    if (nMinCol == -1 && nMaxCol == -1)
        return -1;
    // Search through rows for match.
    for (int iRow = 0; iRow < nRows; iRow++)
    {
        if (nMinCol != -1)
        {
            while (iRow < nRows && dfValue < GetValueAsDouble(iRow, nMinCol))
                iRow++;
            if (iRow == nRows)
                break;
        }
        if (nMaxCol != -1)
        {
            if (dfValue > GetValueAsDouble(iRow, nMaxCol))
                continue;
        }
        return iRow;
    }
    return -1;
}

/************************************************************************/
/*                          GetRowOfValue()                             */
/*                                                                      */
/*      Int arg for now just converted to double.  Perhaps we will      */
/*      handle this in a special way some day?                          */
/************************************************************************/
int MMRRasterAttributeTable::GetRowOfValue(int nValue) const
{
    return GetRowOfValue(static_cast<double>(nValue));
}

/************************************************************************/
/*                          CreateColumn()                              */
/************************************************************************/

CPLErr MMRRasterAttributeTable::CreateColumn(const char *pszFieldName,
                                             GDALRATFieldType eFieldType,
                                             GDALRATFieldUsage eFieldUsage)
{
    if (eAccess == GA_ReadOnly)
    {
        CPLError(CE_Failure, CPLE_NoWriteAccess,
                 "Dataset not open in update mode");
        return CE_Failure;
    }

    // Do we have a descriptor table already?
    if (poDT == nullptr || !EQUAL(poDT->GetType(), "Edsc_Table"))
        CreateDT();

    bool bConvertColors = false;

    // Imagine doesn't have a concept of usage - works of the names instead.
    // Must make sure name matches use.
    if (eFieldUsage == GFU_Red)
    {
        pszFieldName = "Red";
        // Create a real column in the file, but make it
        // available as int to GDAL.
        bConvertColors = true;
        eFieldType = GFT_Real;
    }
    else if (eFieldUsage == GFU_Green)
    {
        pszFieldName = "Green";
        bConvertColors = true;
        eFieldType = GFT_Real;
    }
    else if (eFieldUsage == GFU_Blue)
    {
        pszFieldName = "Blue";
        bConvertColors = true;
        eFieldType = GFT_Real;
    }
    else if (eFieldUsage == GFU_Alpha)
    {
        pszFieldName = "Opacity";
        bConvertColors = true;
        eFieldType = GFT_Real;
    }
    else if (eFieldUsage == GFU_PixelCount)
    {
        pszFieldName = "Histogram";
        // Histogram is always float in MMR.
        eFieldType = GFT_Real;
    }
    else if (eFieldUsage == GFU_Name)
    {
        pszFieldName = "Class_Names";
    }

    // Check to see if a column with pszFieldName exists and create it
    // if necessary.
    MMREntry *poColumn = poDT->GetNamedChild(pszFieldName);

    if (poColumn == nullptr || !EQUAL(poColumn->GetType(), "Edsc_Column"))
        poColumn = MMREntry::New(hMMR->papoBand[nBand - 1]->psInfo,
                                 pszFieldName, "Edsc_Column", poDT);

    poColumn->SetIntField("numRows", nRows);
    int nElementSize = 0;

    if (eFieldType == GFT_Integer)
    {
        nElementSize = sizeof(GInt32);
        poColumn->SetStringField("dataType", "integer");
    }
    else if (eFieldType == GFT_Real)
    {
        nElementSize = sizeof(double);
        poColumn->SetStringField("dataType", "real");
    }
    else if (eFieldType == GFT_String)
    {
        // Just have to guess here since we don't have any strings to check.
        nElementSize = 10;
        poColumn->SetStringField("dataType", "string");
        poColumn->SetIntField("maxNumChars", nElementSize);
    }
    else
    {
        // Cannot deal with any of the others yet.
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Writing this data type in a column is not supported "
                 "for this Raster Attribute Table.");
        return CE_Failure;
    }

    const int nOffset = MMRAllocateSpace(hMMR->papoBand[nBand - 1]->psInfo,
                                         nRows * nElementSize);
    poColumn->SetIntField("columnDataPtr", nOffset);

    if (bConvertColors)
    {
        // GDAL Int column
        eFieldType = GFT_Integer;
    }

    AddColumn(pszFieldName, eFieldType, eFieldUsage, nOffset, nElementSize,
              poColumn, false, bConvertColors);

    return CE_None;
}

/************************************************************************/
/*                          SetLinearBinning()                          */
/************************************************************************/

CPLErr MMRRasterAttributeTable::SetLinearBinning(double dfRow0MinIn,
                                                 double dfBinSizeIn)
{
    if (eAccess == GA_ReadOnly)
    {
        CPLError(CE_Failure, CPLE_NoWriteAccess,
                 "Dataset not open in update mode");
        return CE_Failure;
    }

    bLinearBinning = true;
    dfRow0Min = dfRow0MinIn;
    dfBinSize = dfBinSizeIn;

    // Do we have a descriptor table already?
    if (poDT == nullptr || !EQUAL(poDT->GetType(), "Edsc_Table"))
        CreateDT();

    // We should have an Edsc_BinFunction.
    MMREntry *poBinFunction = poDT->GetNamedChild("#Bin_Function#");
    if (poBinFunction == nullptr ||
        !EQUAL(poBinFunction->GetType(), "Edsc_BinFunction"))
    {
        poBinFunction =
            MMREntry::New(hMMR->papoBand[nBand - 1]->psInfo, "#Bin_Function#",
                          "Edsc_BinFunction", poDT);
    }

    // Because of the BaseData we have to hardcode the size.
    poBinFunction->MakeData(30);

    poBinFunction->SetStringField("binFunction", "direct");
    poBinFunction->SetDoubleField("minLimit", dfRow0Min);
    poBinFunction->SetDoubleField("maxLimit",
                                  (nRows - 1) * dfBinSize + dfRow0Min);
    poBinFunction->SetIntField("numBins", nRows);

    return CE_None;
}

/************************************************************************/
/*                          GetLinearBinning()                          */
/************************************************************************/

int MMRRasterAttributeTable::GetLinearBinning(double *pdfRow0Min,
                                              double *pdfBinSize) const
{
    if (!bLinearBinning)
        return FALSE;

    *pdfRow0Min = dfRow0Min;
    *pdfBinSize = dfBinSize;

    return TRUE;
}

/************************************************************************/
/*                              Serialize()                             */
/************************************************************************/

CPLXMLNode *MMRRasterAttributeTable::Serialize() const
{
    if (GetRowCount() != 0 &&
        GetColumnCount() > RAT_MAX_ELEM_FOR_CLONE / GetRowCount())
        return nullptr;

    return GDALRasterAttributeTable::Serialize();
}

/************************************************************************/
/*                              SetTableType()                             */
/************************************************************************/

CPLErr
MMRRasterAttributeTable::SetTableType(const GDALRATTableType eInTableType)
{
    eTableType = eInTableType;
    return CE_None;
}

/************************************************************************/
/*                              GetTableType()                             */
/************************************************************************/

GDALRATTableType MMRRasterAttributeTable::GetTableType() const
{
    return eTableType;
}

void MMRRasterAttributeTable::RemoveStatistics()
{
    // since we are storing the fields in a vector it will generally
    // be faster to create a new vector and replace the old one
    // rather than actually erasing columns.
    std::vector<MMRAttributeField> aoNewFields;
    for (const auto &field : aoFields)
    {
        switch (field.eUsage)
        {
            case GFU_PixelCount:
            case GFU_Min:
            case GFU_Max:
            case GFU_RedMin:
            case GFU_GreenMin:
            case GFU_BlueMin:
            case GFU_AlphaMin:
            case GFU_RedMax:
            case GFU_GreenMax:
            case GFU_BlueMax:
            case GFU_AlphaMax:
            {
                break;
            }

            default:
                if (field.sName != "Histogram")
                {
                    aoNewFields.push_back(field);
                }
        }
    }
    aoFields = std::move(aoNewFields);
}
