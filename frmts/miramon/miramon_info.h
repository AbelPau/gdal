/******************************************************************************
 *
 * Project:  MiraMonRaster driver
 * Purpose:  Implements MMRInfo class 
 * Author:   Abel Pau
 *
 ******************************************************************************
 * Copyright (c) 2025, Xavier Pons
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef MMR_INFO_H_INCLUDED
#define MMR_INFO_H_INCLUDED

#include "cpl_string.h"
#include "gdal_priv.h"
#include "miramon_rel.h"
#include "miramon_band.h"

class MMRRel;
class MMRBand;

/************************************************************************/
/*                         class MMRInfo                                */
/*                                                                      */
/*      A class that holds all information of a subdataset              */
/*      dataset within miramonopen.cpp                                  */
/************************************************************************/
class MMRInfo
{
  public:
    explicit MMRInfo(char *pszFilename);

    MMRInfo(const MMRInfo &) =
        delete;  // I don't want to construct a MMRInfo from another MMRInfo (effc++)
    MMRInfo &operator=(const MMRInfo &) =
        delete;  // I don't want to assing a MMRInfo to another MMRInfo (effc++)

    ~MMRInfo();
    /*
        When it is known that the file is a REL file (a
        format not used by any other driver), or if it is
        an IMG file accompanied by a sibling I.rel file that
        references this IMG file, special care must be taken.
        Since the Identify method returns UNKNOWN for IMG files,
        it is necessary to be cautious before assuming that any
        IMG file belongs to this driver.
    */
    bool bIsAMiraMonFile = false;

    CPLString osRELFileName = "";
    MMRRel *fRel = nullptr;  // Access stuff to REL file

    int nXSize = 0;
    int nYSize = 0;

    // List of rawBandNames in a subdataset
    std::vector<CPLString> papoSDSBands{};

    int nBands = 0;
    MMRBand **papoBand = nullptr;
};

#endif /* ndef MMR_INFO_H_INCLUDED */
