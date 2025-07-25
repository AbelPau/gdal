/******************************************************************************
 *
 * Project:  SRTM HGT Driver
 * Purpose:  SRTM HGT File Read Support.
 *           http://dds.cr.usgs.gov/srtm/version2_1/Documentation/SRTM_Topo.pdf
 *           http://www2.jpl.nasa.gov/srtm/faq.html
 *           http://dds.cr.usgs.gov/srtm/version2_1
 * Authors:  Michael Mazzella, Even Rouault
 *
 ******************************************************************************
 * Copyright (c) 2005, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2007-2011, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_port.h"
#include "cpl_string.h"
#include "gdal_frmts.h"
#include "gdal_pam.h"
#include "ogr_spatialref.h"

#include <cmath>

constexpr GInt16 SRTMHG_NODATA_VALUE = -32768;

/************************************************************************/
/* ==================================================================== */
/*                              SRTMHGTDataset                          */
/* ==================================================================== */
/************************************************************************/

class SRTMHGTRasterBand;

class SRTMHGTDataset final : public GDALPamDataset
{
    friend class SRTMHGTRasterBand;

    VSILFILE *fpImage = nullptr;
    GDALGeoTransform m_gt{};
    GByte *pabyBuffer = nullptr;
    OGRSpatialReference m_oSRS{};

    static GDALPamDataset *OpenPAM(GDALOpenInfo *);

  public:
    SRTMHGTDataset();
    virtual ~SRTMHGTDataset();

    const OGRSpatialReference *GetSpatialRef() const override;
    virtual CPLErr GetGeoTransform(GDALGeoTransform &gt) const override;

    static int Identify(GDALOpenInfo *poOpenInfo);
    static GDALDataset *Open(GDALOpenInfo *);
    static GDALDataset *CreateCopy(const char *pszFilename,
                                   GDALDataset *poSrcDS, int bStrict,
                                   char **papszOptions,
                                   GDALProgressFunc pfnProgress,
                                   void *pProgressData);
};

/************************************************************************/
/* ==================================================================== */
/*                            SRTMHGTRasterBand                             */
/* ==================================================================== */
/************************************************************************/

class SRTMHGTRasterBand final : public GDALPamRasterBand
{
    friend class SRTMHGTDataset;

    int bNoDataSet;
    double dfNoDataValue;

  public:
    SRTMHGTRasterBand(SRTMHGTDataset *, int, GDALDataType);

    virtual CPLErr IReadBlock(int, int, void *) override;
    virtual CPLErr IWriteBlock(int nBlockXOff, int nBlockYOff,
                               void *pImage) override;

    virtual GDALColorInterp GetColorInterpretation() override;

    virtual double GetNoDataValue(int *pbSuccess = nullptr) override;

    virtual const char *GetUnitType() override;
};

/************************************************************************/
/*                         SRTMHGTRasterBand()                          */
/************************************************************************/

SRTMHGTRasterBand::SRTMHGTRasterBand(SRTMHGTDataset *poDSIn, int nBandIn,
                                     GDALDataType eDT)
    : bNoDataSet(TRUE), dfNoDataValue(SRTMHG_NODATA_VALUE)
{
    poDS = poDSIn;
    nBand = nBandIn;
    eDataType = eDT;
    nBlockXSize = poDSIn->nRasterXSize;
    nBlockYSize = 1;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr SRTMHGTRasterBand::IReadBlock(int /*nBlockXOff*/, int nBlockYOff,
                                     void *pImage)
{
    SRTMHGTDataset *poGDS = reinterpret_cast<SRTMHGTDataset *>(poDS);

    /* -------------------------------------------------------------------- */
    /*      Load the desired data into the working buffer.                  */
    /* -------------------------------------------------------------------- */
    const int nDTSize = GDALGetDataTypeSizeBytes(eDataType);
    VSIFSeekL(poGDS->fpImage,
              static_cast<size_t>(nBlockYOff) * nBlockXSize * nDTSize,
              SEEK_SET);
    VSIFReadL((unsigned char *)pImage, nBlockXSize, nDTSize, poGDS->fpImage);
#ifdef CPL_LSB
    GDALSwapWords(pImage, nDTSize, nBlockXSize, nDTSize);
#endif

    return CE_None;
}

/************************************************************************/
/*                             IWriteBlock()                            */
/************************************************************************/

CPLErr SRTMHGTRasterBand::IWriteBlock(int /*nBlockXOff*/, int nBlockYOff,
                                      void *pImage)
{
    SRTMHGTDataset *poGDS = reinterpret_cast<SRTMHGTDataset *>(poDS);

    if (poGDS->eAccess != GA_Update)
        return CE_Failure;

    const int nDTSize = GDALGetDataTypeSizeBytes(eDataType);
    VSIFSeekL(poGDS->fpImage,
              static_cast<size_t>(nBlockYOff) * nBlockXSize * nDTSize,
              SEEK_SET);

#ifdef CPL_LSB
    if (nDTSize > 1)
    {
        memcpy(poGDS->pabyBuffer, pImage,
               static_cast<size_t>(nBlockXSize) * nDTSize);
        GDALSwapWords(poGDS->pabyBuffer, nDTSize, nBlockXSize, nDTSize);
        VSIFWriteL(reinterpret_cast<unsigned char *>(poGDS->pabyBuffer),
                   nBlockXSize, nDTSize, poGDS->fpImage);
    }
    else
#endif
    {
        VSIFWriteL(reinterpret_cast<unsigned char *>(pImage), nBlockXSize,
                   nDTSize, poGDS->fpImage);
    }

    return CE_None;
}

/************************************************************************/
/*                           GetNoDataValue()                           */
/************************************************************************/

double SRTMHGTRasterBand::GetNoDataValue(int *pbSuccess)

{
    if (eDataType == GDT_Byte)
        return GDALPamRasterBand::GetNoDataValue(pbSuccess);

    if (pbSuccess)
        *pbSuccess = bNoDataSet;

    return dfNoDataValue;
}

/************************************************************************/
/*                             GetUnitType()                            */
/************************************************************************/

const char *SRTMHGTRasterBand::GetUnitType()
{
    const std::string osExt = CPLGetExtensionSafe(poDS->GetDescription());
    const char *pszExt = osExt.c_str();
    if (EQUAL(pszExt, "err") || EQUAL(pszExt, "img") || EQUAL(pszExt, "num") ||
        EQUAL(pszExt, "swb"))
    {
        return "";
    }
    return "m";
}

/************************************************************************/
/*                       GetColorInterpretation()                       */
/************************************************************************/

GDALColorInterp SRTMHGTRasterBand::GetColorInterpretation()
{
    return GCI_Undefined;
}

/************************************************************************/
/* ==================================================================== */
/*                             SRTMHGTDataset                               */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                            SRTMHGTDataset()                              */
/************************************************************************/

SRTMHGTDataset::SRTMHGTDataset()
{
    m_oSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    if (CPLTestBool(CPLGetConfigOption("REPORT_COMPD_CS", "NO")))
    {
        m_oSRS.importFromWkt(
            "COMPD_CS[\"WGS 84 + EGM96 geoid height\", GEOGCS[\"WGS 84\", "
            "DATUM[\"WGS_1984\", SPHEROID[\"WGS 84\",6378137,298.257223563, "
            "AUTHORITY[\"EPSG\",\"7030\"]], AUTHORITY[\"EPSG\",\"6326\"]], "
            "PRIMEM[\"Greenwich\",0, AUTHORITY[\"EPSG\",\"8901\"]], "
            "UNIT[\"degree\",0.0174532925199433, "
            "AUTHORITY[\"EPSG\",\"9122\"]], AUTHORITY[\"EPSG\",\"4326\"]], "
            "VERT_CS[\"EGM96 geoid height\", VERT_DATUM[\"EGM96 geoid\",2005, "
            "AUTHORITY[\"EPSG\",\"5171\"]], UNIT[\"metre\",1, "
            "AUTHORITY[\"EPSG\",\"9001\"]], AXIS[\"Up\",UP], "
            "AUTHORITY[\"EPSG\",\"5773\"]]]");
    }
    else
    {
        m_oSRS.importFromWkt(SRS_WKT_WGS84_LAT_LONG);
    }
}

/************************************************************************/
/*                           ~SRTMHGTDataset()                            */
/************************************************************************/

SRTMHGTDataset::~SRTMHGTDataset()
{
    FlushCache(true);
    if (fpImage != nullptr)
        VSIFCloseL(fpImage);
    CPLFree(pabyBuffer);
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr SRTMHGTDataset::GetGeoTransform(GDALGeoTransform &gt) const
{
    gt = m_gt;
    return CE_None;
}

/************************************************************************/
/*                          GetSpatialRef()                             */
/************************************************************************/

const OGRSpatialReference *SRTMHGTDataset::GetSpatialRef() const

{
    return &m_oSRS;
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

int SRTMHGTDataset::Identify(GDALOpenInfo *poOpenInfo)

{
    const char *fileName = CPLGetFilename(poOpenInfo->pszFilename);
    if (strlen(fileName) < 11 || fileName[7] != '.')
        return FALSE;
    CPLString osLCFilename(CPLString(fileName).tolower());
    if ((osLCFilename[0] != 'n' && osLCFilename[0] != 's') ||
        (osLCFilename[3] != 'e' && osLCFilename[3] != 'w'))
        return FALSE;
    if (!STARTS_WITH(fileName, "/vsizip/") && osLCFilename.endsWith(".hgt.zip"))
    {
        CPLString osNewName("/vsizip/");
        osNewName += poOpenInfo->pszFilename;
        osNewName += "/";
        osNewName += CPLString(fileName).substr(0, 7);
        osNewName += ".hgt";
        GDALOpenInfo oOpenInfo(osNewName, GA_ReadOnly);
        return Identify(&oOpenInfo);
    }

    if (!STARTS_WITH(fileName, "/vsizip/") &&
        osLCFilename.endsWith(".srtmswbd.raw.zip"))
    {
        CPLString osNewName("/vsizip/");
        osNewName += poOpenInfo->pszFilename;
        osNewName += "/";
        osNewName += CPLString(fileName).substr(0, 7);
        osNewName += ".raw";
        GDALOpenInfo oOpenInfo(osNewName, GA_ReadOnly);
        return Identify(&oOpenInfo);
    }

    // .hgts and .err files from
    // https://e4ftl01.cr.usgs.gov/MEASURES/NASADEM_SHHP.001/2000.02.11/ .img
    // and .img.num files from
    // https://e4ftl01.cr.usgs.gov/MEASURES/NASADEM_SIM.001/2000.02.11/
    if (!osLCFilename.endsWith(".hgt") && !osLCFilename.endsWith(".hgts") &&
        !osLCFilename.endsWith(".err") && !osLCFilename.endsWith(".img") &&
        !osLCFilename.endsWith(".num") &&  // .img.num or .num
        !osLCFilename.endsWith(".raw") &&
        !osLCFilename.endsWith(
            ".swb") &&  // https://e4ftl01.cr.usgs.gov/MEASURES/NASADEM_HGT.001/2000.02.11/
        !osLCFilename.endsWith(".hgt.gz"))
        return FALSE;

    /* -------------------------------------------------------------------- */
    /*      We check the file size to see if it is                          */
    /*      SRTM1 (below or above lat 50) or SRTM 3                         */
    /* -------------------------------------------------------------------- */
    VSIStatBufL fileStat;

    if (VSIStatL(poOpenInfo->pszFilename, &fileStat) != 0)
        return FALSE;
    if (fileStat.st_size != 1201 * 1201 * 2 &&
        fileStat.st_size != 1801 * 3601 * 2 &&
        fileStat.st_size != 3601 * 3601 &&
        fileStat.st_size != 3601 * 3601 * 2 &&
        fileStat.st_size != 3601 * 3601 * 4 &&  // .hgts
        fileStat.st_size != 7201 * 7201 * 2)
        return FALSE;

    return TRUE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *SRTMHGTDataset::Open(GDALOpenInfo *poOpenInfo)
{
    return OpenPAM(poOpenInfo);
}

/************************************************************************/
/*                              OpenPAM()                               */
/************************************************************************/

GDALPamDataset *SRTMHGTDataset::OpenPAM(GDALOpenInfo *poOpenInfo)
{
    if (!Identify(poOpenInfo))
        return nullptr;

    const char *fileName = CPLGetFilename(poOpenInfo->pszFilename);
    CPLString osLCFilename(CPLString(fileName).tolower());
    if (!STARTS_WITH(fileName, "/vsizip/") && osLCFilename.endsWith(".hgt.zip"))
    {
        CPLString osFilename("/vsizip/");
        osFilename += poOpenInfo->pszFilename;
        osFilename += "/";
        osFilename += CPLString(fileName).substr(0, 7);
        osFilename += ".hgt";
        GDALOpenInfo oOpenInfo(osFilename, poOpenInfo->eAccess);
        auto poDS = OpenPAM(&oOpenInfo);
        if (poDS != nullptr)
        {
            // override description with the main one
            poDS->SetDescription(poOpenInfo->pszFilename);
        }
        return poDS;
    }

    if (!STARTS_WITH(fileName, "/vsizip/") &&
        osLCFilename.endsWith(".srtmswbd.raw.zip"))
    {
        CPLString osFilename("/vsizip/");
        osFilename += poOpenInfo->pszFilename;
        osFilename += "/";
        osFilename += CPLString(fileName).substr(0, 7);
        osFilename += ".raw";
        GDALOpenInfo oOpenInfo(osFilename, poOpenInfo->eAccess);
        auto poDS = OpenPAM(&oOpenInfo);
        if (poDS != nullptr)
        {
            // override description with the main one
            poDS->SetDescription(poOpenInfo->pszFilename);
        }
        return poDS;
    }

    char latLonValueString[4];
    memset(latLonValueString, 0, 4);
    strncpy(latLonValueString, &fileName[1], 2);
    int southWestLat = atoi(latLonValueString);
    memset(latLonValueString, 0, 4);
    // cppcheck-suppress redundantCopy
    strncpy(latLonValueString, &fileName[4], 3);
    int southWestLon = atoi(latLonValueString);

    if (fileName[0] == 'N' || fileName[0] == 'n')
        /*southWestLat = southWestLat */;
    else if (fileName[0] == 'S' || fileName[0] == 's')
        southWestLat = southWestLat * -1;
    else
        return nullptr;

    if (fileName[3] == 'E' || fileName[3] == 'e')
        /*southWestLon = southWestLon */;
    else if (fileName[3] == 'W' || fileName[3] == 'w')
        southWestLon = southWestLon * -1;
    else
        return nullptr;

    /* -------------------------------------------------------------------- */
    /*      Create a corresponding GDALDataset.                             */
    /* -------------------------------------------------------------------- */
    auto poDS = std::make_unique<SRTMHGTDataset>();

    poDS->fpImage = poOpenInfo->fpL;
    poOpenInfo->fpL = nullptr;

    VSIStatBufL fileStat;
    if (VSIStatL(poOpenInfo->pszFilename, &fileStat) != 0)
    {
        return nullptr;
    }

    int numPixels_x, numPixels_y;

    GDALDataType eDT = GDT_Int16;
    switch (fileStat.st_size)
    {
        case 1201 * 1201 * 2:
            numPixels_x = numPixels_y = 1201;
            break;
        case 1801 * 3601 * 2:
            numPixels_x = 1801;
            numPixels_y = 3601;
            break;
        case 3601 * 3601:
            numPixels_x = numPixels_y = 3601;
            eDT = GDT_Byte;
            break;
        case 3601 * 3601 * 2:
            numPixels_x = numPixels_y = 3601;
            break;
        case 3601 * 3601 * 4:  // .hgts
            numPixels_x = numPixels_y = 3601;
            eDT = GDT_Float32;
            break;
        case 7201 * 7201 * 2:
            numPixels_x = numPixels_y = 7201;
            break;
        default:
            numPixels_x = numPixels_y = 0;
            break;
    }

    poDS->eAccess = poOpenInfo->eAccess;
#ifdef CPL_LSB
    if (poDS->eAccess == GA_Update && eDT != GDT_Byte)
    {
        poDS->pabyBuffer =
            static_cast<GByte *>(CPLMalloc(numPixels_x * sizeof(eDT)));
    }
#endif

    /* -------------------------------------------------------------------- */
    /*      Capture some information from the file that is of interest.     */
    /* -------------------------------------------------------------------- */
    poDS->nRasterXSize = numPixels_x;
    poDS->nRasterYSize = numPixels_y;
    poDS->nBands = 1;

    poDS->m_gt[0] = southWestLon - 0.5 / (numPixels_x - 1);
    poDS->m_gt[1] = 1.0 / (numPixels_x - 1);
    poDS->m_gt[2] = 0.0;
    poDS->m_gt[3] = southWestLat + 1 + 0.5 / (numPixels_y - 1);
    poDS->m_gt[4] = 0.0;
    poDS->m_gt[5] = -1.0 / (numPixels_y - 1);

    poDS->SetMetadataItem(GDALMD_AREA_OR_POINT, GDALMD_AOP_POINT);

    /* -------------------------------------------------------------------- */
    /*      Create band information object.                                 */
    /* -------------------------------------------------------------------- */
    SRTMHGTRasterBand *tmpBand = new SRTMHGTRasterBand(poDS.get(), 1, eDT);
    poDS->SetBand(1, tmpBand);

    /* -------------------------------------------------------------------- */
    /*      Initialize any PAM information.                                 */
    /* -------------------------------------------------------------------- */
    poDS->SetDescription(poOpenInfo->pszFilename);
    poDS->TryLoadXML();

    /* -------------------------------------------------------------------- */
    /*      Support overviews.                                              */
    /* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize(poDS.get(), poOpenInfo->pszFilename);

    return poDS.release();
}

/************************************************************************/
/*                              CreateCopy()                            */
/************************************************************************/

GDALDataset *SRTMHGTDataset::CreateCopy(const char *pszFilename,
                                        GDALDataset *poSrcDS, int bStrict,
                                        char ** /* papszOptions*/,
                                        GDALProgressFunc pfnProgress,
                                        void *pProgressData)
{
    /* -------------------------------------------------------------------- */
    /*      Some some rudimentary checks                                    */
    /* -------------------------------------------------------------------- */
    const int nBands = poSrcDS->GetRasterCount();
    if (nBands == 0)
    {
        CPLError(
            CE_Failure, CPLE_NotSupported,
            "SRTMHGT driver does not support source dataset with zero band.\n");
        return nullptr;
    }
    else if (nBands != 1)
    {
        CPLError((bStrict) ? CE_Failure : CE_Warning, CPLE_NotSupported,
                 "SRTMHGT driver only uses the first band of the dataset.\n");
        if (bStrict)
            return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Checks the input SRS                                            */
    /* -------------------------------------------------------------------- */
    OGRSpatialReference ogrsr_input;
    ogrsr_input.importFromWkt(poSrcDS->GetProjectionRef());

    OGRSpatialReference ogrsr_wgs84;
    ogrsr_wgs84.SetWellKnownGeogCS("WGS84");

    if (ogrsr_input.IsSameGeogCS(&ogrsr_wgs84) == FALSE)
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "The source projection coordinate system is %s. Only WGS 84 "
                 "is supported.\nThe SRTMHGT driver will generate a file as "
                 "if the source was WGS 84 projection coordinate system.",
                 poSrcDS->GetProjectionRef());
    }

    /* -------------------------------------------------------------------- */
    /*      Work out the LL origin.                                         */
    /* -------------------------------------------------------------------- */
    GDALGeoTransform gt;
    if (poSrcDS->GetGeoTransform(gt) != CE_None)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Source image must have a geo transform matrix.");
        return nullptr;
    }

    const int nLLOriginLat = static_cast<int>(
        std::floor(gt[3] + poSrcDS->GetRasterYSize() * gt[5] + 0.5));

    int nLLOriginLong = static_cast<int>(std::floor(gt[0] + 0.5));

    if (std::abs(nLLOriginLat -
                 (gt[3] + (poSrcDS->GetRasterYSize() - 0.5) * gt[5])) > 1e-10 ||
        std::abs(nLLOriginLong - (gt[0] + 0.5 * gt[1])) > 1e-10)
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "The corner coordinates of the source are not properly "
                 "aligned on plain latitude/longitude boundaries.");
    }

    /* -------------------------------------------------------------------- */
    /*      Check image dimensions.                                         */
    /* -------------------------------------------------------------------- */
    const int nXSize = poSrcDS->GetRasterXSize();
    const int nYSize = poSrcDS->GetRasterYSize();

    if (!((nXSize == 1201 && nYSize == 1201) ||
          (nXSize == 1801 && nYSize == 3601) ||
          (nXSize == 3601 && nYSize == 3601) ||
          (nXSize == 7201 && nYSize == 7201)))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Image dimensions should be 1201x1201, 1801x3601, 3601x3601 "
                 "or 7201x7201.");
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Check filename.                                                 */
    /* -------------------------------------------------------------------- */
    char expectedFileName[12];

    CPLsnprintf(expectedFileName, sizeof(expectedFileName), "%c%02d%c%03d.HGT",
                (nLLOriginLat >= 0) ? 'N' : 'S',
                (nLLOriginLat >= 0) ? nLLOriginLat : -nLLOriginLat,
                (nLLOriginLong >= 0) ? 'E' : 'W',
                (nLLOriginLong >= 0) ? nLLOriginLong : -nLLOriginLong);

    if (!EQUAL(expectedFileName, CPLGetFilename(pszFilename)))
    {
        CPLError(CE_Warning, CPLE_AppDefined, "Expected output filename is %s.",
                 expectedFileName);
    }

    /* -------------------------------------------------------------------- */
    /*      Write output file.                                              */
    /* -------------------------------------------------------------------- */
    VSILFILE *fp = VSIFOpenL(pszFilename, "wb");
    if (fp == nullptr)
    {
        CPLError(CE_Failure, CPLE_FileIO, "Cannot create file %s", pszFilename);
        return nullptr;
    }

    GInt16 *panData =
        reinterpret_cast<GInt16 *>(CPLMalloc(sizeof(GInt16) * nXSize));
    GDALRasterBand *poSrcBand = poSrcDS->GetRasterBand(1);

    int bSrcBandHasNoData;
    double srcBandNoData = poSrcBand->GetNoDataValue(&bSrcBandHasNoData);

    for (int iY = 0; iY < nYSize; iY++)
    {
        if (poSrcBand->RasterIO(GF_Read, 0, iY, nXSize, 1,
                                reinterpret_cast<void *>(panData), nXSize, 1,
                                GDT_Int16, 0, 0, nullptr) != CE_None)
        {
            VSIFCloseL(fp);
            CPLFree(panData);
            return nullptr;
        }

        /* Translate nodata values */
        if (bSrcBandHasNoData && srcBandNoData != SRTMHG_NODATA_VALUE)
        {
            for (int iX = 0; iX < nXSize; iX++)
            {
                if (panData[iX] == srcBandNoData)
                    panData[iX] = SRTMHG_NODATA_VALUE;
            }
        }

#ifdef CPL_LSB
        GDALSwapWords(panData, 2, nXSize, 2);
#endif

        if (VSIFWriteL(panData, sizeof(GInt16) * nXSize, 1, fp) != 1)
        {
            CPLError(CE_Failure, CPLE_FileIO,
                     "Failed to write line %d in SRTMHGT dataset.\n", iY);
            VSIFCloseL(fp);
            CPLFree(panData);
            return nullptr;
        }

        if (pfnProgress && !pfnProgress((iY + 1) / static_cast<double>(nYSize),
                                        nullptr, pProgressData))
        {
            CPLError(CE_Failure, CPLE_UserInterrupt,
                     "User terminated CreateCopy()");
            VSIFCloseL(fp);
            CPLFree(panData);
            return nullptr;
        }
    }

    CPLFree(panData);
    VSIFCloseL(fp);

    /* -------------------------------------------------------------------- */
    /*      Reopen and copy missing information into a PAM file.            */
    /* -------------------------------------------------------------------- */
    GDALOpenInfo oOpenInfo(pszFilename, GA_ReadOnly);
    auto poDS = OpenPAM(&oOpenInfo);

    if (poDS)
        poDS->CloneInfo(poSrcDS, GCIF_PAM_DEFAULT);

    return poDS;
}

/************************************************************************/
/*                         GDALRegister_SRTMHGT()                       */
/************************************************************************/
void GDALRegister_SRTMHGT()
{
    if (GDALGetDriverByName("SRTMHGT") != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription("SRTMHGT");
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "SRTMHGT File Format");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "hgt");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC,
                              "drivers/raster/srtmhgt.html");
    poDriver->SetMetadataItem(GDAL_DMD_CREATIONDATATYPES, "Byte Int16 UInt16");
    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");

    poDriver->pfnIdentify = SRTMHGTDataset::Identify;
    poDriver->pfnOpen = SRTMHGTDataset::Open;
    poDriver->pfnCreateCopy = SRTMHGTDataset::CreateCopy;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
