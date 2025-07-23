/******************************************************************************
 *
 * Project:  MiraMonRaster driver
 * Purpose:  Implements MMRRasterband class.
 * Author:   Abel Pau
 * 
 ******************************************************************************
 * Copyright (c) 2025, Xavier Pons
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

//#include "cpl_port.h"
#include "miramon_dataset.h"
#include "miramon_rasterband.h"
#include "miramon_band.h"  // Per a MMRBand

#include "../miramon_common/mm_gdal_functions.h"  // For MMCheck_REL_FILE()

/************************************************************************/
/*                           MMRRasterBand()                            */
/************************************************************************/
MMRRasterBand::MMRRasterBand(MMRDataset *poDSIn, int nBandIn)
    : pfRel(poDSIn->GetRel())
{
    poDS = poDSIn;
    nBand = nBandIn;

    eAccess = poDSIn->GetAccess();

    MMRBand *poBand = pfRel->GetLastBand();
    if (poBand == nullptr)
        return;

    // Getting some band info
    osBandSection = poBand->GetBandSection();
    eMMRDataTypeMiraMon = poBand->GeteMMDataType();
    eMMBytesPerPixel = poBand->GeteMMBytesPerPixel();
    nBlockXSize = poBand->GetBlockXSize();
    nBlockYSize = poBand->GetBlockYSize();

    SetDataType();

    // We have a valid RasterBand.
    SetIsValid(true);
}

/************************************************************************/
/*                           ~MMRRasterBand()                           */
/************************************************************************/

MMRRasterBand::~MMRRasterBand()

{
    FlushCache(true);

    delete Palette;

    if (poDefaultRAT != nullptr)
        delete poDefaultRAT;
}

/************************************************************************/
/*                             SetDataType()                         */
/************************************************************************/
void MMRRasterBand::SetDataType()
{
    switch (eMMRDataTypeMiraMon)
    {
        case MMDataType::DATATYPE_AND_COMPR_BIT:
        case MMDataType::DATATYPE_AND_COMPR_BYTE:
        case MMDataType::DATATYPE_AND_COMPR_BYTE_RLE:
            eDataType = GDT_Byte;
            break;

        case MMDataType::DATATYPE_AND_COMPR_UINTEGER:
        case MMDataType::DATATYPE_AND_COMPR_UINTEGER_RLE:
            eDataType = GDT_UInt16;
            break;

        case MMDataType::DATATYPE_AND_COMPR_INTEGER:
        case MMDataType::DATATYPE_AND_COMPR_INTEGER_RLE:
        case MMDataType::DATATYPE_AND_COMPR_INTEGER_ASCII:
            eDataType = GDT_Int16;
            break;

        case MMDataType::DATATYPE_AND_COMPR_LONG:
        case MMDataType::DATATYPE_AND_COMPR_LONG_RLE:
            eDataType = GDT_Int32;
            break;

        case MMDataType::DATATYPE_AND_COMPR_REAL:
        case MMDataType::DATATYPE_AND_COMPR_REAL_RLE:
        case MMDataType::DATATYPE_AND_COMPR_REAL_ASCII:
            eDataType = GDT_Float32;
            break;

        case MMDataType::DATATYPE_AND_COMPR_DOUBLE:
        case MMDataType::DATATYPE_AND_COMPR_DOUBLE_RLE:
            eDataType = GDT_Float64;
            break;

        default:
            eDataType = GDT_Byte;
            // This should really report an error, but this isn't
            // so easy from within constructors.
            CPLDebug("GDAL", "Unsupported pixel type in MMRRasterBand: %d.",
                     static_cast<int>(eMMRDataTypeMiraMon));
            break;
    }
}

/************************************************************************/
/*                             GetNoDataValue()                         */
/************************************************************************/

double MMRRasterBand::GetNoDataValue(int *pbSuccess)

{
    double dfNoData = 0.0;

    MMRBand *poBand = pfRel->GetLastBand();
    if (!poBand)
    {
        CPLAssert(false);
        return dfNoData;
    }

    if (!poBand->BandHasNoData())
    {
        if (pbSuccess)
            *pbSuccess = FALSE;
        return dfNoData;
    }

    if (pbSuccess)
        *pbSuccess = TRUE;
    return poBand->GetNoDataValue();
}

/************************************************************************/
/*                             GetMinimum()                             */
/************************************************************************/

double MMRRasterBand::GetMinimum(int *pbSuccess)

{
    const char *pszValue = GetMetadataItem("STATISTICS_MINIMUM");

    if (pszValue != nullptr)
    {
        if (pbSuccess)
            *pbSuccess = TRUE;
        return CPLAtofM(pszValue);
    }

    return GDALRasterBand::GetMinimum(pbSuccess);
}

/************************************************************************/
/*                             GetMaximum()                             */
/************************************************************************/

double MMRRasterBand::GetMaximum(int *pbSuccess)

{
    const char *pszValue = GetMetadataItem("STATISTICS_MAXIMUM");

    if (pszValue != nullptr)
    {
        if (pbSuccess)
            *pbSuccess = TRUE;
        return CPLAtofM(pszValue);
    }

    return GDALRasterBand::GetMaximum(pbSuccess);
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr MMRRasterBand::IReadBlock(int nBlockXOff, int nBlockYOff, void *pImage)

{
    CPLErr eErr = CE_None;

    if (nBand < 1 || nBand > pfRel->GetNBands())
        return CE_Failure;

    MMRBand *pBand = pfRel->GetBand(nBand - 1);
    if (!pBand)
        return CE_Failure;
    eErr = pBand->GetRasterBlock(nBlockXOff, nBlockYOff, pImage,
                                 nBlockXSize * nBlockYSize *
                                     GDALGetDataTypeSizeBytes(eDataType));

    if (eErr == CE_None &&
        eMMRDataTypeMiraMon == MMDataType::DATATYPE_AND_COMPR_BIT)
    {
        GByte *pabyData = static_cast<GByte *>(pImage);

        for (int nIAcumulated = nBlockXSize * nBlockYSize - 1;
             nIAcumulated >= 0; nIAcumulated--)
        {
            if ((pabyData[nIAcumulated >> 3] & (1 << (nIAcumulated & 0x7))))
                pabyData[nIAcumulated] = 1;
            else
                pabyData[nIAcumulated] = 0;
        }
    }

    return eErr;
}

/************************************************************************/
/*                         GetDescription()                             */
/************************************************************************/

const char *MMRRasterBand::GetDescription() const
{
    MMRBand *pBand = pfRel->GetBand(nBand - 1);
    if (!pBand)
        return "";

    return pBand->GetBandName();
}

/************************************************************************/
/*                       GetColorInterpretation()                       */
/************************************************************************/

GDALColorInterp MMRRasterBand::GetColorInterpretation()

{
    if (poCT != nullptr)
        return GCI_PaletteIndex;

    return GCI_Undefined;
}

/************************************************************************/
/*                           GetColorTable()                            */
/************************************************************************/

GDALColorTable *MMRRasterBand::GetColorTable()
{
    if (bTriedLoadColorTable)
        return poCT;

    bTriedLoadColorTable = true;

    Palette = new MMRPalettes(*pfRel, osBandSection);
    poCT = new GDALColorTable();

    /*
    * GDALPaletteInterp
    */

    if (CE_None != UpdateTableColorsFromPalette())
    {
        // No color table available. Perhaps some attribute table with the colors?
        delete poCT;
        poCT = nullptr;
        return poCT;
    }

    ConvertColorsFromPaletteToColorTable();

    return poCT;
}

/************************************************************************/
/*                            SetMetadata()                             */
/************************************************************************/

CPLErr MMRRasterBand::SetMetadata(char **papszMDIn, const char *pszDomain)

{
    bMetadataDirty = true;

    return GDALPamRasterBand::SetMetadata(papszMDIn, pszDomain);
}

/************************************************************************/
/*                        SetMetadataItem()                             */
/************************************************************************/

CPLErr MMRRasterBand::SetMetadataItem(const char *pszTag, const char *pszValue,
                                      const char *pszDomain)

{
    bMetadataDirty = true;

    return GDALPamRasterBand::SetMetadataItem(pszTag, pszValue, pszDomain);
}

/************************************************************************/
/*                           GetDefaultRAT()                            */
/************************************************************************/

GDALRasterAttributeTable *MMRRasterBand::GetDefaultRAT()

{
    if (poDefaultRAT != nullptr)
        return poDefaultRAT;

    poDefaultRAT = new GDALDefaultRasterAttributeTable();

    FillRATFromDBF();

    return poDefaultRAT;
}

CPLErr MMRRasterBand::FillRATFromDBF()

{
    CPLString os_IndexJoin = pfRel->GetMetadataValue(
        SECTION_ATTRIBUTE_DATA, osBandSection, "IndexsJoinTaula");

    if (os_IndexJoin.empty())
    {
        if (CE_None != UpdateAttributeColorsFromPalette())
        {
            delete poDefaultRAT;
            poDefaultRAT = nullptr;
            return CE_Failure;
        }

        return CE_None;
    }

    char **papszTokens = CSLTokenizeString2(os_IndexJoin, ",", 0);
    const int nTokens = CSLCount(papszTokens);

    if (nTokens < 1)
    {
        CSLDestroy(papszTokens);
        delete poDefaultRAT;
        poDefaultRAT = nullptr;
        return CE_Failure;
    }

    // TODO: només una de les taules
    for (int nIAttTable = 0; nIAttTable < nTokens; nIAttTable++)
    {
        CPLString osRELName, osDBFName, osAssociateRel;
        if (CE_None != GetAttributeTableName(papszTokens[nIAttTable], osRELName,
                                             osDBFName, osAssociateRel))
        {
            CSLDestroy(papszTokens);
            delete poDefaultRAT;
            poDefaultRAT = nullptr;
            return CE_Failure;
        }

        if (osDBFName.empty())
        {
            CSLDestroy(papszTokens);
            delete poDefaultRAT;
            poDefaultRAT = nullptr;
            return CE_Failure;
        }

        if (osAssociateRel.empty())
        {
            CSLDestroy(papszTokens);
            delete poDefaultRAT;
            poDefaultRAT = nullptr;
            return CE_Failure;
        }

        if (CE_None !=
            CreateAttributteTableFromDBF(osRELName, osDBFName, osAssociateRel))
        {
            CSLDestroy(papszTokens);
            delete poDefaultRAT;
            poDefaultRAT = nullptr;
            return CE_Failure;
        }
    }
    CSLDestroy(papszTokens);
    return CE_None;
}

CPLErr MMRRasterBand::UpdateAttributeColorsFromPalette()

{
    if (!Palette)
        return CE_None;

    CPLString os_Color_TractamentVariable = pfRel->GetMetadataValue(
        SECTION_COLOR_TEXT, osBandSection, "Color_TractamentVariable");

    CPLString os_Color_Const = pfRel->GetMetadataValue(
        SECTION_COLOR_TEXT, osBandSection, "Color_Const");

    if (os_Color_Const == "1")
        return AssignUniformColorTable();

    if (EQUAL(os_Color_TractamentVariable, "Categoric"))
        Palette->SetIsCategorical(true);
    else
        Palette->SetIsCategorical(false);

    CPLErr peErr;
    if (Palette->IsCategorical())
        peErr = FromPaletteToAttributeTableCategoricalMode();
    else
        peErr = FromPaletteToAttributeTableContinousMode();

    if (peErr != CE_None)
        return peErr;

    return CE_None;
}

CPLErr MMRRasterBand::CreateAttributteTableFromDBF(CPLString osRELName,
                                                   CPLString osDBFName,
                                                   CPLString osAssociateRel)
{
    struct MM_DATA_BASE_XP oAttributteTable;
    memset(&oAttributteTable, 0, sizeof(oAttributteTable));

    if (!osRELName.empty())
    {
        if (MM_ReadExtendedDBFHeaderFromFile(
                osDBFName.c_str(), &oAttributteTable,
                static_cast<const char *>(osRELName)))
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Error reading attribute table \"%s\".",
                     osDBFName.c_str());
            return CE_None;
        }
    }
    else
    {
        if (MM_ReadExtendedDBFHeaderFromFile(osDBFName.c_str(),
                                             &oAttributteTable, nullptr))
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Error reading attribute table \"%s\".",
                     osDBFName.c_str());
            return CE_None;
        }
    }

    MM_EXT_DBF_N_FIELDS nIField;
    MM_EXT_DBF_N_FIELDS nFieldIndex = oAttributteTable.nFields;
    MM_EXT_DBF_N_FIELDS nCategIndex = oAttributteTable.nFields;
    for (nIField = 0; nIField < oAttributteTable.nFields; nIField++)
    {
        if (EQUAL(oAttributteTable.pField[nIField].FieldName, osAssociateRel))
        {
            nFieldIndex = nIField;
            if (nIField + 1 < oAttributteTable.nFields)
                nCategIndex = nIField + 1;
            else if (nIField > 1)
                nCategIndex = nIField - 1;
            break;
        }
    }

    if (nFieldIndex == oAttributteTable.nFields)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid attribute table: \"%s\"",
                 oAttributteTable.szFileName);
        return CE_Failure;
    }

    if (oAttributteTable.pField[nFieldIndex].FieldType != 'N')
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid attribute table field: \"%s\"",
                 oAttributteTable.szFileName);
        return CE_Failure;
    }

    // 0 column: category value
    if (oAttributteTable.pField[nFieldIndex].DecimalsIfFloat)
    {
        if (CE_None != poDefaultRAT->CreateColumn(
                           oAttributteTable.pField[nFieldIndex].FieldName,
                           GFT_Real, GFU_MinMax))
            return CE_Failure;
    }
    else
    {
        if (CE_None != poDefaultRAT->CreateColumn(
                           oAttributteTable.pField[nFieldIndex].FieldName,
                           GFT_Integer, GFU_MinMax))
            return CE_Failure;
    }

    GDALRATFieldUsage eFieldUsage;
    GDALRATFieldType eFieldType;
    for (nIField = 0; nIField < oAttributteTable.nFields; nIField++)
    {
        if (nIField == nFieldIndex)
            continue;

        if (oAttributteTable.pField[nIField].FieldType == 'N')
        {
            eFieldUsage = GFU_MinMax;
            if (oAttributteTable.pField[nIField].DecimalsIfFloat)
                eFieldType = GFT_Real;
            else
                eFieldType = GFT_Integer;
        }
        else
        {
            eFieldUsage = GFU_Generic;
            eFieldType = GFT_String;
        }
        if (nIField == nCategIndex)
            eFieldUsage = GFU_Name;

        /*if(*oAttributteTable.pField[nIField].FieldDescription[0] != '\0')
        {
            if (CE_None != poDefaultRAT->CreateColumn(
                oAttributteTable.pField[nIField].FieldDescription[0],
                eFieldType, eFieldUsage))
                return CE_Failure;
        }
        else
        */
        {
            if (CE_None != poDefaultRAT->CreateColumn(
                               oAttributteTable.pField[nIField].FieldName,
                               eFieldType, eFieldUsage))
                return CE_Failure;
        }
    }

    VSIFSeekL(oAttributteTable.pfDataBase, oAttributteTable.FirstRecordOffset,
              SEEK_SET);
    poDefaultRAT->SetRowCount(static_cast<int>(oAttributteTable.nRecords));

    MM_ACCUMULATED_BYTES_TYPE_DBF nBufferSize =
        oAttributteTable.BytesPerRecord + 1;
    char *pzsRecord = static_cast<char *>(VSI_CALLOC_VERBOSE(1, nBufferSize));
    char *pzsField = static_cast<char *>(VSI_CALLOC_VERBOSE(1, nBufferSize));

    for (int nIRecord = 0;
         nIRecord < static_cast<int>(oAttributteTable.nRecords); nIRecord++)
    {
        if (oAttributteTable.BytesPerRecord !=
            VSIFReadL(pzsRecord, sizeof(unsigned char),
                      oAttributteTable.BytesPerRecord,
                      oAttributteTable.pfDataBase))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Invalid attribute table: \"%s\"", osDBFName.c_str());
            return CE_Failure;
        }

        // Category index
        memcpy(pzsField,
               pzsRecord +
                   oAttributteTable.pField[nFieldIndex].AccumulatedBytes,
               oAttributteTable.pField[nFieldIndex].BytesPerField);
        pzsField[oAttributteTable.pField[nFieldIndex].BytesPerField] = '\0';
        CPLString osField = pzsField;
        poDefaultRAT->SetValue(nIRecord, 0, osField);

        int nIOrderedField = 1;
        for (nIField = 0; nIField < oAttributteTable.nFields; nIField++)
        {
            if (nIField == nFieldIndex)
                continue;

            // Category value
            memcpy(pzsField,
                   pzsRecord +
                       oAttributteTable.pField[nIField].AccumulatedBytes,
                   oAttributteTable.pField[nIField].BytesPerField);
            pzsField[oAttributteTable.pField[nIField].BytesPerField] = '\0';
            if (oAttributteTable.CharSet == MM_JOC_CARAC_OEM850_DBASE)
                MM_oemansi(pzsField);

            if (oAttributteTable.CharSet != MM_JOC_CARAC_UTF8_DBF)
            {
                // MiraMon encoding is ISO 8859-1 (Latin1) -> Recode to UTF-8
                char *pszFieldRecoded =
                    CPLRecode(pzsField, CPL_ENC_ISO8859_1, CPL_ENC_UTF8);
                poDefaultRAT->SetValue(nIRecord, nIOrderedField,
                                       pszFieldRecoded);
                CPLFree(pszFieldRecoded);
            }
            else
                poDefaultRAT->SetValue(nIRecord, nIOrderedField, pzsField);

            nIOrderedField++;
        }
    }

    VSIFree(pzsField);
    VSIFree(pzsRecord);
    VSIFCloseL(oAttributteTable.pfDataBase);
    MM_ReleaseMainFields(&oAttributteTable);

    return CE_None;
}

CPLErr MMRRasterBand::UpdateTableColorsFromPalette()

{
    CPLString os_Color_Const = pfRel->GetMetadataValue(
        SECTION_COLOR_TEXT, osBandSection, "Color_Const");

    if (os_Color_Const == "1")
        return AssignUniformColorTable();

    CPLString os_Color_TractamentVariable = pfRel->GetMetadataValue(
        SECTION_COLOR_TEXT, osBandSection, "Color_TractamentVariable");

    if (EQUAL(os_Color_TractamentVariable, "Categoric"))
        Palette->SetIsCategorical(true);
    else
        Palette->SetIsCategorical(false);

    CPLString os_Color_Paleta = pfRel->GetMetadataValue(
        SECTION_COLOR_TEXT, osBandSection, "Color_Paleta");

    if (os_Color_Paleta.empty() || os_Color_Paleta == "<Automatic>")
        return CE_Failure;

    if (!Palette)
        return CE_Failure;

    CPLErr peErr;
    if (Palette->IsCategorical())
        peErr = FromPaletteToColorTableCategoricalMode();
    else
        peErr = FromPaletteToColorTableContinousMode();

    if (peErr != CE_None)
        return peErr;

    return CE_None;
}

CPLErr MMRRasterBand::AssignUniformColorTable()

{
    MMRBand *poBand = pfRel->GetLastBand();
    if (!poBand)
        return CE_Failure;

    Palette->SetIsConstantColor(true);

    // Example: Color_Smb=(255,0,255)
    CPLString os_Color_Smb =
        pfRel->GetMetadataValue(SECTION_COLOR_TEXT, osBandSection, "Color_Smb");
    os_Color_Smb.replaceAll(" ", "");
    if (!os_Color_Smb.empty() && os_Color_Smb.size() >= 7 &&
        os_Color_Smb[0] == '(' && os_Color_Smb[os_Color_Smb.size() - 1] == ')')
    {
        os_Color_Smb.replaceAll("(", "");
        os_Color_Smb.replaceAll(")", "");
        char **papszTokens = CSLTokenizeString2(os_Color_Smb, ",", 0);
        if (CSLCount(papszTokens) != 3)
        {
            CSLDestroy(papszTokens);
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Invalid constant color: \"%s\"", pfRel->GetRELNameChar());
            return CE_Failure;
        }
        Palette->SetConstantColorRGB(static_cast<short>(atoi(papszTokens[0])),
                                     static_cast<short>(atoi(papszTokens[1])),
                                     static_cast<short>(atoi(papszTokens[2])));

        CSLDestroy(papszTokens);
    }

    // Only for 1 or 2 bytes images
    if (eMMBytesPerPixel != MMBytesPerPixel::TYPE_BYTES_PER_PIXEL_BYTE_I_RLE &&
        eMMBytesPerPixel != MMBytesPerPixel::TYPE_BYTES_PER_PIXEL_INTEGER_I_RLE)
    {
        return CE_None;
    }

    int nNPossibleValues = static_cast<int>(
        pow(2, static_cast<double>(8) * static_cast<int>(eMMBytesPerPixel)));
    for (int iColumn = 0; iColumn < 4; iColumn++)
    {
        try
        {
            aadfPCT[iColumn].resize(nNPossibleValues);
        }
        catch (std::bad_alloc &e)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "%s", e.what());
            return CE_Failure;
        }
    }

    for (int nITableColor = 0; nITableColor < nNPossibleValues; nITableColor++)
    {
        if (poBand->BandHasNoData() && nITableColor == poBand->GetNoDataValue())
        {
            aadfPCT[0][nITableColor] = 0;
            aadfPCT[1][nITableColor] = 0;
            aadfPCT[2][nITableColor] = 0;
            aadfPCT[3][nITableColor] = 0;
        }
        else
        {
            // Before the minimum, we apply the color of the first
            // element (as a placeholder).
            aadfPCT[0][nITableColor] = Palette->GetConstantColorRGB().c1;
            aadfPCT[1][nITableColor] = Palette->GetConstantColorRGB().c2;
            aadfPCT[2][nITableColor] = Palette->GetConstantColorRGB().c3;
            aadfPCT[3][nITableColor] = 255;
        }
    }

    return CE_None;
}

// Converts pallete Colors to Colors of pixels
CPLErr MMRRasterBand::FromPaletteToColorTableCategoricalMode()

{
    // If the palette is not loaded, then, ignore the conversion silently
    if (Palette->aadfPaletteColors[0].size() == 0)
        return CE_Failure;

    int nNPossibleValues = static_cast<int>(
        pow(2, static_cast<double>(8) * static_cast<int>(eMMBytesPerPixel)));
    for (int iColumn = 0; iColumn < 4; iColumn++)
    {
        try
        {
            aadfPCT[iColumn].resize(nNPossibleValues);
        }
        catch (std::bad_alloc &e)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "%s", e.what());
            return CE_Failure;
        }
    }

    int nIPaletteColor = 0;
    // Giving color to the ones in the table
    int nNPaletteColors =
        static_cast<int>(Palette->aadfPaletteColors[0].size());

    // No more colors than needed.
    if (nNPaletteColors > nNPossibleValues)
        nNPaletteColors = nNPossibleValues;

    for (nIPaletteColor = 0; nIPaletteColor < nNPaletteColors; nIPaletteColor++)
    {
        aadfPCT[0][nIPaletteColor] =
            Palette->aadfPaletteColors[0][nIPaletteColor];
        aadfPCT[1][nIPaletteColor] =
            Palette->aadfPaletteColors[1][nIPaletteColor];
        aadfPCT[2][nIPaletteColor] =
            Palette->aadfPaletteColors[2][nIPaletteColor];
        aadfPCT[3][nIPaletteColor] =
            Palette->aadfPaletteColors[3][nIPaletteColor];
    }

    // Rest of colors
    for (; nIPaletteColor < nNPossibleValues; nIPaletteColor++)
    {
        aadfPCT[0][nIPaletteColor] = Palette->GetDefaultColorRGB().c1;
        aadfPCT[1][nIPaletteColor] = Palette->GetDefaultColorRGB().c2;
        aadfPCT[2][nIPaletteColor] = Palette->GetDefaultColorRGB().c3;
        aadfPCT[3][nIPaletteColor] = Palette->GetDefaultColorRGB().c4;
    }

    return CE_None;
}

// Converts palleteColors to Colors of pixels for the Color Table
CPLErr MMRRasterBand::FromPaletteToColorTableContinousMode()

{
    MMRBand *poBand = pfRel->GetLastBand();
    if (!poBand)
        return CE_Failure;

    if (static_cast<int>(eMMBytesPerPixel) > 2)
        return CE_Failure;  // Attribute table

    // Some necessary information
    if (!poBand->GetMinVisuSet() || !poBand->GetMaxVisuSet())
        return CE_Failure;

    int nNPossibleValues = static_cast<int>(
        pow(2, static_cast<double>(8) * static_cast<int>(eMMBytesPerPixel)));
    for (int iColumn = 0; iColumn < 4; iColumn++)
    {
        try
        {
            aadfPCT[iColumn].resize(nNPossibleValues);
        }
        catch (std::bad_alloc &e)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "%s", e.what());
            return CE_Failure;
        }
    }

    // Number of real colors (appart from NoData)
    int nNPaletteColors =
        static_cast<int>(Palette->aadfPaletteColors[0].size());
    if (Palette->HasNodata())
        nNPaletteColors--;

    // If palette doesn't have nodata last index is associated
    if (!Palette->HasNodata())
        Palette->SetNoDataPaletteIndex(nNPaletteColors);

    if (static_cast<int>(eMMBytesPerPixel) > 2 &&
        nNPaletteColors < nNPossibleValues)
        return CE_Failure;

    int nFirstValidPaletteIndex;
    unsigned short nIndexColor;
    double dfSlope = 1, dfIntercept = 0;

    if (Palette->HasNodata() && Palette->GetNoDataPaletteIndex() == 0)
        nFirstValidPaletteIndex = 1;
    else
        nFirstValidPaletteIndex = 0;

    if (static_cast<int>(eMMBytesPerPixel) == 2)
    {
        // A scaling is applied between the minimum and maximum display values.
        dfSlope = nNPaletteColors /
                  ((poBand->GetVisuMax() + 1 - poBand->GetVisuMin()));

        if (Palette->HasNodata() && Palette->GetNoDataPaletteIndex() !=
                                        0)  // nodata at the end of the list
            dfIntercept = -dfSlope * poBand->GetVisuMin();
        else
            dfIntercept = -dfSlope * poBand->GetVisuMin() + 1;
    }

    for (int nIPaletteColor = 0; nIPaletteColor < nNPossibleValues;
         nIPaletteColor++)
    {
        if (poBand->BandHasNoData() &&
            nIPaletteColor == Palette->GetNoDataPaletteIndex())
        {
            if (Palette->HasNodata())
                AssignRGBColor(nIPaletteColor,
                               Palette->GetNoDataPaletteIndex());
            else
                AssignRGBColorDirectly(nIPaletteColor, 255);
        }
        else
        {
            if (nIPaletteColor < static_cast<int>(poBand->GetVisuMin()))
            {
                // Before the minimum, we apply the color of the first
                // element (as a placeholder).
                AssignRGBColor(nIPaletteColor, 0);
            }
            else if (nIPaletteColor <= static_cast<int>(poBand->GetVisuMax()))
            {
                // Between the minimum and maximum, we apply the value
                // read from the table.
                if (static_cast<int>(eMMBytesPerPixel) < 2)
                {
                    // The value is applied directly.
                    AssignRGBColor(nIPaletteColor, nFirstValidPaletteIndex);
                    nFirstValidPaletteIndex++;
                }
                else
                {
                    // The value is applied according to the scaling.
                    nIndexColor = static_cast<unsigned short>(
                        dfSlope * nIPaletteColor + dfIntercept);
                    AssignRGBColor(nIPaletteColor, nIndexColor);
                }
            }
            else
            {
                // After the maximum, we apply the value of the last
                // element(as a placeholder).
                AssignRGBColor(nIPaletteColor, nNPaletteColors - 1);
            }
        }
    }

    return CE_None;
}

CPLErr MMRRasterBand::GetAttributeTableName(char *papszToken,
                                            CPLString &osRELName,
                                            CPLString &osDBFName,
                                            CPLString &osAssociateREL)
{
    CPLString os_Join = "JoinTaula";
    os_Join.append("_");
    os_Join.append(papszToken);

    CPLString osTableNameSection_value =
        pfRel->GetMetadataValue(SECTION_ATTRIBUTE_DATA, osBandSection, os_Join);

    if (osTableNameSection_value.empty())
        return CE_Failure;  // No attribute available

    CPLString osTableNameSection = "TAULA_";
    osTableNameSection.append(osTableNameSection_value);

    CPLString osShortRELName =
        pfRel->GetMetadataValue(osTableNameSection, "NomFitxer");

    CPLString osExtension = CPLGetExtensionSafe(osShortRELName);
    if (osExtension.tolower() == "rel")
    {
        // Get path relative to REL file
        osRELName =
            CPLFormFilenameSafe(CPLGetPathSafe(pfRel->GetRELNameChar()).c_str(),
                                osShortRELName, "");

        // Getting information from the associated REL
        MMRRel *fLocalRel = new MMRRel(osRELName);
        CPLString osShortDBFName =
            fLocalRel->GetMetadataValue("TAULA_PRINCIPAL", "NomFitxer");

        if (osShortDBFName.empty())
        {
            osRELName = "";
            delete fLocalRel;
            return CE_Failure;
        }

        // Get path relative to REL file
        osDBFName = CPLFormFilenameSafe(
            CPLGetPathSafe(fLocalRel->GetRELNameChar()).c_str(), osShortDBFName,
            "");

        osAssociateREL =
            fLocalRel->GetMetadataValue("TAULA_PRINCIPAL", "AssociatRel");

        if (osAssociateREL.empty())
        {
            osRELName = "";
            delete fLocalRel;
            return CE_Failure;
        }

        CPLString osSection = "TAULA_PRINCIPAL:";
        osSection.append(osAssociateREL);

        CPLString osTactVar =
            fLocalRel->GetMetadataValue(osSection, "TractamentVariable");
        if (osTactVar == "Categoric")
            poDefaultRAT->SetTableType(GRTT_THEMATIC);
        else
            poDefaultRAT->SetTableType(GRTT_ATHEMATIC);

        delete fLocalRel;
        return CE_None;
    }

    osExtension = CPLGetExtensionSafe(osShortRELName);
    if (osExtension.tolower() == "dbf")
    {
        // Get path relative to REL file
        osDBFName =
            CPLFormFilenameSafe(CPLGetPathSafe(pfRel->GetRELNameChar()).c_str(),
                                osShortRELName, "");

        osAssociateREL =
            pfRel->GetMetadataValue(osTableNameSection, "AssociatRel");

        return CE_None;
    }

    osRELName = "";
    osAssociateREL = "";

    return CE_Failure;
}

// Converts palleteColors to Colors of pixels in the attribute table
CPLErr MMRRasterBand::FromPaletteToAttributeTableContinousMode()

{
    if (!Palette)
        return CE_None;

    MMRBand *poBand = pfRel->GetLastBand();
    if (!poBand)
        return CE_Failure;

    // Some necessary information
    if (!poBand->GetMinVisuSet() || !poBand->GetMaxVisuSet())
        return CE_Failure;

    // Number of real colors (appart from NoData)
    int nNPaletteColors =
        static_cast<int>(Palette->aadfPaletteColors[0].size());
    int nRealNPaletteColors = nNPaletteColors;
    if (Palette->HasNodata())
    {
        if (nNPaletteColors < 1)
            return CE_Failure;
        nNPaletteColors--;
    }

    if (nNPaletteColors <= 0)
        return CE_Failure;

    poDefaultRAT->CreateColumn("MIN", GFT_Real, GFU_Min);
    poDefaultRAT->CreateColumn("MAX", GFT_Real, GFU_Max);
    poDefaultRAT->CreateColumn("Red", GFT_Integer, GFU_Red);
    poDefaultRAT->CreateColumn("Green", GFT_Integer, GFU_Green);
    poDefaultRAT->CreateColumn("Blue", GFT_Integer, GFU_Blue);

    poDefaultRAT->SetTableType(GRTT_ATHEMATIC);
    poDefaultRAT->SetRowCount(
        static_cast<int>(nRealNPaletteColors));  // +1 for last element

    // If palette doesn't have nodata last index is associated
    if (!Palette->HasNodata())
        Palette->SetNoDataPaletteIndex(nNPaletteColors);

    // A scaling is applied between the minimum and maximum display values.
    double dfInterval =
        (poBand->GetVisuMax() - poBand->GetVisuMin()) / nNPaletteColors;

    if (poBand->BandHasNoData() && Palette->HasNodata())
    {
        poDefaultRAT->SetValue(0, 0, poBand->GetNoDataValue());
        poDefaultRAT->SetValue(0, 1, poBand->GetNoDataValue());
        poDefaultRAT->SetValue(
            0, 2,
            Palette->aadfPaletteColors[0][Palette->GetNoDataPaletteIndex()]);
        poDefaultRAT->SetValue(
            0, 3,
            Palette->aadfPaletteColors[1][Palette->GetNoDataPaletteIndex()]);
        poDefaultRAT->SetValue(
            0, 4,
            Palette->aadfPaletteColors[2][Palette->GetNoDataPaletteIndex()]);
    }

    int nIPaletteColor = 0;
    for (; nIPaletteColor < nNPaletteColors; nIPaletteColor++)
    {
        poDefaultRAT->SetValue(nIPaletteColor + 1, 0,
                               poBand->GetVisuMin() +
                                   dfInterval * nIPaletteColor);
        poDefaultRAT->SetValue(
            nIPaletteColor + 1, 1,
            poBand->GetVisuMin() +
                dfInterval * (static_cast<double>(nIPaletteColor) + 1));
        poDefaultRAT->SetValue(nIPaletteColor + 1, 2,
                               Palette->aadfPaletteColors[0][nIPaletteColor]);
        poDefaultRAT->SetValue(nIPaletteColor + 1, 3,
                               Palette->aadfPaletteColors[1][nIPaletteColor]);
        poDefaultRAT->SetValue(nIPaletteColor + 1, 4,
                               Palette->aadfPaletteColors[2][nIPaletteColor]);
    }

    // Last interval
    poDefaultRAT->SetValue(nIPaletteColor, 0,
                           poBand->GetVisuMin() +
                               dfInterval *
                                   (static_cast<double>(nNPaletteColors) - 1));
    poDefaultRAT->SetValue(nIPaletteColor, 1, poBand->GetVisuMax());
    poDefaultRAT->SetValue(nIPaletteColor, 2,
                           Palette->aadfPaletteColors[0][nIPaletteColor - 1]);
    poDefaultRAT->SetValue(nIPaletteColor, 3,
                           Palette->aadfPaletteColors[1][nIPaletteColor - 1]);
    poDefaultRAT->SetValue(nIPaletteColor, 4,
                           Palette->aadfPaletteColors[2][nIPaletteColor - 1]);

    // Last value
    poDefaultRAT->SetValue(nIPaletteColor + 1, 0, poBand->GetVisuMax());
    poDefaultRAT->SetValue(nIPaletteColor + 1, 1, poBand->GetVisuMax());
    poDefaultRAT->SetValue(nIPaletteColor + 1, 2,
                           Palette->aadfPaletteColors[0][nIPaletteColor - 1]);
    poDefaultRAT->SetValue(nIPaletteColor + 1, 3,
                           Palette->aadfPaletteColors[1][nIPaletteColor - 1]);
    poDefaultRAT->SetValue(nIPaletteColor + 1, 4,
                           Palette->aadfPaletteColors[2][nIPaletteColor - 1]);

    return CE_None;
}

// Converts palette Colors to Colors of pixels
CPLErr MMRRasterBand::FromPaletteToAttributeTableCategoricalMode()

{
    // TODO: aixo va a la taula d'atributs en mode categòric
    // If the palette is not loaded, then, ignore the conversion silently
    if (Palette->aadfPaletteColors[0].size() == 0)
        return CE_Failure;

    int nNPossibleValues = static_cast<int>(
        pow(2, static_cast<double>(8) * static_cast<int>(eMMBytesPerPixel)));
    for (int iColumn = 0; iColumn < 4; iColumn++)
    {
        try
        {
            aadfPCT[iColumn].resize(nNPossibleValues);
        }
        catch (std::bad_alloc &e)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "%s", e.what());
            return CE_Failure;
        }
    }

    int nIPaletteColor = 0;
    // Giving color to the ones in the table
    int nNPaletteColors =
        static_cast<int>(Palette->aadfPaletteColors[0].size());

    // No more colors than needed.
    if (nNPaletteColors > nNPossibleValues)
        nNPaletteColors = nNPossibleValues;

    for (nIPaletteColor = 0; nIPaletteColor < nNPaletteColors; nIPaletteColor++)
    {
        aadfPCT[0][nIPaletteColor] =
            Palette->aadfPaletteColors[0][nIPaletteColor];
        aadfPCT[1][nIPaletteColor] =
            Palette->aadfPaletteColors[1][nIPaletteColor];
        aadfPCT[2][nIPaletteColor] =
            Palette->aadfPaletteColors[2][nIPaletteColor];
        aadfPCT[3][nIPaletteColor] =
            Palette->aadfPaletteColors[3][nIPaletteColor];
    }

    // Rest of colors
    for (; nIPaletteColor < nNPossibleValues; nIPaletteColor++)
    {
        aadfPCT[0][nIPaletteColor] = Palette->GetDefaultColorRGB().c1;
        aadfPCT[1][nIPaletteColor] = Palette->GetDefaultColorRGB().c2;
        aadfPCT[2][nIPaletteColor] = Palette->GetDefaultColorRGB().c3;
        aadfPCT[3][nIPaletteColor] = Palette->GetDefaultColorRGB().c4;
    }

    return CE_None;
}

void MMRRasterBand::ConvertColorsFromPaletteToColorTable()
{
    int nColors = static_cast<int>(GetPCT_Red().size());

    if (nColors > 0)
    {
        for (int iColor = 0; iColor < nColors; iColor++)
        {
            GDALColorEntry sEntry = {
                static_cast<short int>(GetPCT_Red()[iColor]),
                static_cast<short int>(GetPCT_Green()[iColor]),
                static_cast<short int>(GetPCT_Blue()[iColor]),
                static_cast<short int>(GetPCT_Alpha()[iColor])};

            if ((sEntry.c1 < 0 || sEntry.c1 > 255) ||
                (sEntry.c2 < 0 || sEntry.c2 > 255) ||
                (sEntry.c3 < 0 || sEntry.c3 > 255) ||
                (sEntry.c4 < 0 || sEntry.c4 > 255))
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Color table entry appears to be corrupt, skipping "
                         "the rest. ");
                break;
            }

            poCT->SetColorEntry(iColor, &sEntry);
        }
    }
}

void MMRRasterBand::AssignRGBColor(int nIndexDstCT, int nIndexSrcPalete)
{
    aadfPCT[0][nIndexDstCT] = Palette->aadfPaletteColors[0][nIndexSrcPalete];
    aadfPCT[1][nIndexDstCT] = Palette->aadfPaletteColors[1][nIndexSrcPalete];
    aadfPCT[2][nIndexDstCT] = Palette->aadfPaletteColors[2][nIndexSrcPalete];
    aadfPCT[3][nIndexDstCT] = Palette->aadfPaletteColors[3][nIndexSrcPalete];
}

void MMRRasterBand::AssignRGBColorDirectly(int nIndexDstCT, double dfValue)
{
    aadfPCT[0][nIndexDstCT] = dfValue;
    aadfPCT[1][nIndexDstCT] = dfValue;
    aadfPCT[2][nIndexDstCT] = dfValue;
    aadfPCT[3][nIndexDstCT] = dfValue;
}
