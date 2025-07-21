/******************************************************************************
 *
 * Project:  MiraMon Raster Driver
 * Purpose:  Implements MMRDataset and MMRRasterBand class 
 * Author:   Abel Pau
 *
 ******************************************************************************
 * Copyright (c) 2025, Xavier Pons
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef MMRDATASET_H_INCLUDED
#define MMRDATASET_H_INCLUDED

#include <cstddef>
#include <vector>
#include <optional>
#include <array>

#ifdef MSVC
#include "..\miramon_common\mm_gdal_constants.h"  // For MM_EXT_DBF_N_FIELDS
#else
#include "../miramon_common/mm_gdal_constants.h"  // For MM_EXT_DBF_N_FIELDS
#endif

#include "gdal_pam.h"
#include "gdal_rat.h"
#include "miramon_info.h"  // For MMRInfo

/************************************************************************/
/* ==================================================================== */
/*                              MMRDataset                              */
/* ==================================================================== */
/************************************************************************/

class MMRRasterBand;

class MMRDataset final : public GDALPamDataset
{
  public:
    explicit MMRDataset(GDALOpenInfo *poOpenInfo);
    MMRDataset(const MMRDataset &) =
        delete;  // I don't want to construct a MMRDataset from another MMRDataset (effc++)
    MMRDataset &operator=(const MMRDataset &) =
        delete;  // I don't want to assing a MMRDataset to another MMRDataset (effc++)
    virtual ~MMRDataset();

    static int Identify(GDALOpenInfo *);
    static GDALDataset *Open(GDALOpenInfo *);

    void AssignBandsToSubdataSets();
    void CreateSubdatasetsFromBands();
    void AssignBands();

    int GetDataSetBoundingBox();
    int GetBandBoundingBox(int nIBand);

    MMRInfo *GetMMRInfo()
    {
        return hMMR;
    }

    const OGRSpatialReference *GetSpatialRef() const override;
    virtual CPLErr GetGeoTransform(GDALGeoTransform &gt) const override;
    virtual CPLErr SetMetadata(char **, const char * = "") override;
    virtual CPLErr SetMetadataItem(const char *, const char *,
                                   const char * = "") override;

  private:
    GDALGeoTransform m_gt{};
    OGRSpatialReference m_oSRS{};
    CPLErr ReadProjection();

    bool NextBandInANewDataSet(int nIBand);

    MMRInfo *hMMR = nullptr;  //owner

    bool bMetadataDirty = false;
    std::vector<gdal::GCP> m_aoGCPs{};

    // Numbers of subdatasets (if any) in this dataset.
    int nNSubdataSets = 0;
};

#endif  // MMRDATASET_H_INCLUDED
