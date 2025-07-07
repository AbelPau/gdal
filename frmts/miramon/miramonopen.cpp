/******************************************************************************
 *
 * Project:  MiraMonRaster driver
 * Purpose:  Implements some band functions.
 * Author:   Abel Pau
 * 
 ******************************************************************************
 * Copyright (c) 2025, Xavier Pons
 *
 * SPDX-License-Identifier: MIT
 *****************************************************************************/

#include "miramon_p.h"

/************************************************************************/
/*                            MMRStandard()                             */
/*                                                                      */
/*      Swap byte order on MSB systems.                                 */
/************************************************************************/

#ifdef CPL_MSB
void MMRStandard(int nBytes, void *pData)

{
    GByte *pabyData = static_cast<GByte *>(pData);

    for (int i = nBytes / 2 - 1; i >= 0; i--)
    {
        GByte byTemp = pabyData[i];
        pabyData[i] = pabyData[nBytes - i - 1];
        pabyData[nBytes - i - 1] = byTemp;
    }
}
#endif
