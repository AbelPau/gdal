#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test basic read support for all datatypes from a MiraMon file.
# Author:   Abel Pau <a.pau@creaf.cat>
#
###############################################################################
# Copyright (c) 2025, Abel Pau <a.pau@creaf.cat>
#
# SPDX-License-Identifier: MIT
###############################################################################


# import gdaltest
import pytest

from osgeo import gdal

init_list = [
    # ("data/miramon/byte_2x3_6_categs_RLE.img", 1, 15),
    ("data/miramon/byte_2x3_6_categs.img", 1, 15),
]


@pytest.mark.parametrize(
    "filename,band_idx,checksum",
    init_list,
    ids=[tup[0].split(".")[0] for tup in init_list],
)
@pytest.mark.require_driver("MiraMonRaster")
def test_miramon_open(filename, band_idx, checksum):
    ds = gdal.Open(filename)
    assert ds is not None, "No s'ha pogut obrir el fitxer"
    band = ds.GetRasterBand(band_idx)
    assert band is not None, f"Error opening band {band_idx}"
    rchecksum = band.Checksum()
    assert rchecksum == checksum, f"Unexpected checksum: {checksum}"
