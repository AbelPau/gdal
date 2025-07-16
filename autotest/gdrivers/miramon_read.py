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


import struct

import pytest

from osgeo import gdal

gdal_to_struct = {
    gdal.GDT_Byte: ("B", 1),
    gdal.GDT_UInt16: ("H", 2),
    gdal.GDT_Int16: ("h", 2),
    gdal.GDT_UInt32: ("I", 4),
    gdal.GDT_Int32: ("i", 4),
    gdal.GDT_Float32: ("f", 4),
    gdal.GDT_Float64: ("d", 8),
}

###### Testing IMG/REL normal files
def check_raster(ds, band_idx, expected, checksum):
    band = ds.GetRasterBand(band_idx)
    assert band is not None, f"Error opening band {band_idx}"
    rchecksum = band.Checksum()

    assert rchecksum == checksum, f"Unexpected checksum: {rchecksum}"

    xsize = band.XSize
    ysize = band.YSize
    dtype = band.DataType
    assert dtype in gdal_to_struct, f"Unsupported GDAL data type: {dtype}"

    fmt, size = gdal_to_struct[dtype]
    buf = band.ReadRaster(0, 0, xsize, ysize, buf_type=dtype)
    assert buf is not None, "Could not read raster data"

    # unpack and assert values
    count = xsize * ysize
    values = struct.unpack(f"{count}{fmt}", buf)

    for i, exp in enumerate(expected):
        assert (
            values[i] == exp
        ), f"Unexpected pixel value at index {i}: got {values[i]}, expected {exp}"


init_list = [
    ("data/miramon/normal/byte_2x3_6_categs.img", 1, [0, 1, 2, 3, 4, 5], 15),
    ("data/miramon/normal/byte_2x3_6_categsI.rel", 1, [0, 1, 2, 3, 4, 5], 15),
    ("data/miramon/normal/integer_2x3_6_categs.img", 1, [0, 1, 2, 3, 4, 5], 15),
    ("data/miramon/normal/integer_2x3_6_categsI.rel", 1, [0, 1, 2, 3, 4, 5], 15),
    ("data/miramon/normal/uinteger_2x3_6_categs.img", 1, [0, 1, 2, 3, 4, 5], 15),
    ("data/miramon/normal/uinteger_2x3_6_categsI.rel", 1, [0, 1, 2, 3, 4, 5], 15),
    ("data/miramon/normal/long_2x3_6_categs.img", 1, [0, 1, 2, 3, 4, 5], 15),
    ("data/miramon/normal/long_2x3_6_categsI.rel", 1, [0, 1, 2, 3, 4, 5], 15),
    ("data/miramon/normal/real_2x3_6_categs.img", 1, [0, 1, 2, 3, 4, 5], 15),
    ("data/miramon/normal/real_2x3_6_categsI.rel", 1, [0, 1, 2, 3, 4, 5], 15),
    ("data/miramon/normal/double_2x3_6_categs.img", 1, [0, 1, 2, 3, 4, 5], 15),
    ("data/miramon/normal/double_2x3_6_categsI.rel", 1, [0, 1, 2, 3, 4, 5], 15),
    ("data/miramon/normal/byte_2x3_6_categs_RLE.img", 1, [0, 1, 2, 3, 4, 5], 15),
    ("data/miramon/normal/byte_2x3_6_categs_RLEI.rel", 1, [0, 1, 2, 3, 4, 5], 15),
    ("data/miramon/normal/byte_2x3_6_categs_RLE_no_ind.img", 1, [0, 1, 2, 3, 4, 5], 15),
    (
        "data/miramon/normal/byte_2x3_6_categs_RLE_no_indI.rel",
        1,
        [0, 1, 2, 3, 4, 5],
        15,
    ),
    ("data/miramon/normal/integer_2x3_6_categs_RLE.img", 1, [0, 1, 2, 3, 4, 5], 15),
    ("data/miramon/normal/integer_2x3_6_categs_RLEI.rel", 1, [0, 1, 2, 3, 4, 5], 15),
    ("data/miramon/normal/uinteger_2x3_6_categs_RLE.img", 1, [0, 1, 2, 3, 4, 5], 15),
    ("data/miramon/normal/uinteger_2x3_6_categs_RLEI.rel", 1, [0, 1, 2, 3, 4, 5], 15),
    ("data/miramon/normal/long_2x3_6_categs_RLE.img", 1, [0, 1, 2, 3, 4, 5], 15),
    ("data/miramon/normal/long_2x3_6_categs_RLEI.rel", 1, [0, 1, 2, 3, 4, 5], 15),
    ("data/miramon/normal/real_2x3_6_categs_RLE.img", 1, [0, 1, 2, 3, 4, 5], 15),
    ("data/miramon/normal/real_2x3_6_categs_RLEI.rel", 1, [0, 1, 2, 3, 4, 5], 15),
    ("data/miramon/normal/double_2x3_6_categs_RLE.img", 1, [0, 1, 2, 3, 4, 5], 15),
    ("data/miramon/normal/double_2x3_6_categs_RLEI.rel", 1, [0, 1, 2, 3, 4, 5], 15),
    (
        "data/miramon/normal/chess_bit.img",
        1,
        [0, 1, 0, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1, 0],
        32,
    ),
    (
        "data/miramon/normal/chess_bitI.rel",
        1,
        [0, 1, 0, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1, 0],
        32,
    ),
]


@pytest.mark.parametrize(
    "filename,band_idx,expected,checksum",
    init_list,
    ids=[tup[0].split(".")[0] for tup in init_list],
)
@pytest.mark.require_driver("MiraMonRaster")
def test_miramon_test_012345_raster(filename, band_idx, expected, checksum):
    # ds = gdal.Open(filename)
    ds = gdal.OpenEx(filename, allowed_drivers=["MiraMonRaster"])
    assert ds is not None, "Could not open the file"
    check_raster(ds, band_idx, expected, checksum)


###### Testing IMG/REL files with errors
@pytest.mark.parametrize(
    "name,message_substring",
    [
        (
            "data/miramon/several_errors/alone_rel.rel",
            "not recognized as being in a supported file format",
        ),
        (
            "data/miramon/several_errors/alone_IrelI.rel",
            "must have VersMetaDades>=4",
        ),
        (
            "data/miramon/several_errors/empy_img.img",
            "not recognized as being in a supported file format",
        ),
        (
            "data/miramon/several_errors/empy_relI.rel",
            "must be REL4",
        ),
        (
            "data/miramon/several_errors/no_assoc_img.rel",
            "not recognized as being in a supported file format",
        ),
        (
            "data/miramon/several_errors/no_assoc_rel.img",
            "not recognized as being in a supported file format",
        ),
        (
            "data/miramon/several_errors/no_colI.rel",
            "No number of columns documented",
        ),
        (
            "data/miramon/several_errors/no_rowI.rel",
            "No number of rows documented",
        ),
        (
            "data/miramon/several_errors/no_zero_col_rowI.rel",
            "(nWidth <= 0 || nHeight <= 0)",
        ),
        (
            "data/miramon/several_errors/no_bandsI.rel",
            "ATTRIBUTE_DATA-IndexsNomsCamps section-key should exist",
        ),
        (
            "data/miramon/several_errors/no_bands2I.rel",
            "it has zero usable bands",
        ),
        (
            "data/miramon/several_errors/no_bands3I.rel",
            "ATTRIBUTE_DATA-IndexsNomsCamps section-key should exist",
        ),
        (
            "data/miramon/several_errors/no_typeI.rel",
            "MiraMonRaster: no nDataType documented",
        ),
        (
            "data/miramon/several_errors/wrong_typeI.rel",
            "MiraMonRaster: data type unhandled",
        ),
        (
            "data/miramon/several_errors/wrong_band_nameI.rel",
            "Failed to open MiraMon band file",
        ),
    ],
)
@pytest.mark.require_driver("MiraMonRaster")
def test_miramon_test_fails(name, message_substring):
    with pytest.raises(Exception) as excinfo:
        gdal.OpenEx(
            name,
            gdal.OF_RASTER,
        )
    assert message_substring in str(excinfo.value)


###### Testing subdatasets
init_list_subdatasets = [
    (
        "data/miramon/multiband/byte_2x3_6_multibandI.rel",
        3,
        0,
        1,
        [0, 1, 2, 3, 4, 5],
        15,
        None,
    ),
    (
        "data/miramon/multiband/byte_2x3_6_multibandI.rel",
        3,
        1,
        1,
        [0, 1, 2, 3, 4, 255],
        10,
        255,
    ),
    (
        "data/miramon/multiband/byte_2x3_6_multibandI.rel",
        3,
        2,
        1,
        [0, 1, 2, 3, 4, 5],
        15,
        0,
    ),
]


@pytest.mark.parametrize(
    "filename,n_exp_sds,idx_sds,idx_bnd,expected,checksum,expected_nodata",
    init_list_subdatasets,
    ids=[tup[0].split("/")[-1].split(".")[0] for tup in init_list_subdatasets],
)
@pytest.mark.require_driver("MiraMonRaster")
def test_miramon_subdatasets_detection(
    filename, n_exp_sds, idx_sds, idx_bnd, expected, checksum, expected_nodata
):
    ds = gdal.OpenEx(filename, allowed_drivers=["MiraMonRaster"])
    assert ds is not None, f"Could not open file: {filename}"

    subdatasets = ds.GetSubDatasets()
    assert subdatasets is not None, "GetSubDatasets() returned None"
    assert (
        len(subdatasets) == n_exp_sds
    ), f"Expected {n_exp_sds} subdatasets, got {len(subdatasets)}"

    # Let's open every one of them
    subdataset_name, desc = subdatasets[idx_sds]
    subds = gdal.OpenEx(subdataset_name, allowed_drivers=["MiraMonRaster"])
    assert subds is not None, f"Could not open subdataset: {subdataset_name}"
    band = subds.GetRasterBand(idx_bnd)
    assert band is not None, "Could not get band from subdataset"
    checksum = band.Checksum()
    assert checksum >= 0, "Invalid checksum from subdataset"
    nodata = band.GetNoDataValue()
    if nodata is not None:
        assert (
            nodata == expected_nodata
        ), f"Unexpected nodata value : got {nodata}, expected {expected_nodata}"

    check_raster(subds, idx_bnd, expected, checksum)


###### Testing color table
init_list_color_tables = [
    (
        "data/miramon/palettes/Constant/byte_2x3_6_categsI.rel",
        1,  # band index
        {
            0: (255, 0, 255, 255),
            1: (255, 0, 255, 255),
            2: (255, 0, 255, 255),
            3: (255, 0, 255, 255),
            4: (255, 0, 255, 255),
            5: (0, 0, 0, 0),
        },
    ),
    (
        "data/miramon/palettes/Categorical/Authomatic/byte_2x3_6_categsI.rel",
        1,  # band index
        None,
    ),
    (
        "data/miramon/palettes/Categorical/Assigned/byte_2x3_6_categsI.rel",
        1,  # band index
        {
            0: (0, 0, 125, 255),
            1: (0, 134, 255, 255),
            2: (0, 255, 0, 255),
            3: (255, 255, 78, 255),
            4: (255, 0, 0, 255),
            5: (255, 0, 133, 255),
        },
    ),
    (
        "data/miramon/palettes/Categorical/ThematicNoDataBeg/MUCSC_2002_30_m_v_6_retI.rel",
        1,  # band index
        {
            0: (0, 0, 0, 0),
            1: (212, 247, 255, 255),
            2: (153, 247, 245, 255),
            8: (255, 255, 201, 255),
            9: (184, 201, 189, 255),
            14: (145, 108, 0, 255),
            15: (83, 166, 0, 255),
            16: (149, 206, 0, 255),
            20: (65, 206, 0, 255),
            21: (128, 0, 128, 255),
            24: (201, 232, 163, 255),
        },
    ),
    (
        "data/miramon/palettes/Categorical/ThematicNoDataEnd/MUCSC_2002_30_m_v_6_retI.rel",
        1,  # band index
        {
            0: (0, 0, 0, 0),
            1: (212, 247, 255, 255),
            2: (153, 247, 245, 255),
            8: (255, 255, 201, 255),
            9: (184, 201, 189, 255),
            14: (145, 108, 0, 255),
            15: (83, 166, 0, 255),
            16: (149, 206, 0, 255),
            20: (65, 206, 0, 255),
            21: (128, 0, 128, 255),
            24: (201, 232, 163, 255),
        },
    ),
]


@pytest.mark.parametrize(
    "filename,idx_bnd,expected_ct",
    init_list_color_tables,
    ids=[tup[0].split("/")[-1].split(".")[0] for tup in init_list_color_tables],
)
@pytest.mark.require_driver("MiraMonRaster")
def test_miramon_color_table(filename, idx_bnd, expected_ct):
    ds = gdal.OpenEx(filename, allowed_drivers=["MiraMonRaster"])
    assert ds is not None, f"Could not open file: {filename}"

    band = ds.GetRasterBand(idx_bnd)
    assert band is not None, f"Could not get band {idx_bnd} from file"

    ct = band.GetColorTable()

    if expected_ct == None:
        assert ct is None
    else:
        assert ct is not None, "No color table found on band"
        for index, expected_color in expected_ct.items():
            entry = ct.GetColorEntry(index)
            assert (
                entry is not None
            ), f"Color entry for index {index} is missing in color table"
            assert (
                tuple(entry) == expected_color
            ), f"Color entry for index {index} does not match: got {entry}, expected {expected_color}"
