/******************************************************************************
 *
 * Project:  MiraMon Raster Driver
 * Purpose:  Implements MMRDataset class: responsible for generating the
 *           main dataset or the subdatasets as needed.
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

#include "gdal_pam.h"
#include "gdal_rat.h"

#include "../miramon_common/mm_gdal_constants.h"  // For MM_EXT_DBF_N_FIELDS
#include "miramon_rel.h"

/* ==================================================================== */
/*                              MMRDataset                              */
/* ==================================================================== */

class MMRRasterBand;
class MMRRel;

class MMRDataset final : public GDALPamDataset
{
  public:
    explicit MMRDataset(GDALOpenInfo *poOpenInfo);  // Used in reading
    MMRDataset(CPLString osFilename, GDALDataset &oSrcDS,
               bool bCompress);  // Used in writing
    MMRDataset(const MMRDataset &) =
        delete;  // I don't want to construct a MMRDataset from another MMRDataset (effc++)
    MMRDataset &operator=(const MMRDataset &) =
        delete;  // I don't want to assign a MMRDataset to another MMRDataset (effc++)
    ~MMRDataset() override;

    static int Identify(GDALOpenInfo *);
    static GDALDataset *Open(GDALOpenInfo *);
    static GDALDataset *CreateCopy(const char *pszFilename,
                                   GDALDataset *poSrcDS, int bStrict,
                                   char **papszOptions,
                                   GDALProgressFunc pfnProgress,
                                   void *pProgressData);

    MMRRel *GetRel()
    {
        return m_pMMRRel.get();
    }

    static constexpr int DEFAULT_COLOR_TABLE_MULTIPLIER_257 = 257;

  private:
    void ReadProjection();
    void AssignBandsToSubdataSets();
    void CreateSubdatasetsFromBands();
    bool CreateRasterBands();
    bool IsNextBandInANewDataSet(int nIBand) const;

    int UpdateGeoTransform();
    const OGRSpatialReference *GetSpatialRef() const override;
    CPLErr GetGeoTransform(GDALGeoTransform &gt) const override;
    static CPLString
    CreateAssociatedMetadataFileName(const CPLString &osFileName);

    bool IsValid() const
    {
        return m_bIsValid;
    }

    GDALGeoTransform m_gt{};
    OGRSpatialReference m_oSRS{};

    bool m_bIsValid =
        false;  // Determines if the created object is valid or not.
    std::unique_ptr<MMRRel> m_pMMRRel = nullptr;

    std::vector<gdal::GCP> m_aoGCPs{};

    // Numbers of subdatasets (if any) in this dataset.
    int m_nNSubdataSets = 0;

    // For writing part
    //
    // EPSG number
    CPLString m_osEPSG = "";
    // Global raster dimensions
    int m_nWidth = 0;
    int m_nHeight = 0;

    double m_dfMinX = MM_UNDEFINED_STATISTICAL_VALUE;
    double m_dfMaxX = -MM_UNDEFINED_STATISTICAL_VALUE;
    double m_dfMinY = MM_UNDEFINED_STATISTICAL_VALUE;
    double m_dfMaxY = -MM_UNDEFINED_STATISTICAL_VALUE;
};

#endif  // MMRDATASET_H_INCLUDED
