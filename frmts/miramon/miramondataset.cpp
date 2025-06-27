/******************************************************************************
 *
 * Project:  MiraMonRaster driver
 * Purpose:  Implements MMRDataset class.
 * Author:   Abel Pau
 * 
 ******************************************************************************
 * Copyright (c) 2025, Xavier Pons
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

//#include "cpl_port.h"
#include "miramondataset.h"
#include "miramon_p.h"
#include "miramonrel.h"
#include "miramon_rastertools.h"

//#include <cassert>
/*
#include <climits>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#if HAVE_FCNTL_H
#include <fcntl.h>
#endif
#include <algorithm>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_minixml.h"
#include "cpl_progress.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "gdal.h"
#include "gdal_frmts.h"
#include "gdal_pam.h"
#include "gdal_priv.h"
#include "gdal_rat.h"
#include "miramon.h"
#include "ogr_core.h"
#include "ogr_spatialref.h"
#include "ogr_srs_api.h"
*/

#ifdef MSVC
#include "..\miramon_common\mm_gdal_functions.h"  // For MMCheck_REL_FILE()
#else
#include "../miramon_common/mm_gdal_functions.h"  // For MMCheck_REL_FILE()
#endif

/************************************************************************/
/*                           MMRRasterBand()                            */
/************************************************************************/
MMRRasterBand::MMRRasterBand(MMRDataset *poDSIn, int nBandIn)
    : poCT(nullptr), eMMRDataType(EPT_MIN), hMMR(poDSIn->hMMR),
      bMetadataDirty(false), poDefaultRAT(nullptr)
{
    poDS = poDSIn;

    nBand = nBandIn;
    eAccess = poDSIn->GetAccess();

    int nCompression = 0;
    MMRGetBandInfo(hMMR, nBand, &osBandSection, &eMMRDataTypeMiraMon,
                   &eMMBytesPerPixel, &nBlockXSize, &nBlockYSize,
                   &nCompression);

    // Set some other information.
    if (nCompression != 0)
        GDALMajorObject::SetMetadataItem("COMPRESSION", "RLE",
                                         "IMAGE_STRUCTURE");
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
                     (int)eMMRDataTypeMiraMon);
            break;
    }

    // Collect color table if present.
    CPLErr eErr = MMRGetPCT(hMMR, nBand);
    int nColors =
        static_cast<int>(hMMR->papoBand[nBand - 1]->GetPCT_Red().size());

    if (eErr == CE_None && nColors > 0)
    {
        poCT = new GDALColorTable(GPI_RGB);
        for (int iColor = 0; iColor < nColors; iColor++)
        {
            GDALColorEntry sEntry = {
                (short int)(hMMR->papoBand[nBand - 1]->GetPCT_Red()[iColor]),
                (short int)(hMMR->papoBand[nBand - 1]->GetPCT_Green()[iColor]),
                (short int)(hMMR->papoBand[nBand - 1]->GetPCT_Blue()[iColor]),
                (short int)(hMMR->papoBand[nBand - 1]->GetPCT_Alpha()[iColor])};

            if ((sEntry.c1 < 0 || sEntry.c1 > 255) ||
                (sEntry.c2 < 0 || sEntry.c2 > 255) ||
                (sEntry.c3 < 0 || sEntry.c3 > 255))
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

/************************************************************************/
/*                           ~MMRRasterBand()                           */
/************************************************************************/

MMRRasterBand::~MMRRasterBand()

{
    FlushCache(true);

    if (poCT != nullptr)
        delete poCT;

    if (poDefaultRAT)
        delete poDefaultRAT;
}

/************************************************************************/
/*                             GetNoDataValue()                         */
/************************************************************************/

double MMRRasterBand::GetNoDataValue(int *pbSuccess)

{
    double dfNoData = 0.0;

    if (MMRGetBandNoData(hMMR, nBand, &dfNoData))
    {
        if (pbSuccess)
            *pbSuccess = TRUE;
        return dfNoData;
    }
    if (pbSuccess)
        *pbSuccess = FALSE;
    return dfNoData;

    //return GDALPamRasterBand::GetNoDataValue(pbSuccess);
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

    eErr = MMRGetRasterBlockEx(hMMR, nBand, nBlockXOff, nBlockYOff, pImage,
                               nBlockXSize * nBlockYSize *
                                   GDALGetDataTypeSizeBytes(eDataType));

    if (eErr == CE_None &&
        eMMRDataTypeMiraMon == MMDataType::DATATYPE_AND_COMPR_BIT)
    {
        GByte *pabyData = static_cast<GByte *>(pImage);

        for (int ii = nBlockXSize * nBlockYSize - 1; ii >= 0; ii--)
        {
            if ((pabyData[ii >> 3] & (1 << (ii & 0x7))))
                pabyData[ii] = 1;
            else
                pabyData[ii] = 0;
        }
    }

    return eErr;
}

/************************************************************************/
/*                         GetDescription()                             */
/************************************************************************/

const char *MMRRasterBand::GetDescription() const
{
    return MMRGetBandName(hMMR, nBand);
    /*
    const char *pszName = MMRGetBandName(hMMR, nBand);

    if (pszName == nullptr)
        return GDALPamRasterBand::GetDescription();

    return pszName;
    */
}

/************************************************************************/
/*                         MMRGetBandName()                             */
/************************************************************************/

const char *MMRGetBandName(MMRHandle hMMR, int nBand)
{
    if (nBand < 1 || nBand > hMMR->nBands)
        return "";

    return hMMR->papoBand[nBand - 1]->GetBandName();
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
/*                            SetMetadata()                             */
/************************************************************************/

CPLErr MMRRasterBand::SetMetadataItem(const char *pszTag, const char *pszValue,
                                      const char *pszDomain)

{
    bMetadataDirty = true;

    return GDALPamRasterBand::SetMetadataItem(pszTag, pszValue, pszDomain);
}

/************************************************************************/
/*                        GetDefaultHistogram()                         */
/************************************************************************/

CPLErr MMRRasterBand::GetDefaultHistogram(double *pdfMin, double *pdfMax,
                                          int *pnBuckets,
                                          GUIntBig **ppanHistogram, int bForce,
                                          GDALProgressFunc pfnProgress,
                                          void *pProgressData)

{
    if (GetMetadataItem("STATISTICS_HISTOBINVALUES") != nullptr &&
        GetMetadataItem("STATISTICS_HISTOMIN") != nullptr &&
        GetMetadataItem("STATISTICS_HISTOMAX") != nullptr)
    {
        const char *pszBinValues = GetMetadataItem("STATISTICS_HISTOBINVALUES");

        *pdfMin = CPLAtof(GetMetadataItem("STATISTICS_HISTOMIN"));
        *pdfMax = CPLAtof(GetMetadataItem("STATISTICS_HISTOMAX"));

        *pnBuckets = 0;
        for (int i = 0; pszBinValues[i] != '\0'; i++)
        {
            if (pszBinValues[i] == '|')
                (*pnBuckets)++;
        }

        *ppanHistogram =
            static_cast<GUIntBig *>(CPLCalloc(sizeof(GUIntBig), *pnBuckets));

        const char *pszNextBin = pszBinValues;
        for (int i = 0; i < *pnBuckets; i++)
        {
            (*ppanHistogram)[i] =
                static_cast<GUIntBig>(CPLAtoGIntBig(pszNextBin));

            while (*pszNextBin != '|' && *pszNextBin != '\0')
                pszNextBin++;
            if (*pszNextBin == '|')
                pszNextBin++;
        }

        // Adjust min/max to reflect outer edges of buckets.
        double dfBucketWidth = (*pdfMax - *pdfMin) / (*pnBuckets - 1);
        *pdfMax += 0.5 * dfBucketWidth;
        *pdfMin -= 0.5 * dfBucketWidth;

        return CE_None;
    }

    return GDALPamRasterBand::GetDefaultHistogram(pdfMin, pdfMax, pnBuckets,
                                                  ppanHistogram, bForce,
                                                  pfnProgress, pProgressData);
}

/************************************************************************/
/*                           GetDefaultRAT()                            */
/************************************************************************/

GDALRasterAttributeTable *MMRRasterBand::GetDefaultRAT()

{
    if (poDefaultRAT != nullptr)
        return poDefaultRAT;

    poDefaultRAT = new GDALDefaultRasterAttributeTable();

    CPLString os_IndexJoin = hMMR->fRel->GetMetadataValue(
        SECTION_ATTRIBUTE_DATA, osBandSection, "IndexsJoinTaula");

    if (os_IndexJoin.empty())
        return nullptr;  // No attribute available

    char **papszTokens = CSLTokenizeString2(os_IndexJoin, ",", 0);
    const int nTokens = CSLCount(papszTokens);

    if (nTokens < 1)
    {
        CSLDestroy(papszTokens);
        return nullptr;
    }

    for (int nIAttTable = 0; nIAttTable < nTokens; nIAttTable++)
    {
        CPLString osRELName, osDBFName, osAssociateRel;
        if (CE_None != GetAttributeTableName(papszTokens[nIAttTable], osRELName,
                                             osDBFName, osAssociateRel))
        {
            CSLDestroy(papszTokens);
            return nullptr;
        }

        if (osDBFName.empty())
        {
            CSLDestroy(papszTokens);
            return nullptr;
        }

        if (CE_None !=
            CreateAttributteTableFromDBF(osRELName, osDBFName, osAssociateRel))
        {
            CSLDestroy(papszTokens);
            return nullptr;
        }
    }
    CSLDestroy(papszTokens);

    return poDefaultRAT;
}

CPLErr MMRRasterBand::CreateAttributteTableFromDBF(CPLString osRELName,
                                                   CPLString osDBFName,
                                                   CPLString osAssociateRel)
{
    struct MM_DATA_BASE_XP oAttributteTable;
    memset(&oAttributteTable, 0, sizeof(oAttributteTable));

    if (osRELName != "")
    {
        if (MM_ReadExtendedDBFHeaderFromFile(
                osDBFName.c_str(), &oAttributteTable, (const char *)osRELName))
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

    MM_EXT_DBF_N_FIELDS nFieldIndex = oAttributteTable.nFields;
    MM_EXT_DBF_N_FIELDS nCategIndex = oAttributteTable.nFields;
    for (MM_EXT_DBF_N_FIELDS nIField = 0; nIField < oAttributteTable.nFields;
         nIField++)
    {
        if (EQUAL(oAttributteTable.pField[nIField].FieldName, osAssociateRel))
        {
            nFieldIndex = nIField;
            if (nIField + 1 < oAttributteTable.nFields)
                nCategIndex = nIField + 1;
            else if (nIField - 1 > 0)
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

    // 1 column: category name
    if (CE_None != poDefaultRAT->CreateColumn(
                       oAttributteTable.pField[nCategIndex].FieldName,
                       GFT_String, GFU_Name))
        return CE_Failure;

    VSIFSeekL(oAttributteTable.pfDataBase, oAttributteTable.FirstRecordOffset,
              SEEK_SET);
    poDefaultRAT->SetRowCount(static_cast<int>(oAttributteTable.nRecords));

    MM_ACCUMULATED_BYTES_TYPE_DBF nBufferSize =
        oAttributteTable.BytesPerRecord + 1;
    char *pzsBuffer = static_cast<char *>(VSI_CALLOC_VERBOSE(1, nBufferSize));
    char *pzsField = static_cast<char *>(VSI_CALLOC_VERBOSE(1, nBufferSize));

    for (int nIRecord = 0;
         nIRecord < static_cast<int>(oAttributteTable.nRecords); nIRecord++)
    {
        if (oAttributteTable.BytesPerRecord !=
            VSIFReadL(pzsBuffer, sizeof(unsigned char),
                      oAttributteTable.BytesPerRecord,
                      oAttributteTable.pfDataBase))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Invalid attribute table: \"%s\"", osDBFName.c_str());
            return CE_Failure;
        }

        // Category index
        memcpy(pzsField,
               pzsBuffer +
                   oAttributteTable.pField[nFieldIndex].AccumulatedBytes,
               oAttributteTable.pField[nFieldIndex].BytesPerField);
        pzsField[oAttributteTable.pField[nFieldIndex].BytesPerField] = '\0';
        CPLString osField = pzsField;
        poDefaultRAT->SetValue(nIRecord, 0, osField);

        // Category value
        memcpy(pzsField,
               pzsBuffer +
                   oAttributteTable.pField[nCategIndex].AccumulatedBytes,
               oAttributteTable.pField[nCategIndex].BytesPerField);
        pzsField[oAttributteTable.pField[nCategIndex].BytesPerField] = '\0';
        char *pszFieldRecoded =
            CPLRecode(pzsField, CPL_ENC_ISO8859_1, CPL_ENC_UTF8);
        poDefaultRAT->SetValue(nIRecord, 1, pszFieldRecoded);
        CPLFree(pszFieldRecoded);
    }

    VSIFree(pzsField);
    VSIFree(pzsBuffer);
    VSIFCloseL(oAttributteTable.pfDataBase);
    MM_ReleaseMainFields(&oAttributteTable);

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

    CPLString osTableNameSection_key = hMMR->fRel->GetMetadataValue(
        SECTION_ATTRIBUTE_DATA, osBandSection, os_Join);

    if (osTableNameSection_key.empty())
        return CE_Failure;  // No attribute available

    CPLString osTableNameSection = "TAULA_";
    osTableNameSection.append(osTableNameSection_key);

    CPLString osShortRELName =
        hMMR->fRel->GetMetadataValue(osTableNameSection, "NomFitxer");

    CPLString osExtension = CPLGetExtensionSafe(osShortRELName);
    CPLString osShortDBFName;
    if (osExtension.tolower() == "rel")
    {
        // Get path relative to REL file
        osRELName = CPLFormFilenameSafe(
            CPLGetPathSafe(hMMR->fRel->GetRELNameChar()).c_str(),
            osShortRELName, "");

        // Getting information from the associated REL
        MMRRel *fRel = new MMRRel(osRELName);
        osShortDBFName = fRel->GetMetadataValue("TAULA_PRINCIPAL", "NomFitxer");

        if (osShortDBFName.empty())
        {
            osRELName = "";
            return CE_Failure;
        }

        osAssociateREL =
            fRel->GetMetadataValue("TAULA_PRINCIPAL", "AssociatRel");

        if (osAssociateREL.empty())
        {
            osRELName = "";
            return CE_Failure;
        }

        delete fRel;
    }
    else
        osShortDBFName = osShortRELName;

    osExtension = CPLGetExtensionSafe(osShortDBFName);
    if (osExtension.tolower() == "dbf")
    {
        // Get path relative to REL file
        osDBFName = CPLFormFilenameSafe(
            CPLGetPathSafe(hMMR->fRel->GetRELNameChar()).c_str(),
            osShortDBFName, "");
        return CE_None;
    }
    else
    {
        osRELName = "";
        osAssociateREL = "";
    }

    return CE_Failure;
}

/************************************************************************/
/* ==================================================================== */
/*                            MMRDataset                               */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                            MMRDataset()                            */
/************************************************************************/

MMRDataset::MMRDataset()
{
    m_oSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

    memset(adfGeoTransform, 0, sizeof(adfGeoTransform));

    nNSubdataSets = 0;
}

/************************************************************************/
/*                           ~MMRDataset()                            */
/************************************************************************/

MMRDataset::~MMRDataset()

{
    FlushCache(true);

    // Destroy the raster bands if they exist.  We forcibly clean
    // them up now to avoid any effort to write to them after the
    // file is closed.
    for (int i = 0; i < nBands && papoBands != nullptr; i++)
    {
        if (papoBands[i] != nullptr)
            delete papoBands[i];
    }
    CPLFree(papoBands);
    papoBands = nullptr;

    // Close the file.
    if (hMMR != nullptr)
    {
        if (MMRClose(hMMR) != 0)
        {
            CPLError(CE_Failure, CPLE_FileIO, "I/O error");
        }
        hMMR = nullptr;
    }
}

/************************************************************************/
/*                           ReadProjection()                           */
/************************************************************************/
CPLErr MMRDataset::ReadProjection()

{
    if (!hMMR->fRel)
        return CE_Failure;

    CPLString osSRS = hMMR->fRel->GetMetadataValue(
        "SPATIAL_REFERENCE_SYSTEM:HORIZONTAL", "HorizontalSystemIdentifier");

    char szResult[MM_MAX_ID_SNY + 10];
    int nResult = ReturnEPSGCodeSRSFromMMIDSRS(osSRS.c_str(), szResult);
    if (nResult == 1 || szResult[0] == '\0')
        return CE_Failure;

    m_oSRS.importFromEPSG(atoi(szResult));

    return m_oSRS.IsEmpty() ? CE_Failure : CE_None;
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/
int MMRDataset::Identify(GDALOpenInfo *poOpenInfo)

{
    if (MMRRel::IdentifySubdataSetFile(poOpenInfo->pszFilename))
        return TRUE;

    return MMRRel::IdentifyFile(poOpenInfo->pszFilename);
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/
GDALDataset *MMRDataset::Open(GDALOpenInfo *poOpenInfo)

{
    // Verify that this is a MMR file.
#ifndef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
    // During fuzzing, do not use Identify to reject crazy content.
    if (!Identify(poOpenInfo))
        return nullptr;
#endif

    // Creating of the object that allows inspect metadata (REL file)
    MMRRel *fRel = new MMRRel(poOpenInfo->pszFilename);

    // Getting the info fromthat REL
    MMRHandle hMMR =
        fRel->GetInfoFromREL(poOpenInfo->pszFilename,
                             (poOpenInfo->eAccess == GA_Update ? "r+" : "r"));
    if (!hMMR)
    {
        delete fRel;
        return nullptr;
    }

    if (hMMR->nBands == 0)
    {
        delete fRel;
        delete hMMR;
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Unable to open %s, it has zero usable bands.",
                 poOpenInfo->pszFilename);
        return nullptr;
    }

    // Creating the Dataset (with bands or Subdatasets).
    MMRDataset *poDS;
    poDS = new MMRDataset();
    poDS->hMMR =
        hMMR;  // Assigning the information to the Dataset, who will delete that
    poDS->eAccess = poOpenInfo->eAccess;

    // General Dataset information available
    poDS->nRasterXSize = hMMR->nXSize;
    poDS->nRasterYSize = hMMR->nYSize;
    poDS->GetDataSetBoundingBox();  // Fills adfGeoTransform
    poDS->ReadProjection();
    poDS->nBands = 0;

    // Assign every band to a subdataset (if any)
    // If all bands should go to a one single Subdataset, then,
    // no subdataset will be created and all bands will go to this
    // dataset.
    poDS->AssignBandsToSubdataSets();

    // Create subdatasets or add bands, as needed
    if (poDS->nNSubdataSets)
        poDS->CreateSubdatasetsFromBands();
    else
        poDS->AssignBands(poOpenInfo);

    return poDS;
}

/************************************************************************/
/*                          GetSpatialRef()                             */
/************************************************************************/

const OGRSpatialReference *MMRDataset::GetSpatialRef() const
{
    return m_oSRS.IsEmpty() ? nullptr : &m_oSRS;
}

/************************************************************************/
/*                           SetSpatialRef()                            */
/************************************************************************/
#ifdef TODO
CPLErr MMRDataset::SetSpatialRef(const OGRSpatialReference *poSRS)

{
    m_oSRS.Clear();
    if (poSRS)
        m_oSRS = *poSRS;
    bGeoDirty = true;

    return CE_None;
}
#endif  //TODO
/************************************************************************/
/*                            SetMetadata()                             */
/************************************************************************/

CPLErr MMRDataset::SetMetadata(char **papszMDIn, const char *pszDomain)

{
    bMetadataDirty = true;

    return GDALPamDataset::SetMetadata(papszMDIn, pszDomain);
}

/************************************************************************/
/*                            SetMetadata()                             */
/************************************************************************/

CPLErr MMRDataset::SetMetadataItem(const char *pszTag, const char *pszValue,
                                   const char *pszDomain)

{
    bMetadataDirty = true;

    return GDALPamDataset::SetMetadataItem(pszTag, pszValue, pszDomain);
}

/************************************************************************/
/*                          GetColumnsNumberFromREL()                          */
/*                          GetDataSetBoundingBox()                     */
/*                          GetGeoTransform()                           */
/************************************************************************/
int MMRDataset::GetColumnsNumberFromREL(int *nNCols)
{
    // Number of columns of the subdataset (if exist)
    // Section [OVERVIEW:ASPECTES_TECNICS] in rel file
    if (!nNCols || !hMMR || !hMMR->fRel)
        return 1;

    CPLString osValue =
        hMMR->fRel->GetMetadataValue(SECTION_OVVW_ASPECTES_TECNICS, "columns");

    if (osValue.empty())
        return 1;

    *nNCols = atoi(osValue);
    return 0;
}

int MMRDataset::GetRowsNumberFromREL(int *nNRows)
{
    // Number of columns of the subdataset (if exist)
    // Section [OVERVIEW:ASPECTES_TECNICS] in rel file
    // Key raws
    if (!nNRows || !hMMR || !hMMR->fRel)
        return 1;

    CPLString osValue =
        hMMR->fRel->GetMetadataValue(SECTION_OVVW_ASPECTES_TECNICS, "rows");

    if (osValue.empty())
        return 1;

    *nNRows = atoi(osValue);
    return 0;
}

int MMRDataset::GetDataSetBoundingBox()
{
    // Bounding box of the band
    // Section [EXTENT] in rel file

    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;

    if (!hMMR || !hMMR->fRel)
        return 1;

    CPLString osMinX = hMMR->fRel->GetMetadataValue(SECTION_EXTENT, "MinX");
    if (osMinX.empty())
        return 1;
    adfGeoTransform[0] = atof(osMinX);

    int nNCols;
    if (1 == GetColumnsNumberFromREL(&nNCols) || nNCols <= 0)
        return 1;

    CPLString osMaxX = hMMR->fRel->GetMetadataValue(SECTION_EXTENT, "MaxX");
    if (osMaxX.empty())
        return 1;

    adfGeoTransform[1] = (atof(osMaxX) - adfGeoTransform[0]) / nNCols;
    adfGeoTransform[2] = 0.0;  // No rotation in MiraMon rasters

    CPLString osMinY = hMMR->fRel->GetMetadataValue(SECTION_EXTENT, "MinY");
    if (osMinY.empty())
        return 1;

    CPLString osMaxY = hMMR->fRel->GetMetadataValue(SECTION_EXTENT, "MaxY");
    if (osMaxY.empty())
        return 1;

    int nNRows;
    if (1 == GetRowsNumberFromREL(&nNRows) || nNRows <= 0)
        return 1;

    adfGeoTransform[3] = atof(osMaxY);
    adfGeoTransform[4] = 0.0;

    adfGeoTransform[5] = (atof(osMinY) - adfGeoTransform[3]) / nNRows;

    return 0;
}

int MMRDataset::GetBandBoundingBox(int nIBand)
{
    // Bounding box of the band
    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;

    if (!hMMR || !hMMR->papoBand || nIBand >= hMMR->nBands ||
        !hMMR->papoBand[nIBand])
        return 1;

    MMRBand *poBand = hMMR->papoBand[nIBand];

    adfGeoTransform[0] = poBand->GetBoundingBoxMinX();
    adfGeoTransform[1] =
        (poBand->GetBoundingBoxMaxX() - adfGeoTransform[0]) / poBand->nWidth;
    adfGeoTransform[2] = 0.0;  // No rotation in MiraMon rasters
    adfGeoTransform[3] = poBand->GetBoundingBoxMaxY();
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] =
        (poBand->GetBoundingBoxMinY() - adfGeoTransform[3]) / poBand->nHeight;

    return 0;
}

CPLErr MMRDataset::GetGeoTransform(double *padfTransform)

{
    if (adfGeoTransform[0] != 0.0 || adfGeoTransform[1] != 1.0 ||
        adfGeoTransform[2] != 0.0 || adfGeoTransform[3] != 0.0 ||
        adfGeoTransform[4] != 0.0 || adfGeoTransform[5] != 1.0)
    {
        memcpy(padfTransform, adfGeoTransform, sizeof(double) * 6);
        return CE_None;
    }

    //return GDALPamDataset::GetGeoTransform(padfTransform);
    return GDALDataset::GetGeoTransform(padfTransform);
}

/************************************************************************/
/*                          SetGeoTransform()                           */
/************************************************************************/
#ifdef TODO
CPLErr MMRDataset::SetGeoTransform(double *padfTransform)

{
    memcpy(adfGeoTransform, padfTransform, sizeof(double) * 6);
    bGeoDirty = true;

    return CE_None;
}
#endif  //TODO

/************************************************************************/
/*                            GetGCPCount()                             */
/************************************************************************/

int MMRDataset::GetGCPCount()
{
    const int nPAMCount = GDALPamDataset::GetGCPCount();
    return nPAMCount > 0 ? nPAMCount : static_cast<int>(m_aoGCPs.size());
}

/************************************************************************/
/*                          GetGCPSpatialRef()                          */
/************************************************************************/

const OGRSpatialReference *MMRDataset::GetGCPSpatialRef() const

{
    const OGRSpatialReference *poSRS = GDALPamDataset::GetGCPSpatialRef();
    if (poSRS)
        return poSRS;
    return !m_aoGCPs.empty() && !m_oSRS.IsEmpty() ? &m_oSRS : nullptr;
}

/************************************************************************/
/*                               GetGCPs()                              */
/************************************************************************/

const GDAL_GCP *MMRDataset::GetGCPs()
{
    const GDAL_GCP *psPAMGCPs = GDALPamDataset::GetGCPs();
    if (psPAMGCPs)
        return psPAMGCPs;
    return gdal::GCP::c_ptr(m_aoGCPs);
}

/************************************************************************/
/*                GDALRegister_MiraMonRaster()                          */
/************************************************************************/

void GDALRegister_MiraMonRaster()

{
    if (GDALGetDriverByName("MiraMonRaster") != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();

    //·$·TODO Fitxer ajuda!!
    poDriver->SetDescription("MiraMonRaster");
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME,
                              "MiraMon Raster Images (.img)");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/raster/hfa.html");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "img");
    poDriver->SetMetadataItem(
        GDAL_DMD_CREATIONDATATYPES,
        "Byte Int8 Int16 UInt16 Int32 UInt32 Float32 Float64 "
        "CFloat32 CFloat64");

    /*poDriver->SetMetadataItem(
        GDAL_DMD_CREATIONOPTIONLIST,
        "<CreationOptionList>"
        "   <Option name='BLOCKSIZE' type='integer' description='tile "
        "width/height (32-2048)' default='64'/>"
        "   <Option name='USE_SPILL' type='boolean' description='Force use of "
        "spill file'/>"
        "   <Option name='COMPRESSED' alias='COMPRESS' type='boolean' "
        "description='compress blocks'/>"
        "   <Option name='NBITS' type='integer' description='Create file with "
        "special sub-byte data type (1/2/4)'/>"
        "   <Option name='STATISTICS' type='boolean' description='Generate "
        "statistics and a histogram'/>"
        "</CreationOptionList>");
        */

    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");

    //poDriver->SetMetadataItem(GDAL_DCAP_UPDATE, "YES");
    //poDriver->SetMetadataItem(GDAL_DMD_UPDATE_ITEMS,
    //                          "GeoTransform SRS NoData "
    //                          "RasterValues "
    //                          "DatasetMetadata BandMetadata");
    poDriver->SetMetadataItem(GDAL_DMD_SUBDATASETS, "YES");

    poDriver->pfnOpen = MMRDataset::Open;
    poDriver->pfnCreate = MMRDataset::Create;
    //poDriver->pfnCreateCopy = MMRDataset::CreateCopy;
    poDriver->pfnIdentify = MMRDataset::Identify;
    //poDriver->pfnCopyFiles = MMRDataset::CopyFiles;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}

bool MMRDataset::NextBandInANewDataSet(int nIBand)
{
    if (nIBand < 0)
        return false;

    if (nIBand + 1 >= hMMR->nBands)
        return false;

    MMRBand *pThisBand = hMMR->papoBand[nIBand];
    MMRBand *pNextBand = hMMR->papoBand[nIBand + 1];

    // Two images with different numbers of columns are assigned to different subdatasets
    if (pThisBand->nWidth != pNextBand->nWidth)
        return true;

    // Two images with different numbers of rows are assigned to different subdatasets
    if (pThisBand->nHeight != pNextBand->nHeight)
        return true;

    // Two images with different resolution are assigned to different subdatasets
    if (pThisBand->GetPixelResolution() != pNextBand->GetPixelResolution())
        return true;

    // Two images with different bounding box are assigned to different subdatasets
    if (pThisBand->GetBoundingBoxMinX() != pNextBand->GetBoundingBoxMinX())
        return true;
    if (pThisBand->GetBoundingBoxMaxX() != pNextBand->GetBoundingBoxMaxX())
        return true;
    if (pThisBand->GetBoundingBoxMinY() != pNextBand->GetBoundingBoxMinY())
        return true;
    if (pThisBand->GetBoundingBoxMaxY() != pNextBand->GetBoundingBoxMaxY())
        return true;

    // One image has NoData values and the other does not;
    // they are assigned to different subdatasets
    if (pThisBand->bNoDataSet != pNextBand->bNoDataSet)
        return true;

    // Two images with different NoData values are assigned to different subdatasets
    if (pThisBand->dfNoData != pNextBand->dfNoData)
        return true;

    return false;
}

// Assigns every band to a subdataset
void MMRDataset::AssignBandsToSubdataSets()
{
    nNSubdataSets = 0;
    if (!hMMR->papoBand)
        return;

    nNSubdataSets = 1;
    int nIBand = 0;
    hMMR->papoBand[nIBand]->AssignSubDataSet(nNSubdataSets);
    for (; nIBand < hMMR->nBands - 1; nIBand++)
    {
        if (NextBandInANewDataSet(nIBand))
        {
            nNSubdataSets++;
            hMMR->papoBand[nIBand + 1]->AssignSubDataSet(nNSubdataSets);
        }
        else
            hMMR->papoBand[nIBand + 1]->AssignSubDataSet(nNSubdataSets);
    }

    // If there is only one subdataset, it means that
    // we don't need subdatasets (all assigned to 0)
    if (nNSubdataSets == 1)
    {
        nNSubdataSets = 0;
        for (nIBand = 0; nIBand < hMMR->nBands; nIBand++)
            hMMR->papoBand[nIBand]->AssignSubDataSet(nNSubdataSets);
    }
}

void MMRDataset::CreateSubdatasetsFromBands()
{
    CPLStringList oSubdatasetList;
    CPLString osDSName;
    CPLString osDSDesc;

    for (int iSubdataset = 1; iSubdataset <= nNSubdataSets; iSubdataset++)
    {
        int nIBand;
        for (nIBand = 0; nIBand < hMMR->nBands; nIBand++)
        {
            if (hMMR->papoBand[nIBand]->GetAssignedSubDataSet() == iSubdataset)
                break;
        }

        // ·$·TODO passar els noms a una funció que determini si calen cometes.
        osDSName.Printf("MiraMonRaster:\"%s\",\"%s\"",
                        hMMR->papoBand[nIBand]->GetRELFileName().c_str(),
                        hMMR->papoBand[nIBand]->GetRawBandFileName().c_str());
        osDSDesc.Printf("Subdataset %d: \"%s\"", iSubdataset,
                        hMMR->papoBand[nIBand]->GetBandName().c_str());
        nIBand++;

        for (; nIBand < hMMR->nBands; nIBand++)
        {
            if (hMMR->papoBand[nIBand]->GetAssignedSubDataSet() != iSubdataset)
                continue;

            osDSName.append(CPLSPrintf(
                ",\"%s\"",
                hMMR->papoBand[nIBand]->GetRawBandFileName().c_str()));
            osDSDesc.append(CPLSPrintf(
                ",\"%s\"", hMMR->papoBand[nIBand]->GetBandName().c_str()));
        }

        oSubdatasetList.AddNameValue(
            CPLSPrintf("SUBDATASET_%d_NAME", iSubdataset), osDSName);
        oSubdatasetList.AddNameValue(
            CPLSPrintf("SUBDATASET_%d_DESC", iSubdataset), osDSDesc);
    }

    if (oSubdatasetList.Count() > 0)
    {
        // Afegir al metadades del dataset principal
        SetMetadata(oSubdatasetList.List(), "SUBDATASETS");
        oSubdatasetList.Clear();
        bMetadataDirty = false;
    }
}

void MMRDataset::AssignBands(GDALOpenInfo *poOpenInfo)
{
    for (int nIBand = 0; nIBand < hMMR->nBands; nIBand++)
    {
        if (!hMMR->papoBand[nIBand])
            continue;  // It's impoosible, but...

        // Establish raster info.
        nRasterXSize = hMMR->papoBand[nIBand]->nWidth;
        nRasterYSize = hMMR->papoBand[nIBand]->nHeight;
        GetBandBoundingBox(nIBand);  // Fills adfGeoTransform for this band(s)
        SetBand(nBands + 1, new MMRRasterBand(this, nBands + 1));

        MMRRasterBand *poBand =
            static_cast<MMRRasterBand *>(GetRasterBand(nIBand + 1));

        if (!hMMR->papoBand[nIBand]->GetFriendlyDescription().empty())
        {
            poBand->SetMetadataItem(
                "DESCRIPTION",
                hMMR->papoBand[nIBand]->GetFriendlyDescription());
        }

        // Collect GDAL custom Metadata, and "auxiliary" metadata from
        // well known MMR structures for the bands.  We defer this till
        // now to ensure that the bands are properly setup before
        // interacting with PAM.
        //·$·TODO ens saltem aixo de moment.

        /*char **papszMD = MMRGetMetadata(hMMR, i + 1);
        if (papszMD != nullptr)
        {
            poBand->SetMetadata(papszMD);
            CSLDestroy(papszMD);
        }*/

        //poBand->ReadAuxMetadata();
        //poBand->ReadHistogramMetadata();
    }

    /*
    // Check for GDAL style metadata.
    char **papszMD = MMRGetMetadata(hMMR, 0);
    if (papszMD != nullptr)
    {
        SetMetadata(papszMD);
        CSLDestroy(papszMD);
    }

    // Read the elevation metadata, if present.
    for (int iBand = 0; iBand < nBands; iBand++)
    {
        MMRRasterBand *poBand =
            static_cast<MMRRasterBand *>(GetRasterBand(iBand + 1));
        const char *pszEU = MMRReadElevationUnit(hMMR, iBand);

        if (pszEU != nullptr)
        {
            poBand->SetUnitType(pszEU);
            if (nBands == 1)
            {
                SetMetadataItem("ELEVATION_UNITS", pszEU);
            }
        }
    }
    */

    // Initialize any PAM information.
    SetDescription(poOpenInfo->pszFilename);
    //TryLoadXML();

    // Clear dirty metadata flags.
    for (int i = 0; i < nBands; i++)
    {
        MMRRasterBand *poBand =
            static_cast<MMRRasterBand *>(GetRasterBand(i + 1));
        poBand->bMetadataDirty = false;
    }
    bMetadataDirty = false;
}
