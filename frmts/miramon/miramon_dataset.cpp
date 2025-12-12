/******************************************************************************
 *
 * Project:  MiraMonRaster driver
 * Purpose:  Implements MMRDataset class: responsible for generating the
 *           main dataset or the subdatasets as needed.
 * Author:   Abel Pau
 *
 ******************************************************************************
 * Copyright (c) 2025, Xavier Pons
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include <algorithm>

#include "miramon_dataset.h"
#include "miramon_rasterband.h"
#include "miramon_band.h"  // Per a MMRBand

#include "gdal_frmts.h"

#include "../miramon_common/mm_gdal_functions.h"  // For MMCheck_REL_FILE()

/************************************************************************/
/*                GDALRegister_MiraMon()                                */
/************************************************************************/
void GDALRegister_MiraMon()

{
    if (GDALGetDriverByName("MiraMonRaster") != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription("MiraMonRaster");
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "MiraMon Raster Images");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC,
                              "drivers/raster/miramon.html");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSIONS, "rel img");

    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_SUBDATASETS, "YES");

    poDriver->pfnOpen = MMRDataset::Open;
    poDriver->pfnCreateCopy = MMRDataset::CreateCopy;
    poDriver->pfnIdentify = MMRDataset::Identify;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}

/************************************************************************/
/*                            MMRDataset()                              */
/************************************************************************/
MMRDataset::MMRDataset(CPLString osRelname, GDALDataset &oSrcDS,
    bool bCompress, const CPLString osPattern)
    : m_bIsValid(false)
{
    nBands = oSrcDS.GetRasterCount();
    if (nBands == 0)
    {
        ReportError(osRelname, CE_Failure, CPLE_AppDefined,
                    "Unable to translate to MiraMon files with zero bands.");
        return;
    }

    // Saving the HRS in the layer structure
    const OGRSpatialReference *poSRS = oSrcDS.GetSpatialRef();
    if (poSRS)
    {
        const char *pszTargetKey = nullptr;
        const char *pszAuthorityName = nullptr;
        const char *pszAuthorityCode = nullptr;

        // Reading horizontal reference system and horizontal units
        if (poSRS->IsProjected())
            pszTargetKey = "PROJCS";
        else if (poSRS->IsGeographic() || poSRS->IsDerivedGeographic())
            pszTargetKey = "GEOGCS";
        else if (poSRS->IsGeocentric())
            pszTargetKey = "GEOCCS";
        else if (poSRS->IsLocal())
            pszTargetKey = "LOCAL_CS";

        if (!poSRS->IsLocal())
        {
            pszAuthorityName = poSRS->GetAuthorityName(pszTargetKey);
            pszAuthorityCode = poSRS->GetAuthorityCode(pszTargetKey);
        }

        if (pszAuthorityName && pszAuthorityCode &&
            EQUAL(pszAuthorityName, "EPSG"))
        {
            CPLDebugOnly("MiraMon", "Setting EPSG code %s", pszAuthorityCode);
            m_osEPSG = pszAuthorityCode;
        }
    }

    // Getting bands information
    std::vector<MMRBand> oBands{};
    oBands.reserve(nBands);

    // Getting bands information
    bool bAllBandsSameDim = true;
    for (int nIBand = 1; nIBand <= nBands; nIBand++)
    {
        GDALRasterBand *pRasterBand = oSrcDS.GetRasterBand(nIBand);
        if (!pRasterBand)
        {
            ReportError(
                osRelname, CE_Failure, CPLE_AppDefined,
                "Unable to translate the band %d to MiraMon. Process canceled.",
                nIBand);
            return;
        }

        const CPLString osIndexBand = CPLSPrintf("%d", nIBand);
        oBands.emplace_back(MMRBand(*pRasterBand, bCompress,
            osPattern, osIndexBand));
        if (!oBands.back().IsValid())
        {
            ReportError(
                osRelname, CE_Failure, CPLE_AppDefined,
                "Unable to translate the band %d to MiraMon. Process canceled.",
                nIBand);
            return;
        }
        if (nIBand == 1)
        {
            m_nWidth = oBands.back().GetWidth();
            m_nHeight = oBands.back().GetHeight();
        }
        else if (m_nWidth != oBands.back().GetWidth() ||
                 m_nHeight != oBands.back().GetHeight())
        {
            bAllBandsSameDim = false;
        }
    }

    // Getting number of columns and rows
    if (!bAllBandsSameDim)
    {
        // It's not an error. MiraMon have Datasets
        // with dimensions for each band
        m_nWidth = 0;
        m_nHeight = 0;
    }
    else
    {
        // Getting geotransform
        GDALGeoTransform gt;
        if (oSrcDS.GetGeoTransform(gt) == CE_None)
        {
            m_dfMinX = gt[0];
            m_dfMaxY = gt[3];
            m_dfMaxX = m_dfMinX + m_nWidth * gt[1];
            m_dfMinY = m_dfMaxY + m_nHeight * gt[5];
        }
    }

    // Getting REL information (and metadata stuff)
    auto pMMfRel = std::make_unique<MMRRel>(
        osRelname, m_osEPSG, m_nWidth, m_nHeight, m_dfMinX, m_dfMaxX, m_dfMinY,
        m_dfMaxY, std::move(oBands));

    if (!pMMfRel->IsValid())
        return;

    // Writing all information in files: I.rel, IMG,...
    if (!pMMfRel->Write())
    {
        pMMfRel->SetIsValid(false);
        return;
    }

    m_bIsValid = true;
}

MMRDataset::MMRDataset(GDALOpenInfo *poOpenInfo)
{
    m_oSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

    // Creating the class MMRRel.
    auto pMMfRel = std::make_unique<MMRRel>(poOpenInfo->pszFilename, true);
    if (!pMMfRel->IsValid())
    {
        if (pMMfRel->isAMiraMonFile())
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Unable to open %s, probably it's not a MiraMon file.",
                     poOpenInfo->pszFilename);
        }
        return;
    }

    if (pMMfRel->GetNBands() == 0)
    {
        if (pMMfRel->isAMiraMonFile())
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Unable to open %s, it has zero usable bands.",
                     poOpenInfo->pszFilename);
        }
        return;
    }

    m_pMMRRel = std::move(pMMfRel);

    // General Dataset information available
    nRasterXSize = m_pMMRRel->GetColumnsNumberFromREL();
    nRasterYSize = m_pMMRRel->GetRowsNumberFromREL();
    ReadProjection();
    nBands = 0;

    // Assign every band to a subdataset (if any)
    // If all bands should go to a one single Subdataset, then,
    // no subdataset will be created and all bands will go to this
    // dataset.
    AssignBandsToSubdataSets();

    // Create subdatasets or add bands, as needed
    if (m_nNSubdataSets)
    {
        CreateSubdatasetsFromBands();
        // Fills adfGeoTransform if documented
        UpdateGeoTransform();
    }
    else
    {
        if (!CreateRasterBands())
            return;

        // Fills adfGeoTransform if documented. If not, then gets one from last band.
        if (1 == UpdateGeoTransform())
        {
            MMRBand *poBand = m_pMMRRel->GetBand(m_pMMRRel->GetNBands() - 1);
            if (poBand)
                m_gt = poBand->m_gt;
        }
    }

    // Make sure we don't try to do any pam stuff with this dataset.
    nPamFlags |= GPF_NOSAVE;

    // We have a valid DataSet.
    m_bIsValid = true;
}

/************************************************************************/
/*                           ~MMRDataset()                              */
/************************************************************************/

MMRDataset::~MMRDataset()

{
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/
int MMRDataset::Identify(GDALOpenInfo *poOpenInfo)
{
    // Checking for subdataset
    int nIdentifyResult =
        MMRRel::IdentifySubdataSetFile(poOpenInfo->pszFilename);
    if (nIdentifyResult != GDAL_IDENTIFY_FALSE)
        return nIdentifyResult;

    // Checking for MiraMon raster file
    return MMRRel::IdentifyFile(poOpenInfo);
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/
GDALDataset *MMRDataset::Open(GDALOpenInfo *poOpenInfo)

{
    // Verify that this is a MMR file.
    if (!Identify(poOpenInfo))
        return nullptr;

    // Confirm the requested access is supported.
    if (poOpenInfo->eAccess == GA_Update)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "The MiraMonRaster driver does not support update "
                 "access to existing datasets.");
        return nullptr;
    }

    // Create the Dataset (with bands or Subdatasets).
    auto poDS = std::make_unique<MMRDataset>(poOpenInfo);
    if (!poDS->IsValid())
        return nullptr;

    // Set description
    poDS->SetDescription(poOpenInfo->pszFilename);

    return poDS.release();
}

/************************************************************************/
/*                             CreateCopy()                             */
/************************************************************************/
GDALDataset *MMRDataset::CreateCopy(const char *pszFilename,
                                    GDALDataset *poSrcDS, int bStrict,
                                    char **papszOptions,
                                    GDALProgressFunc pfnProgress,
                                    void *pProgressData)

{
    bool bCompress =
        EQUAL(CSLFetchNameValueDef(papszOptions, "COMPRESS", "YES"), "YES");

    // pszFilename doesn't have extension or must end in "I.rel"
    const CPLString osFileName = pszFilename;
    CPLString osRelName = CreateAssociatedMetadataFileName(osFileName);
    if (osRelName.empty())
        return nullptr;

    // osPattern is needed to create band names.
    CPLString osOptPattern =
        CSLFetchNameValueDef(papszOptions, "PATTERN", "");
    CPLString osPattern = CreatePatternFileName(osRelName, osOptPattern);

    if (osPattern.empty())
        osPattern=CPLGetBasenameSafe(osRelName);


    auto poDS = std::make_unique<MMRDataset>(osRelName, *poSrcDS, bCompress, osPattern);

    if (!poDS->IsValid())
        return nullptr;

    poDS->SetDescription(pszFilename);
    poDS->eAccess = GA_Update;

    return poDS.release();

#ifdef KK
    VSILFILE *l_fpL = nullptr;
    CPLString l_osTmpFilename;

    const int nXSize = poSrcDS->GetRasterXSize();
    const int nYSize = poSrcDS->GetRasterYSize();

    const int nColorTableMultiplier = std::max(
        1,
        std::min(257,
                 atoi(CSLFetchNameValueDef(
                     papszOptions, "COLOR_TABLE_MULTIPLIER",
                     CPLSPrintf("%d", DEFAULT_COLOR_TABLE_MULTIPLIER_257)))));

    bool bTileInterleaving = false;
    TIFF *l_hTIFF = CreateLL(pszFilename, nXSize, nYSize, l_nBands, eType,
                             dfExtraSpaceForOverviews, nColorTableMultiplier,
                             papszCreateOptions, &l_fpL, l_osTmpFilename,
                             /* bCreateCopy = */ true, bTileInterleaving);
    const bool bStreaming = !l_osTmpFilename.empty();

    CSLDestroy(papszCreateOptions);
    papszCreateOptions = nullptr;

    if (l_hTIFF == nullptr)
    {
        if (bStreaming)
            VSIUnlink(l_osTmpFilename);
        return nullptr;
    }

    uint16_t l_nPlanarConfig = 0;
    TIFFGetField(l_hTIFF, TIFFTAG_PLANARCONFIG, &l_nPlanarConfig);

    uint16_t l_nCompression = 0;

    if (!TIFFGetField(l_hTIFF, TIFFTAG_COMPRESSION, &(l_nCompression)))
        l_nCompression = COMPRESSION_NONE;

    /* -------------------------------------------------------------------- */
    /*      Set the alpha channel if we find one.                           */
    /* -------------------------------------------------------------------- */
    uint16_t *extraSamples = nullptr;
    uint16_t nExtraSamples = 0;
    if (TIFFGetField(l_hTIFF, TIFFTAG_EXTRASAMPLES, &nExtraSamples,
                     &extraSamples) &&
        nExtraSamples > 0)
    {
        // We need to allocate a new array as (current) libtiff
        // versions will not like that we reuse the array we got from
        // TIFFGetField().
        uint16_t *pasNewExtraSamples = static_cast<uint16_t *>(
            CPLMalloc(nExtraSamples * sizeof(uint16_t)));
        memcpy(pasNewExtraSamples, extraSamples,
               nExtraSamples * sizeof(uint16_t));
        const char *pszAlpha = CPLGetConfigOption(
            "GTIFF_ALPHA", CSLFetchNameValue(papszOptions, "ALPHA"));
        const uint16_t nAlpha =
            GTiffGetAlphaValue(pszAlpha, DEFAULT_ALPHA_TYPE);
        const int nBaseSamples = l_nBands - nExtraSamples;
        for (int iExtraBand = nBaseSamples + 1; iExtraBand <= l_nBands;
             iExtraBand++)
        {
            if (poSrcDS->GetRasterBand(iExtraBand)->GetColorInterpretation() ==
                GCI_AlphaBand)
            {
                pasNewExtraSamples[iExtraBand - nBaseSamples - 1] = nAlpha;
                if (!pszAlpha)
                {
                    // Use the ALPHA metadata item from the source band, when
                    // present, if no explicit ALPHA creation option
                    pasNewExtraSamples[iExtraBand - nBaseSamples - 1] =
                        GTiffGetAlphaValue(
                            poSrcDS->GetRasterBand(iExtraBand)
                                ->GetMetadataItem("ALPHA", "IMAGE_STRUCTURE"),
                            nAlpha);
                }
            }
        }
        TIFFSetField(l_hTIFF, TIFFTAG_EXTRASAMPLES, nExtraSamples,
                     pasNewExtraSamples);

        CPLFree(pasNewExtraSamples);
    }

    /* -------------------------------------------------------------------- */
    /*      If the output is jpeg compressed, and the input is RGB make     */
    /*      sure we note that.                                              */
    /* -------------------------------------------------------------------- */

    if (l_nCompression == COMPRESSION_JPEG)
    {
        if (l_nBands >= 3 &&
            (poSrcDS->GetRasterBand(1)->GetColorInterpretation() ==
             GCI_YCbCr_YBand) &&
            (poSrcDS->GetRasterBand(2)->GetColorInterpretation() ==
             GCI_YCbCr_CbBand) &&
            (poSrcDS->GetRasterBand(3)->GetColorInterpretation() ==
             GCI_YCbCr_CrBand))
        {
            // Do nothing.
        }
        else
        {
            // Assume RGB if it is not explicitly YCbCr.
            CPLDebug("GTiff", "Setting JPEGCOLORMODE_RGB");
            TIFFSetField(l_hTIFF, TIFFTAG_JPEGCOLORMODE, JPEGCOLORMODE_RGB);
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Does the source image consist of one band, with a palette?      */
    /*      If so, copy over.                                               */
    /* -------------------------------------------------------------------- */
    if ((l_nBands == 1 || l_nBands == 2) &&
        poSrcDS->GetRasterBand(1)->GetColorTable() != nullptr &&
        eType == GDT_Byte)
    {
        unsigned short anTRed[256] = {0};
        unsigned short anTGreen[256] = {0};
        unsigned short anTBlue[256] = {0};
        GDALColorTable *poCT = poSrcDS->GetRasterBand(1)->GetColorTable();

        for (int iColor = 0; iColor < 256; ++iColor)
        {
            if (iColor < poCT->GetColorEntryCount())
            {
                GDALColorEntry sRGB = {0, 0, 0, 0};

                poCT->GetColorEntryAsRGB(iColor, &sRGB);

                anTRed[iColor] = GTiffDataset::ClampCTEntry(
                    iColor, 1, sRGB.c1, nColorTableMultiplier);
                anTGreen[iColor] = GTiffDataset::ClampCTEntry(
                    iColor, 2, sRGB.c2, nColorTableMultiplier);
                anTBlue[iColor] = GTiffDataset::ClampCTEntry(
                    iColor, 3, sRGB.c3, nColorTableMultiplier);
            }
            else
            {
                anTRed[iColor] = 0;
                anTGreen[iColor] = 0;
                anTBlue[iColor] = 0;
            }
        }

        if (!bForcePhotometric)
            TIFFSetField(l_hTIFF, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_PALETTE);
        TIFFSetField(l_hTIFF, TIFFTAG_COLORMAP, anTRed, anTGreen, anTBlue);
    }
    else if ((l_nBands == 1 || l_nBands == 2) &&
             poSrcDS->GetRasterBand(1)->GetColorTable() != nullptr &&
             eType == GDT_UInt16)
    {
        unsigned short *panTRed = static_cast<unsigned short *>(
            CPLMalloc(65536 * sizeof(unsigned short)));
        unsigned short *panTGreen = static_cast<unsigned short *>(
            CPLMalloc(65536 * sizeof(unsigned short)));
        unsigned short *panTBlue = static_cast<unsigned short *>(
            CPLMalloc(65536 * sizeof(unsigned short)));

        GDALColorTable *poCT = poSrcDS->GetRasterBand(1)->GetColorTable();

        for (int iColor = 0; iColor < 65536; ++iColor)
        {
            if (iColor < poCT->GetColorEntryCount())
            {
                GDALColorEntry sRGB = {0, 0, 0, 0};

                poCT->GetColorEntryAsRGB(iColor, &sRGB);

                panTRed[iColor] = GTiffDataset::ClampCTEntry(
                    iColor, 1, sRGB.c1, nColorTableMultiplier);
                panTGreen[iColor] = GTiffDataset::ClampCTEntry(
                    iColor, 2, sRGB.c2, nColorTableMultiplier);
                panTBlue[iColor] = GTiffDataset::ClampCTEntry(
                    iColor, 3, sRGB.c3, nColorTableMultiplier);
            }
            else
            {
                panTRed[iColor] = 0;
                panTGreen[iColor] = 0;
                panTBlue[iColor] = 0;
            }
        }

        if (!bForcePhotometric)
            TIFFSetField(l_hTIFF, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_PALETTE);
        TIFFSetField(l_hTIFF, TIFFTAG_COLORMAP, panTRed, panTGreen, panTBlue);

        CPLFree(panTRed);
        CPLFree(panTGreen);
        CPLFree(panTBlue);
    }
    else if (poSrcDS->GetRasterBand(1)->GetColorTable() != nullptr)
        ReportError(
            pszFilename, CE_Failure, CPLE_AppDefined,
            "Unable to export color table to GeoTIFF file.  Color tables "
            "can only be written to 1 band or 2 bands Byte or "
            "UInt16 GeoTIFF files.");

    if (l_nCompression == COMPRESSION_JPEG)
    {
        uint16_t l_nPhotometric = 0;
        TIFFGetField(l_hTIFF, TIFFTAG_PHOTOMETRIC, &l_nPhotometric);
        // Check done in tif_jpeg.c later, but not with a very clear error
        // message
        if (l_nPhotometric == PHOTOMETRIC_PALETTE)
        {
            ReportError(pszFilename, CE_Failure, CPLE_NotSupported,
                        "JPEG compression not supported with paletted image");
            XTIFFClose(l_hTIFF);
            VSIUnlink(l_osTmpFilename);
            CPL_IGNORE_RET_VAL(VSIFCloseL(l_fpL));
            return nullptr;
        }
    }

    if (l_nBands == 2 &&
        poSrcDS->GetRasterBand(1)->GetColorTable() != nullptr &&
        (eType == GDT_Byte || eType == GDT_UInt16))
    {
        uint16_t v[1] = {EXTRASAMPLE_UNASSALPHA};

        TIFFSetField(l_hTIFF, TIFFTAG_EXTRASAMPLES, 1, v);
    }

    const int nMaskFlags = poSrcDS->GetRasterBand(1)->GetMaskFlags();
    bool bCreateMask = false;
    CPLString osHiddenStructuralMD;
    const char *pszInterleave =
        CSLFetchNameValueDef(papszOptions, "INTERLEAVE", "PIXEL");
    if (bCopySrcOverviews &&
        CPLTestBool(CSLFetchNameValueDef(papszOptions, "TILED", "NO")))
    {
        osHiddenStructuralMD += "LAYOUT=IFDS_BEFORE_DATA\n";
        osHiddenStructuralMD += "BLOCK_ORDER=ROW_MAJOR\n";
        osHiddenStructuralMD += "BLOCK_LEADER=SIZE_AS_UINT4\n";
        osHiddenStructuralMD += "BLOCK_TRAILER=LAST_4_BYTES_REPEATED\n";
        if (l_nBands > 1 && !EQUAL(pszInterleave, "PIXEL"))
        {
            osHiddenStructuralMD += "INTERLEAVE=";
            osHiddenStructuralMD += CPLString(pszInterleave).toupper();
            osHiddenStructuralMD += "\n";
        }
        osHiddenStructuralMD +=
            "KNOWN_INCOMPATIBLE_EDITION=NO\n ";  // Final space intended, so
                                                 // this can be replaced by YES
    }
    if (!(nMaskFlags & (GMF_ALL_VALID | GMF_ALPHA | GMF_NODATA)) &&
        (nMaskFlags & GMF_PER_DATASET) && !bStreaming)
    {
        bCreateMask = true;
        if (GTiffDataset::MustCreateInternalMask() &&
            !osHiddenStructuralMD.empty() && EQUAL(pszInterleave, "PIXEL"))
        {
            osHiddenStructuralMD += "MASK_INTERLEAVED_WITH_IMAGERY=YES\n";
        }
    }
    if (!osHiddenStructuralMD.empty())
    {
        const int nHiddenMDSize = static_cast<int>(osHiddenStructuralMD.size());
        osHiddenStructuralMD =
            CPLOPrintf("GDAL_STRUCTURAL_METADATA_SIZE=%06d bytes\n",
                       nHiddenMDSize) +
            osHiddenStructuralMD;
        VSI_TIFFWrite(l_hTIFF, osHiddenStructuralMD.c_str(),
                      osHiddenStructuralMD.size());
    }

    // FIXME? libtiff writes extended tags in the order they are specified
    // and not in increasing order.

    /* -------------------------------------------------------------------- */
    /*      Transfer some TIFF specific metadata, if available.             */
    /*      The return value will tell us if we need to try again later with*/
    /*      PAM because the profile doesn't allow to write some metadata    */
    /*      as TIFF tag                                                     */
    /* -------------------------------------------------------------------- */
    const bool bHasWrittenMDInGeotiffTAG = GTiffDataset::WriteMetadata(
        poSrcDS, l_hTIFF, false, eProfile, pszFilename, papszOptions);

    /* -------------------------------------------------------------------- */
    /*      Write NoData value, if exist.                                   */
    /* -------------------------------------------------------------------- */
    if (eProfile == GTiffProfile::GDALGEOTIFF)
    {
        int bSuccess = FALSE;
        GDALRasterBand *poFirstBand = poSrcDS->GetRasterBand(1);
        if (poFirstBand->GetRasterDataType() == GDT_Int64)
        {
            const auto nNoData = poFirstBand->GetNoDataValueAsInt64(&bSuccess);
            if (bSuccess)
                GTiffDataset::WriteNoDataValue(l_hTIFF, nNoData);
        }
        else if (poFirstBand->GetRasterDataType() == GDT_UInt64)
        {
            const auto nNoData = poFirstBand->GetNoDataValueAsUInt64(&bSuccess);
            if (bSuccess)
                GTiffDataset::WriteNoDataValue(l_hTIFF, nNoData);
        }
        else
        {
            const auto dfNoData = poFirstBand->GetNoDataValue(&bSuccess);
            if (bSuccess)
                GTiffDataset::WriteNoDataValue(l_hTIFF, dfNoData);
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Are we addressing PixelIsPoint mode?                            */
    /* -------------------------------------------------------------------- */
    bool bPixelIsPoint = false;
    bool bPointGeoIgnore = false;

    if (poSrcDS->GetMetadataItem(GDALMD_AREA_OR_POINT) &&
        EQUAL(poSrcDS->GetMetadataItem(GDALMD_AREA_OR_POINT), GDALMD_AOP_POINT))
    {
        bPixelIsPoint = true;
        bPointGeoIgnore =
            CPLTestBool(CPLGetConfigOption("GTIFF_POINT_GEO_IGNORE", "FALSE"));
    }

    /* -------------------------------------------------------------------- */
    /*      Write affine transform if it is meaningful.                     */
    /* -------------------------------------------------------------------- */
    const OGRSpatialReference *l_poSRS = nullptr;
    GDALGeoTransform l_gt;
    if (poSrcDS->GetGeoTransform(l_gt) == CE_None)
    {
        if (bGeoTIFF)
        {
            l_poSRS = poSrcDS->GetSpatialRef();

            if (l_gt[2] == 0.0 && l_gt[4] == 0.0 && l_gt[5] < 0.0)
            {
                double dfOffset = 0.0;
                {
                    // In the case the SRS has a vertical component and we have
                    // a single band, encode its scale/offset in the GeoTIFF
                    // tags
                    int bHasScale = FALSE;
                    double dfScale =
                        poSrcDS->GetRasterBand(1)->GetScale(&bHasScale);
                    int bHasOffset = FALSE;
                    dfOffset =
                        poSrcDS->GetRasterBand(1)->GetOffset(&bHasOffset);
                    const bool bApplyScaleOffset =
                        l_poSRS && l_poSRS->IsVertical() &&
                        poSrcDS->GetRasterCount() == 1;
                    if (bApplyScaleOffset && !bHasScale)
                        dfScale = 1.0;
                    if (!bApplyScaleOffset || !bHasOffset)
                        dfOffset = 0.0;
                    const double adfPixelScale[3] = {l_gt[1], fabs(l_gt[5]),
                                                     bApplyScaleOffset ? dfScale
                                                                       : 0.0};

                    TIFFSetField(l_hTIFF, TIFFTAG_GEOPIXELSCALE, 3,
                                 adfPixelScale);
                }

                double adfTiePoints[6] = {0.0,     0.0,     0.0,
                                          l_gt[0], l_gt[3], dfOffset};

                if (bPixelIsPoint && !bPointGeoIgnore)
                {
                    adfTiePoints[3] += l_gt[1] * 0.5 + l_gt[2] * 0.5;
                    adfTiePoints[4] += l_gt[4] * 0.5 + l_gt[5] * 0.5;
                }

                TIFFSetField(l_hTIFF, TIFFTAG_GEOTIEPOINTS, 6, adfTiePoints);
            }
            else
            {
                double adfMatrix[16] = {0.0};

                adfMatrix[0] = l_gt[1];
                adfMatrix[1] = l_gt[2];
                adfMatrix[3] = l_gt[0];
                adfMatrix[4] = l_gt[4];
                adfMatrix[5] = l_gt[5];
                adfMatrix[7] = l_gt[3];
                adfMatrix[15] = 1.0;

                if (bPixelIsPoint && !bPointGeoIgnore)
                {
                    adfMatrix[3] += l_gt[1] * 0.5 + l_gt[2] * 0.5;
                    adfMatrix[7] += l_gt[4] * 0.5 + l_gt[5] * 0.5;
                }

                TIFFSetField(l_hTIFF, TIFFTAG_GEOTRANSMATRIX, 16, adfMatrix);
            }
        }

        /* --------------------------------------------------------------------
         */
        /*      Do we need a TFW file? */
        /* --------------------------------------------------------------------
         */
        if (CPLFetchBool(papszOptions, "TFW", false))
            GDALWriteWorldFile(pszFilename, "tfw", l_gt.data());
        else if (CPLFetchBool(papszOptions, "WORLDFILE", false))
            GDALWriteWorldFile(pszFilename, "wld", l_gt.data());
    }

    /* -------------------------------------------------------------------- */
    /*      Otherwise write tiepoints if they are available.                */
    /* -------------------------------------------------------------------- */
    else if (poSrcDS->GetGCPCount() > 0 && bGeoTIFF)
    {
        const GDAL_GCP *pasGCPs = poSrcDS->GetGCPs();
        double *padfTiePoints = static_cast<double *>(
            CPLMalloc(6 * sizeof(double) * poSrcDS->GetGCPCount()));

        for (int iGCP = 0; iGCP < poSrcDS->GetGCPCount(); ++iGCP)
        {

            padfTiePoints[iGCP * 6 + 0] = pasGCPs[iGCP].dfGCPPixel;
            padfTiePoints[iGCP * 6 + 1] = pasGCPs[iGCP].dfGCPLine;
            padfTiePoints[iGCP * 6 + 2] = 0;
            padfTiePoints[iGCP * 6 + 3] = pasGCPs[iGCP].dfGCPX;
            padfTiePoints[iGCP * 6 + 4] = pasGCPs[iGCP].dfGCPY;
            padfTiePoints[iGCP * 6 + 5] = pasGCPs[iGCP].dfGCPZ;

            if (bPixelIsPoint && !bPointGeoIgnore)
            {
                padfTiePoints[iGCP * 6 + 0] -= 0.5;
                padfTiePoints[iGCP * 6 + 1] -= 0.5;
            }
        }

        TIFFSetField(l_hTIFF, TIFFTAG_GEOTIEPOINTS, 6 * poSrcDS->GetGCPCount(),
                     padfTiePoints);
        CPLFree(padfTiePoints);

        l_poSRS = poSrcDS->GetGCPSpatialRef();

        if (CPLFetchBool(papszOptions, "TFW", false) ||
            CPLFetchBool(papszOptions, "WORLDFILE", false))
        {
            ReportError(
                pszFilename, CE_Warning, CPLE_AppDefined,
                "TFW=ON or WORLDFILE=ON creation options are ignored when "
                "GCPs are available");
        }
    }
    else
    {
        l_poSRS = poSrcDS->GetSpatialRef();
    }

    /* -------------------------------------------------------------------- */
    /*      Copy xml:XMP data                                               */
    /* -------------------------------------------------------------------- */
    char **papszXMP = poSrcDS->GetMetadata("xml:XMP");
    if (papszXMP != nullptr && *papszXMP != nullptr)
    {
        int nTagSize = static_cast<int>(strlen(*papszXMP));
        TIFFSetField(l_hTIFF, TIFFTAG_XMLPACKET, nTagSize, *papszXMP);
    }

    /* -------------------------------------------------------------------- */
    /*      Write the projection information, if possible.                  */
    /* -------------------------------------------------------------------- */
    const bool bHasProjection = l_poSRS != nullptr;
    bool bExportSRSToPAM = false;
    if ((bHasProjection || bPixelIsPoint) && bGeoTIFF)
    {
        GTIF *psGTIF = GTiffDataset::GTIFNew(l_hTIFF);

        if (bHasProjection)
        {
            const auto eGeoTIFFKeysFlavor = GetGTIFFKeysFlavor(papszOptions);
            if (IsSRSCompatibleOfGeoTIFF(l_poSRS, eGeoTIFFKeysFlavor))
            {
                GTIFSetFromOGISDefnEx(
                    psGTIF,
                    OGRSpatialReference::ToHandle(
                        const_cast<OGRSpatialReference *>(l_poSRS)),
                    eGeoTIFFKeysFlavor, GetGeoTIFFVersion(papszOptions));
            }
            else
            {
                bExportSRSToPAM = true;
            }
        }

        if (bPixelIsPoint)
        {
            GTIFKeySet(psGTIF, GTRasterTypeGeoKey, TYPE_SHORT, 1,
                       RasterPixelIsPoint);
        }

        GTIFWriteKeys(psGTIF);
        GTIFFree(psGTIF);
    }

    bool l_bDontReloadFirstBlock = false;

#ifdef HAVE_LIBJPEG
    if (bCopyFromJPEG)
    {
        GTIFF_CopyFromJPEG_WriteAdditionalTags(l_hTIFF, poSrcDS);
    }
#endif

    /* -------------------------------------------------------------------- */
    /*      Cleanup                                                         */
    /* -------------------------------------------------------------------- */
    if (bCopySrcOverviews)
    {
        TIFFDeferStrileArrayWriting(l_hTIFF);
    }
    TIFFWriteCheck(l_hTIFF, TIFFIsTiled(l_hTIFF), "GTiffCreateCopy()");
    TIFFWriteDirectory(l_hTIFF);
    if (bStreaming)
    {
        // We need to write twice the directory to be sure that custom
        // TIFF tags are correctly sorted and that padding bytes have been
        // added.
        TIFFSetDirectory(l_hTIFF, 0);
        TIFFWriteDirectory(l_hTIFF);

        if (VSIFSeekL(l_fpL, 0, SEEK_END) != 0)
            ReportError(pszFilename, CE_Failure, CPLE_FileIO, "Cannot seek");
        const int nSize = static_cast<int>(VSIFTellL(l_fpL));

        vsi_l_offset nDataLength = 0;
        VSIGetMemFileBuffer(l_osTmpFilename, &nDataLength, FALSE);
        TIFFSetDirectory(l_hTIFF, 0);
        GTiffFillStreamableOffsetAndCount(l_hTIFF, nSize);
        TIFFWriteDirectory(l_hTIFF);
    }
    const auto nDirCount = TIFFNumberOfDirectories(l_hTIFF);
    if (nDirCount >= 1)
    {
        TIFFSetDirectory(l_hTIFF, static_cast<tdir_t>(nDirCount - 1));
    }
    const toff_t l_nDirOffset = TIFFCurrentDirOffset(l_hTIFF);
    TIFFFlush(l_hTIFF);
    XTIFFClose(l_hTIFF);

    VSIFSeekL(l_fpL, 0, SEEK_SET);

    // fpStreaming will assigned to the instance and not closed here.
    VSILFILE *fpStreaming = nullptr;
    if (bStreaming)
    {
        vsi_l_offset nDataLength = 0;
        void *pabyBuffer =
            VSIGetMemFileBuffer(l_osTmpFilename, &nDataLength, FALSE);
        fpStreaming = VSIFOpenL(pszFilename, "wb");
        if (fpStreaming == nullptr)
        {
            VSIUnlink(l_osTmpFilename);
            CPL_IGNORE_RET_VAL(VSIFCloseL(l_fpL));
            return nullptr;
        }
        if (static_cast<vsi_l_offset>(VSIFWriteL(pabyBuffer, 1,
                                                 static_cast<int>(nDataLength),
                                                 fpStreaming)) != nDataLength)
        {
            ReportError(pszFilename, CE_Failure, CPLE_FileIO,
                        "Could not write %d bytes",
                        static_cast<int>(nDataLength));
            CPL_IGNORE_RET_VAL(VSIFCloseL(fpStreaming));
            VSIUnlink(l_osTmpFilename);
            CPL_IGNORE_RET_VAL(VSIFCloseL(l_fpL));
            return nullptr;
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Re-open as a dataset and copy over missing metadata using       */
    /*      PAM facilities.                                                 */
    /* -------------------------------------------------------------------- */
    l_hTIFF = VSI_TIFFOpen(bStreaming ? l_osTmpFilename.c_str() : pszFilename,
                           "r+", l_fpL);
    if (l_hTIFF == nullptr)
    {
        if (bStreaming)
            VSIUnlink(l_osTmpFilename);
        l_fpL->CancelCreation();
        CPL_IGNORE_RET_VAL(VSIFCloseL(l_fpL));
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Create a corresponding GDALDataset.                             */
    /* -------------------------------------------------------------------- */
    GTiffDataset *poDS = new GTiffDataset();
    const bool bSuppressASAP =
        CPLTestBool(CSLFetchNameValueDef(papszOptions, "@SUPPRESS_ASAP", "NO"));
    if (bSuppressASAP)
        poDS->MarkSuppressOnClose();
    poDS->SetDescription(pszFilename);
    poDS->eAccess = GA_Update;
    poDS->m_osFilename = pszFilename;
    poDS->m_fpL = l_fpL;
    poDS->m_bIMDRPCMetadataLoaded = true;
    poDS->m_nColorTableMultiplier = nColorTableMultiplier;
    poDS->m_bTileInterleave = bTileInterleaving;

    if (bTileInterleaving)
    {
        poDS->m_oGTiffMDMD.SetMetadataItem("INTERLEAVE", "TILE",
                                           "IMAGE_STRUCTURE");
    }

    const bool bAppend = CPLFetchBool(papszOptions, "APPEND_SUBDATASET", false);
    if (poDS->OpenOffset(l_hTIFF,
                         bAppend ? l_nDirOffset : TIFFCurrentDirOffset(l_hTIFF),
                         GA_Update,
                         false,  // bAllowRGBAInterface
                         true    // bReadGeoTransform
                         ) != CE_None)
    {
        l_fpL->CancelCreation();
        delete poDS;
        if (bStreaming)
            VSIUnlink(l_osTmpFilename);
        return nullptr;
    }

    // Legacy... Patch back GDT_Int8 type to GDT_Byte if the user used
    // PIXELTYPE=SIGNEDBYTE
    const char *pszPixelType = CSLFetchNameValue(papszOptions, "PIXELTYPE");
    if (pszPixelType == nullptr)
        pszPixelType = "";
    if (eType == GDT_Byte && EQUAL(pszPixelType, "SIGNEDBYTE"))
    {
        for (int i = 0; i < poDS->nBands; ++i)
        {
            auto poBand = static_cast<GTiffRasterBand *>(poDS->papoBands[i]);
            poBand->eDataType = GDT_Byte;
            poBand->EnablePixelTypeSignedByteWarning(false);
            poBand->SetMetadataItem("PIXELTYPE", "SIGNEDBYTE",
                                    "IMAGE_STRUCTURE");
            poBand->EnablePixelTypeSignedByteWarning(true);
        }
    }

    poDS->oOvManager.Initialize(poDS, pszFilename);

    if (bStreaming)
    {
        VSIUnlink(l_osTmpFilename);
        poDS->m_fpToWrite = fpStreaming;
    }
    poDS->m_eProfile = eProfile;

    int nCloneInfoFlags = GCIF_PAM_DEFAULT & ~GCIF_MASK;

    // If we explicitly asked not to tag the alpha band as such, do not
    // reintroduce this alpha color interpretation in PAM.
    if (poSrcDS->GetRasterBand(l_nBands)->GetColorInterpretation() ==
            GCI_AlphaBand &&
        GTiffGetAlphaValue(
            CPLGetConfigOption("GTIFF_ALPHA",
                               CSLFetchNameValue(papszOptions, "ALPHA")),
            DEFAULT_ALPHA_TYPE) == EXTRASAMPLE_UNSPECIFIED)
    {
        nCloneInfoFlags &= ~GCIF_COLORINTERP;
    }
    // Ignore source band color interpretation if requesting PHOTOMETRIC=RGB
    else if (l_nBands >= 3 &&
             EQUAL(CSLFetchNameValueDef(papszOptions, "PHOTOMETRIC", ""),
                   "RGB"))
    {
        for (int i = 1; i <= 3; i++)
        {
            poDS->GetRasterBand(i)->SetColorInterpretation(
                static_cast<GDALColorInterp>(GCI_RedBand + (i - 1)));
        }
        nCloneInfoFlags &= ~GCIF_COLORINTERP;
        if (!(l_nBands == 4 &&
              CSLFetchNameValue(papszOptions, "ALPHA") != nullptr))
        {
            for (int i = 4; i <= l_nBands; i++)
            {
                poDS->GetRasterBand(i)->SetColorInterpretation(
                    poSrcDS->GetRasterBand(i)->GetColorInterpretation());
            }
        }
    }

    CPLString osOldGTIFF_REPORT_COMPD_CSVal(
        CPLGetConfigOption("GTIFF_REPORT_COMPD_CS", ""));
    CPLSetThreadLocalConfigOption("GTIFF_REPORT_COMPD_CS", "YES");
    poDS->CloneInfo(poSrcDS, nCloneInfoFlags);
    CPLSetThreadLocalConfigOption("GTIFF_REPORT_COMPD_CS",
                                  osOldGTIFF_REPORT_COMPD_CSVal.empty()
                                      ? nullptr
                                      : osOldGTIFF_REPORT_COMPD_CSVal.c_str());

    if ((!bGeoTIFF || bExportSRSToPAM) &&
        (poDS->GetPamFlags() & GPF_DISABLED) == 0)
    {
        // Copy georeferencing info to PAM if the profile is not GeoTIFF
        poDS->GDALPamDataset::SetSpatialRef(poDS->GetSpatialRef());
        GDALGeoTransform gt;
        if (poDS->GetGeoTransform(gt) == CE_None)
        {
            poDS->GDALPamDataset::SetGeoTransform(gt);
        }
        poDS->GDALPamDataset::SetGCPs(poDS->GetGCPCount(), poDS->GetGCPs(),
                                      poDS->GetGCPSpatialRef());
    }

    poDS->m_papszCreationOptions = CSLDuplicate(papszOptions);
    poDS->m_bDontReloadFirstBlock = l_bDontReloadFirstBlock;

    /* -------------------------------------------------------------------- */
    /*      CloneInfo() does not merge metadata, it just replaces it        */
    /*      totally.  So we have to merge it.                               */
    /* -------------------------------------------------------------------- */

    char **papszSRC_MD = poSrcDS->GetMetadata();
    char **papszDST_MD = CSLDuplicate(poDS->GetMetadata());

    papszDST_MD = CSLMerge(papszDST_MD, papszSRC_MD);

    poDS->SetMetadata(papszDST_MD);
    CSLDestroy(papszDST_MD);

    // Depending on the PHOTOMETRIC tag, the TIFF file may not have the same
    // band count as the source. Will fail later in GDALDatasetCopyWholeRaster
    // anyway.
    for (int nBand = 1;
         nBand <= std::min(poDS->GetRasterCount(), poSrcDS->GetRasterCount());
         ++nBand)
    {
        GDALRasterBand *poSrcBand = poSrcDS->GetRasterBand(nBand);
        GDALRasterBand *poDstBand = poDS->GetRasterBand(nBand);
        papszSRC_MD = poSrcBand->GetMetadata();
        papszDST_MD = CSLDuplicate(poDstBand->GetMetadata());

        papszDST_MD = CSLMerge(papszDST_MD, papszSRC_MD);

        poDstBand->SetMetadata(papszDST_MD);
        CSLDestroy(papszDST_MD);

        char **papszCatNames = poSrcBand->GetCategoryNames();
        if (nullptr != papszCatNames)
            poDstBand->SetCategoryNames(papszCatNames);
    }

    l_hTIFF = static_cast<TIFF *>(poDS->GetInternalHandle("TIFF_HANDLE"));

    /* -------------------------------------------------------------------- */
    /*      Handle forcing xml:ESRI data to be written to PAM.              */
    /* -------------------------------------------------------------------- */
    if (CPLTestBool(CPLGetConfigOption("ESRI_XML_PAM", "NO")))
    {
        char **papszESRIMD = poSrcDS->GetMetadata("xml:ESRI");
        if (papszESRIMD)
        {
            poDS->SetMetadata(papszESRIMD, "xml:ESRI");
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Second chance: now that we have a PAM dataset, it is possible   */
    /*      to write metadata that we could not write as a TIFF tag.        */
    /* -------------------------------------------------------------------- */
    if (!bHasWrittenMDInGeotiffTAG && !bStreaming)
    {
        GTiffDataset::WriteMetadata(
            poDS, l_hTIFF, true, eProfile, pszFilename, papszOptions,
            true /* don't write RPC and IMD file again */);
    }

    if (!bStreaming)
        GTiffDataset::WriteRPC(poDS, l_hTIFF, true, eProfile, pszFilename,
                               papszOptions,
                               true /* write only in PAM AND if needed */);

    poDS->m_bWriteCOGLayout = bCopySrcOverviews;

    // To avoid unnecessary directory rewriting.
    poDS->m_bMetadataChanged = false;
    poDS->m_bGeoTIFFInfoChanged = false;
    poDS->m_bNoDataChanged = false;
    poDS->m_bForceUnsetGTOrGCPs = false;
    poDS->m_bForceUnsetProjection = false;
    poDS->m_bStreamingOut = bStreaming;

    // Don't try to load external metadata files (#6597).
    poDS->m_bIMDRPCMetadataLoaded = true;

    // We must re-set the compression level at this point, since it has been
    // lost a few lines above when closing the newly create TIFF file The
    // TIFFTAG_ZIPQUALITY & TIFFTAG_JPEGQUALITY are not store in the TIFF file.
    // They are just TIFF session parameters.

    poDS->m_nZLevel = GTiffGetZLevel(papszOptions);
    poDS->m_nLZMAPreset = GTiffGetLZMAPreset(papszOptions);
    poDS->m_nZSTDLevel = GTiffGetZSTDPreset(papszOptions);
    poDS->m_nWebPLevel = GTiffGetWebPLevel(papszOptions);
    poDS->m_bWebPLossless = GTiffGetWebPLossless(papszOptions);
    if (poDS->m_nWebPLevel != 100 && poDS->m_bWebPLossless &&
        CSLFetchNameValue(papszOptions, "WEBP_LEVEL"))
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "WEBP_LEVEL is specified, but WEBP_LOSSLESS=YES. "
                 "WEBP_LEVEL will be ignored.");
    }
    poDS->m_nJpegQuality = GTiffGetJpegQuality(papszOptions);
    poDS->m_nJpegTablesMode = GTiffGetJpegTablesMode(papszOptions);
    poDS->GetDiscardLsbOption(papszOptions);
    poDS->m_dfMaxZError = GTiffGetLERCMaxZError(papszOptions);
    poDS->m_dfMaxZErrorOverview = GTiffGetLERCMaxZErrorOverview(papszOptions);
#if HAVE_JXL
    poDS->m_bJXLLossless = GTiffGetJXLLossless(papszOptions);
    poDS->m_nJXLEffort = GTiffGetJXLEffort(papszOptions);
    poDS->m_fJXLDistance = GTiffGetJXLDistance(papszOptions);
    poDS->m_fJXLAlphaDistance = GTiffGetJXLAlphaDistance(papszOptions);
#endif
    poDS->InitCreationOrOpenOptions(true, papszOptions);

    if (l_nCompression == COMPRESSION_ADOBE_DEFLATE ||
        l_nCompression == COMPRESSION_LERC)
    {
        GTiffSetDeflateSubCodec(l_hTIFF);

        if (poDS->m_nZLevel != -1)
        {
            TIFFSetField(l_hTIFF, TIFFTAG_ZIPQUALITY, poDS->m_nZLevel);
        }
    }
    if (l_nCompression == COMPRESSION_JPEG)
    {
        if (poDS->m_nJpegQuality != -1)
        {
            TIFFSetField(l_hTIFF, TIFFTAG_JPEGQUALITY, poDS->m_nJpegQuality);
        }
        TIFFSetField(l_hTIFF, TIFFTAG_JPEGTABLESMODE, poDS->m_nJpegTablesMode);
    }
    if (l_nCompression == COMPRESSION_LZMA)
    {
        if (poDS->m_nLZMAPreset != -1)
        {
            TIFFSetField(l_hTIFF, TIFFTAG_LZMAPRESET, poDS->m_nLZMAPreset);
        }
    }
    if (l_nCompression == COMPRESSION_ZSTD ||
        l_nCompression == COMPRESSION_LERC)
    {
        if (poDS->m_nZSTDLevel != -1)
        {
            TIFFSetField(l_hTIFF, TIFFTAG_ZSTD_LEVEL, poDS->m_nZSTDLevel);
        }
    }
    if (l_nCompression == COMPRESSION_LERC)
    {
        TIFFSetField(l_hTIFF, TIFFTAG_LERC_MAXZERROR, poDS->m_dfMaxZError);
    }
#if HAVE_JXL
    if (l_nCompression == COMPRESSION_JXL ||
        l_nCompression == COMPRESSION_JXL_DNG_1_7)
    {
        TIFFSetField(l_hTIFF, TIFFTAG_JXL_LOSSYNESS,
                     poDS->m_bJXLLossless ? JXL_LOSSLESS : JXL_LOSSY);
        TIFFSetField(l_hTIFF, TIFFTAG_JXL_EFFORT, poDS->m_nJXLEffort);
        TIFFSetField(l_hTIFF, TIFFTAG_JXL_DISTANCE,
                     static_cast<double>(poDS->m_fJXLDistance));
        TIFFSetField(l_hTIFF, TIFFTAG_JXL_ALPHA_DISTANCE,
                     static_cast<double>(poDS->m_fJXLAlphaDistance));
    }
#endif
    if (l_nCompression == COMPRESSION_WEBP)
    {
        if (poDS->m_nWebPLevel != -1)
        {
            TIFFSetField(l_hTIFF, TIFFTAG_WEBP_LEVEL, poDS->m_nWebPLevel);
        }

        if (poDS->m_bWebPLossless)
        {
            TIFFSetField(l_hTIFF, TIFFTAG_WEBP_LOSSLESS, poDS->m_bWebPLossless);
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Do we want to ensure all blocks get written out on close to     */
    /*      avoid sparse files?                                             */
    /* -------------------------------------------------------------------- */
    if (!CPLFetchBool(papszOptions, "SPARSE_OK", false))
        poDS->m_bFillEmptyTilesAtClosing = true;

    poDS->m_bWriteEmptyTiles =
        (bCopySrcOverviews && poDS->m_bFillEmptyTilesAtClosing) || bStreaming ||
        (poDS->m_nCompression != COMPRESSION_NONE &&
         poDS->m_bFillEmptyTilesAtClosing);
    // Only required for people writing non-compressed striped files in the
    // rightorder and wanting all tstrips to be written in the same order
    // so that the end result can be memory mapped without knowledge of each
    // strip offset
    if (CPLTestBool(CSLFetchNameValueDef(
            papszOptions, "WRITE_EMPTY_TILES_SYNCHRONOUSLY", "FALSE")) ||
        CPLTestBool(CSLFetchNameValueDef(
            papszOptions, "@WRITE_EMPTY_TILES_SYNCHRONOUSLY", "FALSE")))
    {
        poDS->m_bWriteEmptyTiles = true;
    }

    // Precreate (internal) mask, so that the IBuildOverviews() below
    // has a chance to create also the overviews of the mask.
    CPLErr eErr = CE_None;

    if (bCreateMask)
    {
        eErr = poDS->CreateMaskBand(nMaskFlags);
        if (poDS->m_poMaskDS)
        {
            poDS->m_poMaskDS->m_bFillEmptyTilesAtClosing =
                poDS->m_bFillEmptyTilesAtClosing;
            poDS->m_poMaskDS->m_bWriteEmptyTiles = poDS->m_bWriteEmptyTiles;
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Create and then copy existing overviews if requested            */
    /*  We do it such that all the IFDs are at the beginning of the file,   */
    /*  and that the imagery data for the smallest overview is written      */
    /*  first, that way the file is more usable when embedded in a          */
    /*  compressed stream.                                                  */
    /* -------------------------------------------------------------------- */

    // For scaled progress due to overview copying.
    const int nBandsWidthMask = l_nBands + (bCreateMask ? 1 : 0);
    double dfTotalPixels =
        static_cast<double>(nXSize) * nYSize * nBandsWidthMask;
    double dfCurPixels = 0;

    if (eErr == CE_None && bCopySrcOverviews)
    {
        std::unique_ptr<GDALDataset> poMaskOvrDS;
        const char *pszMaskOvrDS =
            CSLFetchNameValue(papszOptions, "@MASK_OVERVIEW_DATASET");
        if (pszMaskOvrDS)
        {
            poMaskOvrDS.reset(GDALDataset::Open(pszMaskOvrDS));
            if (!poMaskOvrDS)
            {
                l_fpL->CancelCreation();
                delete poDS;
                return nullptr;
            }
            if (poMaskOvrDS->GetRasterCount() != 1)
            {
                l_fpL->CancelCreation();
                delete poDS;
                return nullptr;
            }
        }
        if (nSrcOverviews)
        {
            eErr = poDS->CreateOverviewsFromSrcOverviews(poSrcDS, poOvrDS.get(),
                                                         nSrcOverviews);

            if (eErr == CE_None &&
                (poMaskOvrDS != nullptr ||
                 (poSrcDS->GetRasterBand(1)->GetOverview(0) &&
                  poSrcDS->GetRasterBand(1)->GetOverview(0)->GetMaskFlags() ==
                      GMF_PER_DATASET)))
            {
                int nOvrBlockXSize = 0;
                int nOvrBlockYSize = 0;
                GTIFFGetOverviewBlockSize(
                    GDALRasterBand::ToHandle(poDS->GetRasterBand(1)),
                    &nOvrBlockXSize, &nOvrBlockYSize, nullptr, nullptr);
                eErr = poDS->CreateInternalMaskOverviews(nOvrBlockXSize,
                                                         nOvrBlockYSize);
            }
        }

        TIFFForceStrileArrayWriting(poDS->m_hTIFF);

        if (poDS->m_poMaskDS)
        {
            TIFFForceStrileArrayWriting(poDS->m_poMaskDS->m_hTIFF);
        }

        for (int i = 0; i < poDS->m_nOverviewCount; i++)
        {
            TIFFForceStrileArrayWriting(poDS->m_papoOverviewDS[i]->m_hTIFF);

            if (poDS->m_papoOverviewDS[i]->m_poMaskDS)
            {
                TIFFForceStrileArrayWriting(
                    poDS->m_papoOverviewDS[i]->m_poMaskDS->m_hTIFF);
            }
        }

        if (eErr == CE_None && nSrcOverviews)
        {
            if (poDS->m_nOverviewCount != nSrcOverviews)
            {
                ReportError(
                    pszFilename, CE_Failure, CPLE_AppDefined,
                    "Did only manage to instantiate %d overview levels, "
                    "whereas source contains %d",
                    poDS->m_nOverviewCount, nSrcOverviews);
                eErr = CE_Failure;
            }

            for (int i = 0; eErr == CE_None && i < nSrcOverviews; ++i)
            {
                GDALRasterBand *poOvrBand =
                    poOvrDS
                        ? (i == 0
                               ? poOvrDS->GetRasterBand(1)
                               : poOvrDS->GetRasterBand(1)->GetOverview(i - 1))
                        : poSrcDS->GetRasterBand(1)->GetOverview(i);
                const double dfOvrPixels =
                    static_cast<double>(poOvrBand->GetXSize()) *
                    poOvrBand->GetYSize();
                dfTotalPixels += dfOvrPixels * l_nBands;
                if (poOvrBand->GetMaskFlags() == GMF_PER_DATASET ||
                    poMaskOvrDS != nullptr)
                {
                    dfTotalPixels += dfOvrPixels;
                }
                else if (i == 0 && poDS->GetRasterBand(1)->GetMaskFlags() ==
                                       GMF_PER_DATASET)
                {
                    ReportError(pszFilename, CE_Warning, CPLE_AppDefined,
                                "Source dataset has a mask band on full "
                                "resolution, overviews on the regular bands, "
                                "but lacks overviews on the mask band.");
                }
            }

            // Now copy the imagery.
            // Begin with the smallest overview.
            for (int iOvrLevel = nSrcOverviews - 1;
                 eErr == CE_None && iOvrLevel >= 0; --iOvrLevel)
            {
                auto poDstDS = poDS->m_papoOverviewDS[iOvrLevel];

                // Create a fake dataset with the source overview level so that
                // GDALDatasetCopyWholeRaster can cope with it.
                GDALDataset *poSrcOvrDS =
                    poOvrDS
                        ? (iOvrLevel == 0 ? poOvrDS.get()
                                          : GDALCreateOverviewDataset(
                                                poOvrDS.get(), iOvrLevel - 1,
                                                /* bThisLevelOnly = */ true))
                        : GDALCreateOverviewDataset(
                              poSrcDS, iOvrLevel,
                              /* bThisLevelOnly = */ true);
                GDALRasterBand *poSrcOvrBand =
                    poOvrDS ? (iOvrLevel == 0
                                   ? poOvrDS->GetRasterBand(1)
                                   : poOvrDS->GetRasterBand(1)->GetOverview(
                                         iOvrLevel - 1))
                            : poSrcDS->GetRasterBand(1)->GetOverview(iOvrLevel);
                double dfNextCurPixels =
                    dfCurPixels +
                    static_cast<double>(poSrcOvrBand->GetXSize()) *
                        poSrcOvrBand->GetYSize() * l_nBands;

                poDstDS->m_bBlockOrderRowMajor = true;
                poDstDS->m_bLeaderSizeAsUInt4 = true;
                poDstDS->m_bTrailerRepeatedLast4BytesRepeated = true;
                poDstDS->m_bFillEmptyTilesAtClosing =
                    poDS->m_bFillEmptyTilesAtClosing;
                poDstDS->m_bWriteEmptyTiles = poDS->m_bWriteEmptyTiles;
                poDstDS->m_bTileInterleave = poDS->m_bTileInterleave;
                GDALRasterBand *poSrcMaskBand = nullptr;
                if (poDstDS->m_poMaskDS)
                {
                    poDstDS->m_poMaskDS->m_bBlockOrderRowMajor = true;
                    poDstDS->m_poMaskDS->m_bLeaderSizeAsUInt4 = true;
                    poDstDS->m_poMaskDS->m_bTrailerRepeatedLast4BytesRepeated =
                        true;
                    poDstDS->m_poMaskDS->m_bFillEmptyTilesAtClosing =
                        poDS->m_bFillEmptyTilesAtClosing;
                    poDstDS->m_poMaskDS->m_bWriteEmptyTiles =
                        poDS->m_bWriteEmptyTiles;

                    poSrcMaskBand =
                        poMaskOvrDS
                            ? (iOvrLevel == 0
                                   ? poMaskOvrDS->GetRasterBand(1)
                                   : poMaskOvrDS->GetRasterBand(1)->GetOverview(
                                         iOvrLevel - 1))
                            : poSrcOvrBand->GetMaskBand();
                }

                if (poDstDS->m_poMaskDS)
                {
                    dfNextCurPixels +=
                        static_cast<double>(poSrcOvrBand->GetXSize()) *
                        poSrcOvrBand->GetYSize();
                }
                void *pScaledData =
                    GDALCreateScaledProgress(dfCurPixels / dfTotalPixels,
                                             dfNextCurPixels / dfTotalPixels,
                                             pfnProgress, pProgressData);

                eErr = CopyImageryAndMask(poDstDS, poSrcOvrDS, poSrcMaskBand,
                                          GDALScaledProgress, pScaledData);

                dfCurPixels = dfNextCurPixels;
                GDALDestroyScaledProgress(pScaledData);

                if (poSrcOvrDS != poOvrDS.get())
                    delete poSrcOvrDS;
                poSrcOvrDS = nullptr;
            }
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Copy actual imagery.                                            */
    /* -------------------------------------------------------------------- */
    double dfNextCurPixels =
        dfCurPixels + static_cast<double>(nXSize) * nYSize * l_nBands;
    void *pScaledData = GDALCreateScaledProgress(
        dfCurPixels / dfTotalPixels, dfNextCurPixels / dfTotalPixels,
        pfnProgress, pProgressData);

#if defined(HAVE_LIBJPEG) || defined(JPEG_DIRECT_COPY)
    bool bTryCopy = true;
#endif

#ifdef HAVE_LIBJPEG
    if (bCopyFromJPEG)
    {
        eErr = GTIFF_CopyFromJPEG(poDS, poSrcDS, pfnProgress, pProgressData,
                                  bTryCopy);

        // In case of failure in the decompression step, try normal copy.
        if (bTryCopy)
            eErr = CE_None;
    }
#endif

#ifdef JPEG_DIRECT_COPY
    if (bDirectCopyFromJPEG)
    {
        eErr = GTIFF_DirectCopyFromJPEG(poDS, poSrcDS, pfnProgress,
                                        pProgressData, bTryCopy);

        // In case of failure in the reading step, try normal copy.
        if (bTryCopy)
            eErr = CE_None;
    }
#endif

    bool bWriteMask = true;
    if (
#if defined(HAVE_LIBJPEG) || defined(JPEG_DIRECT_COPY)
        bTryCopy &&
#endif
        (poDS->m_bTreatAsSplit || poDS->m_bTreatAsSplitBitmap))
    {
        // For split bands, we use TIFFWriteScanline() interface.
        CPLAssert(poDS->m_nBitsPerSample == 8 || poDS->m_nBitsPerSample == 1);

        if (poDS->m_nPlanarConfig == PLANARCONFIG_CONTIG && poDS->nBands > 1)
        {
            GByte *pabyScanline = static_cast<GByte *>(
                VSI_MALLOC_VERBOSE(TIFFScanlineSize(l_hTIFF)));
            if (pabyScanline == nullptr)
                eErr = CE_Failure;
            for (int j = 0; j < nYSize && eErr == CE_None; ++j)
            {
                eErr = poSrcDS->RasterIO(GF_Read, 0, j, nXSize, 1, pabyScanline,
                                         nXSize, 1, GDT_Byte, l_nBands, nullptr,
                                         poDS->nBands, 0, 1, nullptr);
                if (eErr == CE_None &&
                    TIFFWriteScanline(l_hTIFF, pabyScanline, j, 0) == -1)
                {
                    ReportError(pszFilename, CE_Failure, CPLE_AppDefined,
                                "TIFFWriteScanline() failed.");
                    eErr = CE_Failure;
                }
                if (!GDALScaledProgress((j + 1) * 1.0 / nYSize, nullptr,
                                        pScaledData))
                    eErr = CE_Failure;
            }
            CPLFree(pabyScanline);
        }
        else
        {
            GByte *pabyScanline =
                static_cast<GByte *>(VSI_MALLOC_VERBOSE(nXSize));
            if (pabyScanline == nullptr)
                eErr = CE_Failure;
            else
                eErr = CE_None;
            for (int iBand = 1; iBand <= l_nBands && eErr == CE_None; ++iBand)
            {
                for (int j = 0; j < nYSize && eErr == CE_None; ++j)
                {
                    eErr = poSrcDS->GetRasterBand(iBand)->RasterIO(
                        GF_Read, 0, j, nXSize, 1, pabyScanline, nXSize, 1,
                        GDT_Byte, 0, 0, nullptr);
                    if (poDS->m_bTreatAsSplitBitmap)
                    {
                        for (int i = 0; i < nXSize; ++i)
                        {
                            const GByte byVal = pabyScanline[i];
                            if ((i & 0x7) == 0)
                                pabyScanline[i >> 3] = 0;
                            if (byVal)
                                pabyScanline[i >> 3] |= 0x80 >> (i & 0x7);
                        }
                    }
                    if (eErr == CE_None &&
                        TIFFWriteScanline(l_hTIFF, pabyScanline, j,
                                          static_cast<uint16_t>(iBand - 1)) ==
                            -1)
                    {
                        ReportError(pszFilename, CE_Failure, CPLE_AppDefined,
                                    "TIFFWriteScanline() failed.");
                        eErr = CE_Failure;
                    }
                    if (!GDALScaledProgress((j + 1 + (iBand - 1) * nYSize) *
                                                1.0 / (l_nBands * nYSize),
                                            nullptr, pScaledData))
                        eErr = CE_Failure;
                }
            }
            CPLFree(pabyScanline);
        }

        // Necessary to be able to read the file without re-opening.
        TIFFSizeProc pfnSizeProc = TIFFGetSizeProc(l_hTIFF);

        TIFFFlushData(l_hTIFF);

        toff_t nNewDirOffset = pfnSizeProc(TIFFClientdata(l_hTIFF));
        if ((nNewDirOffset % 2) == 1)
            ++nNewDirOffset;

        TIFFFlush(l_hTIFF);

        if (poDS->m_nDirOffset != TIFFCurrentDirOffset(l_hTIFF))
        {
            poDS->m_nDirOffset = nNewDirOffset;
            CPLDebug("GTiff", "directory moved during flush.");
        }
    }
    else if (
#if defined(HAVE_LIBJPEG) || defined(JPEG_DIRECT_COPY)
        bTryCopy &&
#endif
        eErr == CE_None)
    {
        const char *papszCopyWholeRasterOptions[3] = {nullptr, nullptr,
                                                      nullptr};
        int iNextOption = 0;
        papszCopyWholeRasterOptions[iNextOption++] = "SKIP_HOLES=YES";
        if (l_nCompression != COMPRESSION_NONE)
        {
            papszCopyWholeRasterOptions[iNextOption++] = "COMPRESSED=YES";
        }

        // For streaming with separate, we really want that bands are written
        // after each other, even if the source is pixel interleaved.
        else if (bStreaming && poDS->m_nPlanarConfig == PLANARCONFIG_SEPARATE)
        {
            papszCopyWholeRasterOptions[iNextOption++] = "INTERLEAVE=BAND";
        }

        if (bCopySrcOverviews || bTileInterleaving)
        {
            poDS->m_bBlockOrderRowMajor = true;
            poDS->m_bLeaderSizeAsUInt4 = bCopySrcOverviews;
            poDS->m_bTrailerRepeatedLast4BytesRepeated = bCopySrcOverviews;
            if (poDS->m_poMaskDS)
            {
                poDS->m_poMaskDS->m_bBlockOrderRowMajor = true;
                poDS->m_poMaskDS->m_bLeaderSizeAsUInt4 = bCopySrcOverviews;
                poDS->m_poMaskDS->m_bTrailerRepeatedLast4BytesRepeated =
                    bCopySrcOverviews;
                GDALDestroyScaledProgress(pScaledData);
                pScaledData =
                    GDALCreateScaledProgress(dfCurPixels / dfTotalPixels, 1.0,
                                             pfnProgress, pProgressData);
            }

            eErr = CopyImageryAndMask(poDS, poSrcDS,
                                      poSrcDS->GetRasterBand(1)->GetMaskBand(),
                                      GDALScaledProgress, pScaledData);
            if (poDS->m_poMaskDS)
            {
                bWriteMask = false;
            }
        }
        else
        {
            eErr = GDALDatasetCopyWholeRaster(
                /* (GDALDatasetH) */ poSrcDS,
                /* (GDALDatasetH) */ poDS, papszCopyWholeRasterOptions,
                GDALScaledProgress, pScaledData);
        }
    }

    GDALDestroyScaledProgress(pScaledData);

    if (eErr == CE_None && !bStreaming && bWriteMask)
    {
        pScaledData = GDALCreateScaledProgress(dfNextCurPixels / dfTotalPixels,
                                               1.0, pfnProgress, pProgressData);
        if (poDS->m_poMaskDS)
        {
            const char *l_papszOptions[2] = {"COMPRESSED=YES", nullptr};
            eErr = GDALRasterBandCopyWholeRaster(
                poSrcDS->GetRasterBand(1)->GetMaskBand(),
                poDS->GetRasterBand(1)->GetMaskBand(),
                const_cast<char **>(l_papszOptions), GDALScaledProgress,
                pScaledData);
        }
        else
        {
            eErr =
                GDALDriver::DefaultCopyMasks(poSrcDS, poDS, bStrict, nullptr,
                                             GDALScaledProgress, pScaledData);
        }
        GDALDestroyScaledProgress(pScaledData);
    }

    poDS->m_bWriteCOGLayout = false;

    if (eErr == CE_None &&
        CPLTestBool(CSLFetchNameValueDef(poDS->m_papszCreationOptions,
                                         "@FLUSHCACHE", "NO")))
    {
        if (poDS->FlushCache(false) != CE_None)
        {
            eErr = CE_Failure;
        }
    }

    if (eErr == CE_Failure)
    {
        if (CPLTestBool(CPLGetConfigOption("GTIFF_DELETE_ON_ERROR", "YES")))
        {
            l_fpL->CancelCreation();
            delete poDS;
            poDS = nullptr;

            if (!bStreaming)
            {
                // Should really delete more carefully.
                VSIUnlink(pszFilename);
            }
        }
        else
        {
            delete poDS;
            poDS = nullptr;
        }
    }
#endif  // KK

    return nullptr;  //poDS;
}

bool MMRDataset::CreateRasterBands()
{
    MMRBand *pBand;

    for (int nIBand = 0; nIBand < m_pMMRRel->GetNBands(); nIBand++)
    {
        // Establish raster band info.
        pBand = m_pMMRRel->GetBand(nIBand);
        if (!pBand)
            return false;
        nRasterXSize = pBand->GetWidth();
        nRasterYSize = pBand->GetHeight();
        pBand->UpdateGeoTransform();  // Fills adfGeoTransform for this band

        auto poRasterBand = std::make_unique<MMRRasterBand>(this, nBands + 1);
        if (!poRasterBand->IsValid())
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Failed to create a RasterBand from '%s'",
                     m_pMMRRel->GetRELNameChar());

            return false;
        }

        SetBand(nBands + 1, std::move(poRasterBand));

        MMRRasterBand *poBand =
            cpl::down_cast<MMRRasterBand *>(GetRasterBand(nIBand + 1));

        pBand = m_pMMRRel->GetBand(nIBand);
        if (!pBand)
            return false;
        if (!pBand->GetFriendlyDescription().empty())
        {
            poBand->SetMetadataItem("DESCRIPTION",
                                    pBand->GetFriendlyDescription());
        }
    }
    // Some metadata items must be preserved just in case to be restored
    // if they are preserved through translations.
    m_pMMRRel->RELToGDALMetadata(this);

    return true;
}

void MMRDataset::ReadProjection()

{
    if (!m_pMMRRel)
        return;

    CPLString osSRS;

    if (!m_pMMRRel->GetMetadataValue("SPATIAL_REFERENCE_SYSTEM:HORIZONTAL",
                                     "HorizontalSystemIdentifier", osSRS) ||
        osSRS.empty())
        return;

    char szResult[MM_MAX_ID_SNY + 10];
    int nResult = ReturnEPSGCodeSRSFromMMIDSRS(osSRS.c_str(), szResult);
    if (nResult == 1 || szResult[0] == '\0')
        return;

    int nEPSG;
    if (1 == sscanf(szResult, "%d", &nEPSG))
        m_oSRS.importFromEPSG(nEPSG);

    return;
}

/************************************************************************/
/*                           SUBDATASETS                                */
/************************************************************************/
// Assigns every band to a subdataset
void MMRDataset::AssignBandsToSubdataSets()
{
    m_nNSubdataSets = 0;
    if (!m_pMMRRel.get())
        return;

    m_nNSubdataSets = 1;
    int nIBand = 0;
    MMRBand *pBand = m_pMMRRel->GetBand(nIBand);
    if (!pBand)
        return;

    pBand->AssignSubDataSet(m_nNSubdataSets);
    MMRBand *pNextBand;
    for (; nIBand < m_pMMRRel->GetNBands() - 1; nIBand++)
    {
        if (IsNextBandInANewDataSet(nIBand))
        {
            m_nNSubdataSets++;
            pNextBand = m_pMMRRel->GetBand(nIBand + 1);
            if (!pNextBand)
                return;
            pNextBand->AssignSubDataSet(m_nNSubdataSets);
        }
        else
        {
            pNextBand = m_pMMRRel->GetBand(nIBand + 1);
            if (!pNextBand)
                return;
            pNextBand->AssignSubDataSet(m_nNSubdataSets);
        }
    }

    // If there is only one subdataset, it means that
    // we don't need subdatasets (all assigned to 0)
    if (m_nNSubdataSets == 1)
    {
        m_nNSubdataSets = 0;
        for (nIBand = 0; nIBand < m_pMMRRel->GetNBands(); nIBand++)
        {
            pBand = m_pMMRRel->GetBand(nIBand);
            if (!pBand)
                break;
            pBand->AssignSubDataSet(m_nNSubdataSets);
        }
    }
}

void MMRDataset::CreateSubdatasetsFromBands()
{
    CPLStringList oSubdatasetList;
    CPLString osDSName;
    CPLString osDSDesc;
    MMRBand *pBand;

    for (int iSubdataset = 1; iSubdataset <= m_nNSubdataSets; iSubdataset++)
    {
        int nIBand;
        for (nIBand = 0; nIBand < m_pMMRRel->GetNBands(); nIBand++)
        {
            pBand = m_pMMRRel->GetBand(nIBand);
            if (!pBand)
                return;
            if (pBand->GetAssignedSubDataSet() == iSubdataset)
                break;
        }

        if (nIBand == m_pMMRRel->GetNBands())
            break;

        pBand = m_pMMRRel->GetBand(nIBand);
        if (!pBand)
            return;

        osDSName.Printf("MiraMonRaster:\"%s\",\"%s\"",
                        pBand->GetRELFileName().c_str(),
                        pBand->GetRawBandFileName().c_str());
        osDSDesc.Printf("Subdataset %d: \"%s\"", iSubdataset,
                        pBand->GetBandName().c_str());
        nIBand++;

        for (; nIBand < m_pMMRRel->GetNBands(); nIBand++)
        {
            pBand = m_pMMRRel->GetBand(nIBand);
            if (!pBand)
                return;
            if (pBand->GetAssignedSubDataSet() != iSubdataset)
                continue;

            osDSName.append(
                CPLSPrintf(",\"%s\"", pBand->GetRawBandFileName().c_str()));
            osDSDesc.append(
                CPLSPrintf(",\"%s\"", pBand->GetBandName().c_str()));
        }

        oSubdatasetList.AddNameValue(
            CPLSPrintf("SUBDATASET_%d_NAME", iSubdataset), osDSName);
        oSubdatasetList.AddNameValue(
            CPLSPrintf("SUBDATASET_%d_DESC", iSubdataset), osDSDesc);
    }

    if (oSubdatasetList.Count() > 0)
    {
        // Add metadata to the main dataset
        SetMetadata(oSubdatasetList.List(), "SUBDATASETS");
        oSubdatasetList.Clear();
    }
}

bool MMRDataset::IsNextBandInANewDataSet(int nIBand) const
{
    if (nIBand < 0)
        return false;

    if (nIBand + 1 >= m_pMMRRel->GetNBands())
        return false;

    MMRBand *pThisBand = m_pMMRRel->GetBand(nIBand);
    MMRBand *pNextBand = m_pMMRRel->GetBand(nIBand + 1);
    if (!pThisBand || !pNextBand)
        return false;

    // Two images with different numbers of columns are assigned to different subdatasets
    if (pThisBand->GetWidth() != pNextBand->GetWidth())
        return true;

    // Two images with different numbers of rows are assigned to different subdatasets
    if (pThisBand->GetHeight() != pNextBand->GetHeight())
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
    if (pThisBand->BandHasNoData() != pNextBand->BandHasNoData())
        return true;

    // Two images with different NoData values are assigned to different subdatasets
    if (pThisBand->GetNoDataValue() != pNextBand->GetNoDataValue())
        return true;

    return false;
}

/************************************************************************/
/*                          UpdateGeoTransform()                     */
/************************************************************************/
int MMRDataset::UpdateGeoTransform()
{
    // Bounding box of the band
    // Section [EXTENT] in rel file

    if (!m_pMMRRel)
        return 1;

    CPLString osMinX;
    if (!m_pMMRRel->GetMetadataValue(SECTION_EXTENT, "MinX", osMinX) ||
        osMinX.empty())
        return 1;

    if (1 != CPLsscanf(osMinX, "%lf", &(m_gt[0])))
        m_gt[0] = 0.0;

    int nNCols = m_pMMRRel->GetColumnsNumberFromREL();
    if (nNCols <= 0)
        return 1;

    CPLString osMaxX;
    if (!m_pMMRRel->GetMetadataValue(SECTION_EXTENT, "MaxX", osMaxX) ||
        osMaxX.empty())
        return 1;

    double dfMaxX;
    if (1 != CPLsscanf(osMaxX, "%lf", &dfMaxX))
        dfMaxX = 1.0;

    m_gt[1] = (dfMaxX - m_gt[0]) / nNCols;
    m_gt[2] = 0.0;  // No rotation in MiraMon rasters

    CPLString osMinY;
    if (!m_pMMRRel->GetMetadataValue(SECTION_EXTENT, "MinY", osMinY) ||
        osMinY.empty())
        return 1;

    CPLString osMaxY;
    if (!m_pMMRRel->GetMetadataValue(SECTION_EXTENT, "MaxY", osMaxY) ||
        osMaxY.empty())
        return 1;

    int nNRows = m_pMMRRel->GetRowsNumberFromREL();
    if (nNRows <= 0)
        return 1;

    double dfMaxY;
    if (1 != CPLsscanf(osMaxY, "%lf", &dfMaxY))
        dfMaxY = 1.0;

    m_gt[3] = dfMaxY;
    m_gt[4] = 0.0;

    double dfMinY;
    if (1 != CPLsscanf(osMinY, "%lf", &dfMinY))
        dfMinY = 0.0;
    m_gt[5] = (dfMinY - m_gt[3]) / nNRows;

    return 0;
}

const OGRSpatialReference *MMRDataset::GetSpatialRef() const
{
    return m_oSRS.IsEmpty() ? nullptr : &m_oSRS;
}

CPLErr MMRDataset::GetGeoTransform(GDALGeoTransform &gt) const
{
    if (m_gt[0] != 0.0 || m_gt[1] != 1.0 || m_gt[2] != 0.0 || m_gt[3] != 0.0 ||
        m_gt[4] != 0.0 || m_gt[5] != 1.0)
    {
        gt = m_gt;
        return CE_None;
    }

    return GDALDataset::GetGeoTransform(gt);
}

/************************************************************************/
/*                          REL/IMG names                               */
/************************************************************************/

// Finds the metadata filename associated to osFileName (usually an IMG file)
CPLString
MMRDataset::CreateAssociatedMetadataFileName(const CPLString &osFileName)
{
    if (osFileName.empty())
    {
        CPLError(CE_Failure, CPLE_OpenFailed, "Expected output file name.");
        return "";
    }

    CPLString osRELName = osFileName;

    // If the string finishes in "I.rel" we consider it can be
    // the associated file to all bands that are documented in this file.
    if (cpl::ends_with(osFileName, pszExtRasterREL))
        return osRELName;

    // If the string finishes in ".img" we consider it can converted to rel
    // just changing "img" for "I.rel"
    if (cpl::ends_with(osFileName, pszExtRaster))
    {
        // Extract extension
        osRELName = CPLResetExtensionSafe(osRELName, "");

        if (!osRELName.length())
            return "";

        // Extract "."
        osRELName.resize(osRELName.size() - 1);

        if (!osRELName.length())
            return "";

        // Add "I.rel"
        osRELName += pszExtRasterREL;
        return osRELName;
    }

    // If the file is not a REL file, let's assume that "I.rel" can be added
    // to get the REL file.
    osRELName += pszExtRasterREL;
    return osRELName;
}

// Finds the pattern name to the bands
CPLString
MMRDataset::CreatePatternFileName(const CPLString &osFileName, const CPLString &osPattern)
{
    if(!osPattern.empty())
        return osPattern;

    CPLString osRELName = osFileName;

    if (!cpl::ends_with(osFileName, pszExtRasterREL))
        return "";

    // Extract I.rel and path
    osRELName.resize(osRELName.size() - strlen("I.rel"));
    return CPLGetBasenameSafe(osRELName);
}
