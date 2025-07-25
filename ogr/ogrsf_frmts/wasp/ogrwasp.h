/******************************************************************************
 *
 * Project:  WAsP Translator
 * Purpose:  Definition of classes for OGR .map driver.
 * Author:   Vincent Mora, vincent dot mora at oslandia dot com
 *
 ******************************************************************************
 * Copyright (c) 2014, Oslandia <info at oslandia dot com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OGR_WASP_H_INCLUDED
#define OGR_WASP_H_INCLUDED

#include "ogrsf_frmts.h"

#include <memory>
#include <fstream>
#include <vector>

/************************************************************************/
/*                             OGRWAsPLayer                             */
/************************************************************************/

class OGRWAsPLayer final : public OGRLayer,
                           public OGRGetNextFeatureThroughRaw<OGRWAsPLayer>
{
    /* stuff for polygon processing */

    /* note if shared ptr are available, replace the ptr in the two structs */
    /* and remove deletion of array elements in ~OGRWAsPLayer() */
    struct Zone
    {
        OGREnvelope oEnvelope;
        OGRPolygon *poPolygon;
        double dfZ;
    };

    struct Boundary
    {
        OGRLineString *poLine;
        double dfLeft;
        double dfRight;
    };

    GDALDataset *m_poDS = nullptr;
    const bool bMerge;
    std::vector<Zone> oZones{};
    std::vector<Boundary> oBoundaries{};

    static bool isEqual(const double &dfRouhness1, const double &dfRouhness2)
    {
        return fabs(dfRouhness1 - dfRouhness2) < 1e-3;
    }

    /* end of stuff for polygon processing */

    int iFeatureCount{};

    const CPLString sName;
    VSILFILE *hFile{};

    /* for roughness zone, two fields for linestrings (left/right), one for
     * polygons */
    /* for elevation, one field (height) */
    const CPLString sFirstField;
    const CPLString sSecondField;
    const CPLString sGeomField;
    int iFirstFieldIdx{};
    int iSecondFieldIdx{};
    int iGeomFieldIdx{};

    OGRFeatureDefn *poLayerDefn{};
    OGRSpatialReference *poSpatialReference{};

    vsi_l_offset iOffsetFeatureBegin{};

    enum OpenMode
    {
        READ_ONLY,
        WRITE_ONLY
    };

    OpenMode eMode = READ_ONLY;

    std::unique_ptr<double> pdfTolerance{};
    std::unique_ptr<double> pdfAdjacentPointTolerance{};
    std::unique_ptr<double> pdfPointToCircleRadius{};

    OGRErr WriteRoughness(OGRLineString *, const double &dfZleft,
                          const double &dfZright);
    OGRErr WriteRoughness(OGRPolygon *, const double &dfZ);
    OGRErr WriteRoughness(OGRGeometry *, const double &dfZleft,
                          const double &dfZright);

    OGRErr WriteElevation(OGRLineString *, const double &dfZ);
    OGRErr WriteElevation(OGRGeometry *, const double &dfZ);

    static double AvgZ(OGRLineString *poGeom);
    static double AvgZ(OGRPolygon *poGeom);
    static double AvgZ(OGRGeometryCollection *poGeom);
    static double AvgZ(OGRGeometry *poGeom);

    /* return a simplified line (caller is responsible for resource)
     *
     * if pdfTolerance is not NULL,
     *     calls GEOS simplify
     *
     * if pdfAdjacentPointTolerance is not NULL,
     *     remove consecutive points that are less than tolerance apart
     *     in x and y
     *
     * if pdfPointToCircleRadius is not NULL,
     *     lines that have been simplified to a point are converted to a 8 pt
     * circle
     * */
    OGRLineString *Simplify(const OGRLineString &line) const;

    OGRFeature *GetNextRawFeature();

    CPL_DISALLOW_COPY_ASSIGN(OGRWAsPLayer)

  public:
    /* For writing */
    /* Takes ownership of poTolerance */
    OGRWAsPLayer(GDALDataset *poDS, const char *pszName, VSILFILE *hFile,
                 OGRSpatialReference *poSpatialRef,
                 const CPLString &sFirstField, const CPLString &sSecondField,
                 const CPLString &sGeomField, bool bMerge, double *pdfTolerance,
                 double *pdfAdjacentPointTolerance,
                 double *pdfPointToCircleRadius);

    /* For reading */
    OGRWAsPLayer(GDALDataset *poDS, const char *pszName, VSILFILE *hFile,
                 OGRSpatialReference *poSpatialRef);

    virtual ~OGRWAsPLayer();

    virtual OGRFeatureDefn *GetLayerDefn() override
    {
        return poLayerDefn;
    }

    virtual void ResetReading() override;
    virtual int TestCapability(const char *) override;

    virtual OGRErr CreateField(const OGRFieldDefn *poField,
                               int bApproxOK = TRUE) override;
    virtual OGRErr CreateGeomField(const OGRGeomFieldDefn *poGeomField,
                                   int bApproxOK = TRUE) override;

    virtual OGRErr ICreateFeature(OGRFeature *poFeature) override;

    DEFINE_GET_NEXT_FEATURE_THROUGH_RAW(OGRWAsPLayer)

    virtual const char *GetName() override
    {
        return sName.c_str();
    }

    GDALDataset *GetDataset() override
    {
        return m_poDS;
    }
};

/************************************************************************/
/*                           OGRWAsPDataSource                          */
/************************************************************************/

class OGRWAsPDataSource final : public GDALDataset
{
    CPLString sFilename{};
    VSILFILE *hFile{};
    std::unique_ptr<OGRWAsPLayer> oLayer{};

    void GetOptions(CPLString &sFirstField, CPLString &sSecondField,
                    CPLString &sGeomField, bool &bMerge) const;

    CPL_DISALLOW_COPY_ASSIGN(OGRWAsPDataSource)

  public:
    /** @note takes ownership of hFile (i.e. responsibility for closing) */
    OGRWAsPDataSource(const char *pszName, VSILFILE *hFile);
    virtual ~OGRWAsPDataSource();

    virtual int GetLayerCount() override
    {
        return oLayer.get() ? 1 : 0;
    }

    virtual OGRLayer *GetLayer(int) override;
    virtual OGRLayer *GetLayerByName(const char *) override;

    OGRLayer *ICreateLayer(const char *pszName,
                           const OGRGeomFieldDefn *poGeomFieldDefn,
                           CSLConstList papszOptions) override;

    virtual int TestCapability(const char *) override;
    OGRErr Load(bool bSilent = false);
};

#endif /* ndef OGR_WASP_H_INCLUDED */
