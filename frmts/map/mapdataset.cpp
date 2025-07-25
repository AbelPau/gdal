/******************************************************************************
 *
 * Project:  OziExplorer .MAP Driver
 * Purpose:  GDALDataset driver for OziExplorer .MAP files
 * Author:   Jean-Claude Repetto, <jrepetto at @free dot fr>
 *
 ******************************************************************************
 * Copyright (c) 2012, Jean-Claude Repetto
 * Copyright (c) 2012, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdal_frmts.h"
#include "gdal_pam.h"
#include "gdal_proxy.h"
#include "ogr_geometry.h"
#include "ogr_spatialref.h"

/************************************************************************/
/* ==================================================================== */
/*                                MAPDataset                            */
/* ==================================================================== */
/************************************************************************/

class MAPDataset final : public GDALDataset
{
    GDALDataset *poImageDS{};

    OGRSpatialReference m_oSRS{};
    int bGeoTransformValid{};
    GDALGeoTransform m_gt{};
    int nGCPCount{};
    GDAL_GCP *pasGCPList{};
    OGRPolygon *poNeatLine{};
    CPLString osImgFilename{};

    CPL_DISALLOW_COPY_ASSIGN(MAPDataset)

  public:
    MAPDataset();
    virtual ~MAPDataset();

    const OGRSpatialReference *GetSpatialRef() const override;
    virtual CPLErr GetGeoTransform(GDALGeoTransform &gt) const override;
    virtual int GetGCPCount() override;
    const OGRSpatialReference *GetGCPSpatialRef() const override;
    virtual const GDAL_GCP *GetGCPs() override;
    virtual char **GetFileList() override;

    virtual int CloseDependentDatasets() override;

    static GDALDataset *Open(GDALOpenInfo *);
    static int Identify(GDALOpenInfo *poOpenInfo);
};

/************************************************************************/
/* ==================================================================== */
/*                         MAPWrapperRasterBand                         */
/* ==================================================================== */
/************************************************************************/
class MAPWrapperRasterBand final : public GDALProxyRasterBand
{
    GDALRasterBand *poBaseBand{};

    CPL_DISALLOW_COPY_ASSIGN(MAPWrapperRasterBand)

  protected:
    virtual GDALRasterBand *
    RefUnderlyingRasterBand(bool /*bForceOpen*/) const override;

  public:
    explicit MAPWrapperRasterBand(GDALRasterBand *poBaseBandIn)
    {
        this->poBaseBand = poBaseBandIn;
        eDataType = poBaseBand->GetRasterDataType();
        poBaseBand->GetBlockSize(&nBlockXSize, &nBlockYSize);
    }

    ~MAPWrapperRasterBand()
    {
    }
};

GDALRasterBand *
MAPWrapperRasterBand::RefUnderlyingRasterBand(bool /*bForceOpen*/) const
{
    return poBaseBand;
}

/************************************************************************/
/* ==================================================================== */
/*                             MAPDataset                               */
/* ==================================================================== */
/************************************************************************/

MAPDataset::MAPDataset()
    : poImageDS(nullptr), bGeoTransformValid(false), nGCPCount(0),
      pasGCPList(nullptr), poNeatLine(nullptr)
{
    m_oSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
}

/************************************************************************/
/*                            ~MAPDataset()                             */
/************************************************************************/

MAPDataset::~MAPDataset()

{
    if (poImageDS != nullptr)
    {
        GDALClose(poImageDS);
        poImageDS = nullptr;
    }

    if (nGCPCount)
    {
        GDALDeinitGCPs(nGCPCount, pasGCPList);
        CPLFree(pasGCPList);
    }

    if (poNeatLine != nullptr)
    {
        delete poNeatLine;
        poNeatLine = nullptr;
    }
}

/************************************************************************/
/*                       CloseDependentDatasets()                       */
/************************************************************************/

int MAPDataset::CloseDependentDatasets()
{
    int bRet = GDALDataset::CloseDependentDatasets();
    if (poImageDS != nullptr)
    {
        GDALClose(poImageDS);
        poImageDS = nullptr;
        bRet = TRUE;
    }
    return bRet;
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

int MAPDataset::Identify(GDALOpenInfo *poOpenInfo)

{
    if (poOpenInfo->nHeaderBytes < 200 ||
        !poOpenInfo->IsExtensionEqualToCI("MAP"))
        return FALSE;

    if (strstr(reinterpret_cast<const char *>(poOpenInfo->pabyHeader),
               "OziExplorer Map Data File") == nullptr)
        return FALSE;

    return TRUE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *MAPDataset::Open(GDALOpenInfo *poOpenInfo)
{
    if (!Identify(poOpenInfo))
        return nullptr;

    /* -------------------------------------------------------------------- */
    /*      Confirm the requested access is supported.                      */
    /* -------------------------------------------------------------------- */
    if (poOpenInfo->eAccess == GA_Update)
    {
        ReportUpdateNotSupportedByDriver("MAP");
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Create a corresponding GDALDataset.                             */
    /* -------------------------------------------------------------------- */

    MAPDataset *poDS = new MAPDataset();

    /* -------------------------------------------------------------------- */
    /*      Try to load and parse the .MAP file.                            */
    /* -------------------------------------------------------------------- */

    char *pszWKT = nullptr;
    bool bOziFileOK = CPL_TO_BOOL(
        GDALLoadOziMapFile(poOpenInfo->pszFilename, poDS->m_gt.data(), &pszWKT,
                           &poDS->nGCPCount, &poDS->pasGCPList));
    if (pszWKT)
    {
        poDS->m_oSRS.importFromWkt(pszWKT);
        CPLFree(pszWKT);
    }

    if (bOziFileOK && poDS->nGCPCount == 0)
        poDS->bGeoTransformValid = TRUE;

    /* We need to read again the .map file because the GDALLoadOziMapFile
       function does not returns all required data . An API change is necessary
       : maybe in GDAL 2.0 ? */

    char **papszLines = CSLLoad2(poOpenInfo->pszFilename, 200, 200, nullptr);

    if (!papszLines)
    {
        delete poDS;
        return nullptr;
    }

    const int nLines = CSLCount(papszLines);
    if (nLines < 3)
    {
        delete poDS;
        CSLDestroy(papszLines);
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      We need to open the image in order to establish                 */
    /*      details like the band count and types.                          */
    /* -------------------------------------------------------------------- */
    poDS->osImgFilename = papszLines[2];

    const CPLString osPath = CPLGetPathSafe(poOpenInfo->pszFilename);
    if (CPLIsFilenameRelative(poDS->osImgFilename))
    {
        poDS->osImgFilename =
            CPLFormCIFilenameSafe(osPath, poDS->osImgFilename, nullptr);
    }
    else
    {
        VSIStatBufL sStat;
        if (VSIStatL(poDS->osImgFilename, &sStat) != 0)
        {
            poDS->osImgFilename = CPLGetFilename(poDS->osImgFilename);
            poDS->osImgFilename =
                CPLFormCIFilenameSafe(osPath, poDS->osImgFilename, nullptr);
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Try and open the file.                                          */
    /* -------------------------------------------------------------------- */
    poDS->poImageDS =
        GDALDataset::FromHandle(GDALOpen(poDS->osImgFilename, GA_ReadOnly));
    if (poDS->poImageDS == nullptr || poDS->poImageDS->GetRasterCount() == 0)
    {
        CSLDestroy(papszLines);
        delete poDS;
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Attach the bands.                                               */
    /* -------------------------------------------------------------------- */
    poDS->nRasterXSize = poDS->poImageDS->GetRasterXSize();
    poDS->nRasterYSize = poDS->poImageDS->GetRasterYSize();
    if (!GDALCheckDatasetDimensions(poDS->nRasterXSize, poDS->nRasterYSize))
    {
        CSLDestroy(papszLines);
        GDALClose(poDS->poImageDS);
        delete poDS;
        return nullptr;
    }

    for (int iBand = 1; iBand <= poDS->poImageDS->GetRasterCount(); iBand++)
        poDS->SetBand(iBand, new MAPWrapperRasterBand(
                                 poDS->poImageDS->GetRasterBand(iBand)));

    /* -------------------------------------------------------------------- */
    /*      Add the neatline/cutline, if required                           */
    /* -------------------------------------------------------------------- */

    /* First, we need to check if it is necessary to define a neatline */
    bool bNeatLine = false;
    for (int iLine = 10; iLine < nLines; iLine++)
    {
        if (STARTS_WITH_CI(papszLines[iLine], "MMPXY,"))
        {
            char **papszTok =
                CSLTokenizeString2(papszLines[iLine], ",",
                                   CSLT_STRIPLEADSPACES | CSLT_STRIPENDSPACES);

            if (CSLCount(papszTok) != 4)
            {
                CSLDestroy(papszTok);
                continue;
            }

            const int x = atoi(papszTok[2]);
            const int y = atoi(papszTok[3]);
            if ((x != 0 && x != poDS->nRasterXSize) ||
                (y != 0 && y != poDS->nRasterYSize))
            {
                bNeatLine = true;
                CSLDestroy(papszTok);
                break;
            }
            CSLDestroy(papszTok);
        }
    }

    /* Create and fill the neatline polygon */
    if (bNeatLine)
    {
        poDS->poNeatLine =
            new OGRPolygon(); /* Create a polygon to store the neatline */
        OGRLinearRing *poRing = new OGRLinearRing();

        if (poDS->bGeoTransformValid) /* Compute the projected coordinates of
                                         the corners */
        {
            for (int iLine = 10; iLine < nLines; iLine++)
            {
                if (STARTS_WITH_CI(papszLines[iLine], "MMPXY,"))
                {
                    char **papszTok = CSLTokenizeString2(
                        papszLines[iLine], ",",
                        CSLT_STRIPLEADSPACES | CSLT_STRIPENDSPACES);

                    if (CSLCount(papszTok) != 4)
                    {
                        CSLDestroy(papszTok);
                        continue;
                    }

                    const double x = CPLAtofM(papszTok[2]);
                    const double y = CPLAtofM(papszTok[3]);
                    const double X =
                        poDS->m_gt[0] + x * poDS->m_gt[1] + y * poDS->m_gt[2];
                    const double Y =
                        poDS->m_gt[3] + x * poDS->m_gt[4] + y * poDS->m_gt[5];
                    poRing->addPoint(X, Y);
                    CPLDebug("CORNER MMPXY", "%f, %f, %f, %f", x, y, X, Y);
                    CSLDestroy(papszTok);
                }
            }
        }
        else /* Convert the geographic coordinates to projected coordinates */
        {
            OGRCoordinateTransformation *poTransform = nullptr;
            if (!poDS->m_oSRS.IsEmpty())
            {
                OGRSpatialReference *poLongLat = poDS->m_oSRS.CloneGeogCS();
                if (poLongLat)
                {
                    poLongLat->SetAxisMappingStrategy(
                        OAMS_TRADITIONAL_GIS_ORDER);
                    poTransform = OGRCreateCoordinateTransformation(
                        poLongLat, &poDS->m_oSRS);
                    delete poLongLat;
                }
            }

            for (int iLine = 10; iLine < nLines; iLine++)
            {
                if (STARTS_WITH_CI(papszLines[iLine], "MMPLL,"))
                {
                    CPLDebug("MMPLL", "%s", papszLines[iLine]);

                    char **papszTok = CSLTokenizeString2(
                        papszLines[iLine], ",",
                        CSLT_STRIPLEADSPACES | CSLT_STRIPENDSPACES);

                    if (CSLCount(papszTok) != 4)
                    {
                        CSLDestroy(papszTok);
                        continue;
                    }

                    double dfLon = CPLAtofM(papszTok[2]);
                    double dfLat = CPLAtofM(papszTok[3]);

                    if (poTransform)
                        poTransform->Transform(1, &dfLon, &dfLat);
                    poRing->addPoint(dfLon, dfLat);
                    CPLDebug("CORNER MMPLL", "%f, %f", dfLon, dfLat);
                    CSLDestroy(papszTok);
                }
            }
            if (poTransform)
                delete poTransform;
        }

        poRing->closeRings();
        poDS->poNeatLine->addRingDirectly(poRing);

        char *pszNeatLineWkt = nullptr;
        poDS->poNeatLine->exportToWkt(&pszNeatLineWkt);
        CPLDebug("NEATLINE", "%s", pszNeatLineWkt);
        poDS->SetMetadataItem("NEATLINE", pszNeatLineWkt);
        CPLFree(pszNeatLineWkt);
    }

    CSLDestroy(papszLines);

    return poDS;
}

/************************************************************************/
/*                          GetSpatialRef()                             */
/************************************************************************/

const OGRSpatialReference *MAPDataset::GetSpatialRef() const
{
    return (!m_oSRS.IsEmpty() && nGCPCount == 0) ? &m_oSRS : nullptr;
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr MAPDataset::GetGeoTransform(GDALGeoTransform &gt) const

{
    gt = m_gt;

    return (nGCPCount == 0) ? CE_None : CE_Failure;
}

/************************************************************************/
/*                           GetGCPCount()                              */
/************************************************************************/

int MAPDataset::GetGCPCount()
{
    return nGCPCount;
}

/************************************************************************/
/*                          GetGCPSpatialRef()                          */
/************************************************************************/

const OGRSpatialReference *MAPDataset::GetGCPSpatialRef() const
{
    return (!m_oSRS.IsEmpty() && nGCPCount != 0) ? &m_oSRS : nullptr;
}

/************************************************************************/
/*                               GetGCPs()                              */
/************************************************************************/

const GDAL_GCP *MAPDataset::GetGCPs()
{
    return pasGCPList;
}

/************************************************************************/
/*                            GetFileList()                             */
/************************************************************************/

char **MAPDataset::GetFileList()
{
    char **papszFileList = GDALDataset::GetFileList();

    papszFileList = CSLAddString(papszFileList, osImgFilename);

    return papszFileList;
}

/************************************************************************/
/*                          GDALRegister_MAP()                          */
/************************************************************************/

void GDALRegister_MAP()

{
    if (GDALGetDriverByName("MAP") != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription("MAP");
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "OziExplorer .MAP");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/raster/map.html");

    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");

    poDriver->pfnOpen = MAPDataset::Open;
    poDriver->pfnIdentify = MAPDataset::Identify;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
