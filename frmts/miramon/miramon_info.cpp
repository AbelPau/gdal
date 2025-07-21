/******************************************************************************
 *
 * Project:  MiraMonRaster driver
 * Purpose:  Implements MMRInfo class.
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

/************************************************************************/
/* ==================================================================== */
/*                            MMRInfo                                   */
/* ==================================================================== */
/************************************************************************/

MMRInfo::MMRInfo(char *pszFilename)
{
    // Creates the object that allows inspect metadata (REL file)
    fRel = new MMRRel(pszFilename);

    // Sets the info from that REL
    if (CE_None != fRel->UpdateInfoFromREL(pszFilename, *this))
        return;
}

/************************************************************************/
/*                              ~MMRInfo()                              */
/************************************************************************/
MMRInfo::~MMRInfo()
{
    for (int i = 0; i < nBands; i++)
        delete papoBand[i];

    delete[] papoBand;

    delete fRel;
}
