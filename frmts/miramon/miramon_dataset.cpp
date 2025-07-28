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

#include "miramon_dataset.h"
#include "miramon_rasterband.h"
#include "miramon_band.h"  // Per a MMRBand

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
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "rel");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSIONS, "rel img");

    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_SUBDATASETS, "YES");

    poDriver->pfnOpen = MMRDataset::Open;
    poDriver->pfnIdentify = MMRDataset::Identify;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}

/************************************************************************/
/*                            MMRDataset()                            */
/************************************************************************/

MMRDataset::MMRDataset(GDALOpenInfo *poOpenInfo)
{
    m_oSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

    // Creating the class MMRRel.
    auto pMMfRel = std::make_unique<MMRRel>(poOpenInfo->pszFilename);
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

    pfRel = pMMfRel.release();

    // General Dataset information available
    nRasterXSize = pfRel->GetColumnsNumberFromREL();
    nRasterYSize = pfRel->GetRowsNumberFromREL();
    ReadProjection();
    nBands = 0;

    // Assign every band to a subdataset (if any)
    // If all bands should go to a one single Subdataset, then,
    // no subdataset will be created and all bands will go to this
    // dataset.
    AssignBandsToSubdataSets();

    // Create subdatasets or add bands, as needed
    if (nNSubdataSets)
    {
        CreateSubdatasetsFromBands();
        // Fills adfGeoTransform if documented
        UpdateGeoTransform();
    }
    else
    {
        CreateRasterBands();
        // Fills adfGeoTransform if documented. If not, then gets one from last band.
        if (1 == UpdateGeoTransform())
        {
            MMRBand *poBand = pfRel->GetBand(pfRel->GetNBands() - 1);
            if (poBand)
                memcpy(&m_gt, &poBand->m_gt, sizeof(m_gt));
        }
    }

    // Make sure we don't try to do any pam stuff with this dataset.
    nPamFlags |= GPF_NOSAVE;

    // We have a valid DataSet.
    bIsValid = true;
}

/************************************************************************/
/*                           ~MMRDataset()                            */
/************************************************************************/

MMRDataset::~MMRDataset()

{
    delete pfRel;
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

void MMRDataset::CreateRasterBands()
{
    MMRBand *pBand;

    for (int nIBand = 0; nIBand < pfRel->GetNBands(); nIBand++)
    {
        // Establish raster band info.
        pBand = pfRel->GetBand(nIBand);
        if (!pBand)
            return;
        nRasterXSize = pBand->GetWidth();
        nRasterYSize = pBand->GetHeight();
        pBand->UpdateGeoTransform();  // Fills adfGeoTransform for this band

        MMRRasterBand *poRasterBand = new MMRRasterBand(this, nBands + 1);
        if (!poRasterBand->IsValid())
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Failed to create a RasterBand from '%s'",
                     pfRel->GetRELNameChar());
        }

        SetBand(nBands + 1, poRasterBand);

        MMRRasterBand *poBand =
            static_cast<MMRRasterBand *>(GetRasterBand(nIBand + 1));

        pBand = pfRel->GetBand(nIBand);
        if (!pBand)
            return;
        if (!pBand->GetFriendlyDescription().empty())
        {
            poBand->SetMetadataItem("DESCRIPTION",
                                    pBand->GetFriendlyDescription());
        }
    }
    // Some metadata items must be preserved just in case to be restored
    // if they are preserved through translations.
    pfRel->RELToGDALMetadata(this);
}

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
/*                           SUBDATASETS                                */
/************************************************************************/
// Assigns every band to a subdataset
void MMRDataset::AssignBandsToSubdataSets()
{
    nNSubdataSets = 0;
    if (!pfRel->GetBands())
        return;

    nNSubdataSets = 1;
    int nIBand = 0;
    MMRBand *pBand = pfRel->GetBand(nIBand);
    if (!pBand)
        return;

    pBand->AssignSubDataSet(nNSubdataSets);
    MMRBand *pNextBand;
    for (; nIBand < pfRel->GetNBands() - 1; nIBand++)
    {
        if (IsNextBandInANewDataSet(nIBand))
        {
            nNSubdataSets++;
            pNextBand = pfRel->GetBand(nIBand + 1);
            if (!pNextBand)
                return;
            pNextBand->AssignSubDataSet(nNSubdataSets);
        }
        else
        {
            pNextBand = pfRel->GetBand(nIBand + 1);
            if (!pNextBand)
                return;
            pNextBand->AssignSubDataSet(nNSubdataSets);
        }
    }

    // If there is only one subdataset, it means that
    // we don't need subdatasets (all assigned to 0)
    if (nNSubdataSets == 1)
    {
        nNSubdataSets = 0;
        for (nIBand = 0; nIBand < pfRel->GetNBands(); nIBand++)
        {
            pBand = pfRel->GetBand(nIBand);
            if (!pBand)
                break;
            pBand->AssignSubDataSet(nNSubdataSets);
        }
    }
}

void MMRDataset::CreateSubdatasetsFromBands()
{
    CPLStringList oSubdatasetList;
    CPLString osDSName;
    CPLString osDSDesc;
    MMRBand *pBand;

    for (int iSubdataset = 1; iSubdataset <= nNSubdataSets; iSubdataset++)
    {
        int nIBand;
        for (nIBand = 0; nIBand < pfRel->GetNBands(); nIBand++)
        {
            pBand = pfRel->GetBand(nIBand);
            if (!pBand)
                return;
            if (pBand->GetAssignedSubDataSet() == iSubdataset)
                break;
        }

        if (nIBand == pfRel->GetNBands())
            break;

        pBand = pfRel->GetBand(nIBand);
        if (!pBand)
            return;
        // ·$·TODO passar els noms a una funció que determini si calen cometes.
        osDSName.Printf("MiraMonRaster:\"%s\",\"%s\"",
                        pBand->GetRELFileName().c_str(),
                        pBand->GetRawBandFileName().c_str());
        osDSDesc.Printf("Subdataset %d: \"%s\"", iSubdataset,
                        pBand->GetBandName().c_str());
        nIBand++;

        for (; nIBand < pfRel->GetNBands(); nIBand++)
        {
            pBand = pfRel->GetBand(nIBand);
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
        // Afegir al metadades del dataset principal
        SetMetadata(oSubdatasetList.List(), "SUBDATASETS");
        oSubdatasetList.Clear();
    }
}

bool MMRDataset::IsNextBandInANewDataSet(int nIBand)
{
    if (nIBand < 0)
        return false;

    if (nIBand + 1 >= pfRel->GetNBands())
        return false;

    MMRBand *pThisBand = pfRel->GetBand(nIBand);
    MMRBand *pNextBand = pfRel->GetBand(nIBand + 1);
    if (!pThisBand || !pNextBand)
        return false;

    // Two images with different numbers of columns are assigned to different subdatasets
    if (pThisBand->GetWidth() != pNextBand->GetWidth())
        return true;

    // Two images with different numbers of rows are assigned to different subdatasets
    if (pThisBand->GetHeight() != pNextBand->GetHeight())
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

/************************************************************************/
/*                          UpdateGeoTransform()                     */
/************************************************************************/
int MMRDataset::UpdateGeoTransform()
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

    // Això va al constructor del REL
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
