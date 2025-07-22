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
#include "miramon_dataset.h"
#include "miramon_rasterband.h"
#include "miramon_band.h"  // Per a MMRBand

#ifdef MSVC
#include "..\miramon_common\mm_gdal_functions.h"  // For MMCheck_REL_FILE()
#else
#include "../miramon_common/mm_gdal_functions.h"  // For MMCheck_REL_FILE()
#endif

/************************************************************************/
/*                            MMRDataset()                            */
/************************************************************************/

MMRDataset::MMRDataset(GDALOpenInfo *poOpenInfo)
{
    m_oSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

    // Creating the class MMRRel.
    auto pMMfRel = std::make_unique<MMRRel>(poOpenInfo->pszFilename);

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

    pfRel = pMMfRel.release();

    // General Dataset information available
    nRasterXSize = pfRel->GetXSize();
    nRasterYSize = pfRel->GetYSize();
    GetDataSetBoundingBox();  // Fills adfGeoTransform
    ReadProjection();
    nBands = 0;

    // Assign every band to a subdataset (if any)
    // If all bands should go to a one single Subdataset, then,
    // no subdataset will be created and all bands will go to this
    // dataset.
    AssignBandsToSubdataSets();

    // Create subdatasets or add bands, as needed
    if (nNSubdataSets)
        CreateSubdatasetsFromBands();
    else
        AssignBands();
}

/************************************************************************/
/*                           ~MMRDataset()                            */
/************************************************************************/

MMRDataset::~MMRDataset()

{
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
}

/************************************************************************/
/*                           ReadProjection()                           */
/************************************************************************/
CPLErr MMRDataset::ReadProjection()

{
    if (!pfRel)
        return CE_Failure;

    CPLString osSRS = pfRel->GetMetadataValue(
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
    if (poDS->pfRel == nullptr)
        return nullptr;

    // Set description
    poDS->SetDescription(poOpenInfo->pszFilename);

    return poDS.release();
}

/************************************************************************/
/*                          GetSpatialRef()                             */
/************************************************************************/

const OGRSpatialReference *MMRDataset::GetSpatialRef() const
{
    return m_oSRS.IsEmpty() ? nullptr : &m_oSRS;
}

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
/*                          GetDataSetBoundingBox()                     */
/************************************************************************/
int MMRDataset::GetDataSetBoundingBox()
{
    // Bounding box of the band
    // Section [EXTENT] in rel file

    m_gt[0] = 0.0;
    m_gt[1] = 1.0;
    m_gt[2] = 0.0;
    m_gt[3] = 0.0;
    m_gt[4] = 0.0;
    m_gt[5] = 1.0;

    if (!pfRel)
        return 1;

    CPLString osMinX = pfRel->GetMetadataValue(SECTION_EXTENT, "MinX");
    if (osMinX.empty())
        return 1;
    m_gt[0] = atof(osMinX);

    int nNCols = pfRel->GetColumnsNumberFromREL();
    if (nNCols <= 0)
        return 1;

    CPLString osMaxX = pfRel->GetMetadataValue(SECTION_EXTENT, "MaxX");
    if (osMaxX.empty())
        return 1;

    m_gt[1] = (atof(osMaxX) - m_gt[0]) / nNCols;
    m_gt[2] = 0.0;  // No rotation in MiraMon rasters

    CPLString osMinY = pfRel->GetMetadataValue(SECTION_EXTENT, "MinY");
    if (osMinY.empty())
        return 1;

    CPLString osMaxY = pfRel->GetMetadataValue(SECTION_EXTENT, "MaxY");
    if (osMaxY.empty())
        return 1;

    int nNRows = pfRel->GetRowsNumberFromREL();
    if (nNRows <= 0)
        return 1;

    m_gt[3] = atof(osMaxY);
    m_gt[4] = 0.0;
    m_gt[5] = (atof(osMinY) - m_gt[3]) / nNRows;

    return 0;
}

int MMRDataset::GetBandBoundingBox(int nIBand)
{
    // Bounding box of the band
    m_gt[0] = 0.0;
    m_gt[1] = 1.0;
    m_gt[2] = 0.0;
    m_gt[3] = 0.0;
    m_gt[4] = 0.0;
    m_gt[5] = 1.0;

    if (!pfRel->GetBands() || nIBand >= pfRel->GetNBands() ||
        !pfRel->GetBand(nIBand))
        return 1;

    MMRBand *poBand = pfRel->GetBand(nIBand);

    m_gt[0] = poBand->GetBoundingBoxMinX();
    m_gt[1] = (poBand->GetBoundingBoxMaxX() - m_gt[0]) / poBand->nWidth;
    m_gt[2] = 0.0;  // No rotation in MiraMon rasters
    m_gt[3] = poBand->GetBoundingBoxMaxY();
    m_gt[4] = 0.0;
    m_gt[5] = (poBand->GetBoundingBoxMinY() - m_gt[3]) / poBand->nHeight;

    return 0;
}

CPLErr MMRDataset::GetGeoTransform(GDALGeoTransform &gt) const
{
    if (m_gt[0] != 0.0 || m_gt[1] != 1.0 || m_gt[2] != 0.0 || m_gt[3] != 0.0 ||
        m_gt[4] != 0.0 || m_gt[5] != 1.0)
    {
        gt = m_gt;
        return CE_None;
    }

    //return GDALPamDataset::GetGeoTransform(padfTransform);
    return GDALDataset::GetGeoTransform(gt);
}

/************************************************************************/
/*                            GetGCPCount()                             */
/************************************************************************/
/*
int MMRDataset::GetGCPCount()
{
    const int nPAMCount = GDALPamDataset::GetGCPCount();
    return nPAMCount > 0 ? nPAMCount : static_cast<int>(m_aoGCPs.size());
}*/

/************************************************************************/
/*                          GetGCPSpatialRef()                          */
/************************************************************************/
/*
const OGRSpatialReference *MMRDataset::GetGCPSpatialRef() const

{
    const OGRSpatialReference *poSRS = GDALPamDataset::GetGCPSpatialRef();
    if (poSRS)
        return poSRS;
    return !m_aoGCPs.empty() && !m_oSRS.IsEmpty() ? &m_oSRS : nullptr;
}*/

/************************************************************************/
/*                               GetGCPs()                              */
/************************************************************************/
/*
const GDAL_GCP *MMRDataset::GetGCPs()
{
    const GDAL_GCP *psPAMGCPs = GDALPamDataset::GetGCPs();
    if (psPAMGCPs)
        return psPAMGCPs;
    return gdal::GCP::c_ptr(m_aoGCPs);
}*/

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
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "rel");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSIONS, "rel img");
    // For the writing part
    // poDriver->SetMetadataItem(
    //    GDAL_DMD_CREATIONDATATYPES,
    //    "Byte Int8 Int16 UInt16 Int32 UInt32 Float32 Float64 "
    //    "CFloat32 CFloat64");

    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_SUBDATASETS, "YES");

    poDriver->pfnOpen = MMRDataset::Open;
    poDriver->pfnIdentify = MMRDataset::Identify;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}

bool MMRDataset::NextBandInANewDataSet(int nIBand)
{
    if (nIBand < 0)
        return false;

    if (nIBand + 1 >= pfRel->GetNBands())
        return false;

    MMRBand *pThisBand = pfRel->GetBand(nIBand);
    MMRBand *pNextBand = pfRel->GetBand(nIBand + 1);

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
    if (pThisBand->BandHasNoData() != pNextBand->BandHasNoData())
        return true;

    // Two images with different NoData values are assigned to different subdatasets
    if (pThisBand->GetNoDataValue() != pNextBand->GetNoDataValue())
        return true;

    return false;
}

// Assigns every band to a subdataset
void MMRDataset::AssignBandsToSubdataSets()
{
    nNSubdataSets = 0;
    if (!pfRel->GetBands())
        return;

    nNSubdataSets = 1;
    int nIBand = 0;
    pfRel->GetBand(nIBand)->AssignSubDataSet(nNSubdataSets);
    for (; nIBand < pfRel->GetNBands() - 1; nIBand++)
    {
        if (NextBandInANewDataSet(nIBand))
        {
            nNSubdataSets++;
            pfRel->GetBand(nIBand + 1)->AssignSubDataSet(nNSubdataSets);
        }
        else
            pfRel->GetBand(nIBand + 1)->AssignSubDataSet(nNSubdataSets);
    }

    // If there is only one subdataset, it means that
    // we don't need subdatasets (all assigned to 0)
    if (nNSubdataSets == 1)
    {
        nNSubdataSets = 0;
        for (nIBand = 0; nIBand < pfRel->GetNBands(); nIBand++)
            pfRel->GetBand(nIBand)->AssignSubDataSet(nNSubdataSets);
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
        for (nIBand = 0; nIBand < pfRel->GetNBands(); nIBand++)
        {
            if (pfRel->GetBand(nIBand)->GetAssignedSubDataSet() == iSubdataset)
                break;
        }

        // ·$·TODO passar els noms a una funció que determini si calen cometes.
        osDSName.Printf("MiraMonRaster:\"%s\",\"%s\"",
                        pfRel->GetBand(nIBand)->GetRELFileName().c_str(),
                        pfRel->GetBand(nIBand)->GetRawBandFileName().c_str());
        osDSDesc.Printf("Subdataset %d: \"%s\"", iSubdataset,
                        pfRel->GetBand(nIBand)->GetBandName().c_str());
        nIBand++;

        for (; nIBand < pfRel->GetNBands(); nIBand++)
        {
            if (pfRel->GetBand(nIBand)->GetAssignedSubDataSet() != iSubdataset)
                continue;

            osDSName.append(CPLSPrintf(
                ",\"%s\"",
                pfRel->GetBand(nIBand)->GetRawBandFileName().c_str()));
            osDSDesc.append(CPLSPrintf(
                ",\"%s\"", pfRel->GetBand(nIBand)->GetBandName().c_str()));
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

void MMRDataset::AssignBands()
{
    for (int nIBand = 0; nIBand < pfRel->GetNBands(); nIBand++)
    {
        if (!pfRel->GetBand(nIBand))
            continue;  // It's impoosible, but...

        // Establish raster info.
        nRasterXSize = pfRel->GetBand(nIBand)->nWidth;
        nRasterYSize = pfRel->GetBand(nIBand)->nHeight;
        GetBandBoundingBox(nIBand);  // Fills adfGeoTransform for this band(s)

        SetBand(nBands + 1, new MMRRasterBand(this, nBands + 1));

        MMRRasterBand *poBand =
            static_cast<MMRRasterBand *>(GetRasterBand(nIBand + 1));

        if (!pfRel->GetBand(nIBand)->GetFriendlyDescription().empty())
        {
            poBand->SetMetadataItem(
                "DESCRIPTION",
                pfRel->GetBand(nIBand)->GetFriendlyDescription());
        }

        // Collect GDAL custom Metadata, and "auxiliary" metadata from
        // well known MMR structures for the bands.  We defer this till
        // now to ensure that the bands are properly setup before
        // interacting with PAM.
        //·$·TODO ens saltem aixo de moment.

        /*char**papszMD = MMRGetMetadata(hMMR, i + 1);
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
    char**papszMD = MMRGetMetadata(hMMR, 0);
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
        const char* pszEU = MMRReadElevationUnit(hMMR, iBand);

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

    // Clear dirty metadata flags.
    for (int i = 0; i < nBands; i++)
    {
        MMRRasterBand *poBand =
            static_cast<MMRRasterBand *>(GetRasterBand(i + 1));
        poBand->bMetadataDirty = false;
    }
    bMetadataDirty = false;
}
