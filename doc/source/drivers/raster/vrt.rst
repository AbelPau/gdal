.. _raster.vrt:

================================================================================
VRT -- GDAL Virtual Format
================================================================================

.. shortname:: VRT

.. built_in_by_default::

Introduction
------------

The VRT driver is a format driver for GDAL that allows a virtual GDAL dataset
to be composed from other GDAL datasets with repositioning, and algorithms
potentially applied as well as various kinds of metadata altered or added.
VRT descriptions of datasets can be saved in an XML format normally given the
extension .vrt.

Note .vrt files starting with

- <OGRVRTDataSource> open with :ref:`ogrinfo`, etc.
- <VRTDataset> open with :ref:`gdalinfo`, etc.

The VRT format can also describe :ref:`gdal_vrttut_warped`
and :ref:`gdal_vrttut_pansharpen`

For mosaic with a very large number of tiles (tens of thousands or mores),
the :ref:`GTI <raster.gti>` driver may be used starting with GDAL 3.9.

An example of a simple .vrt file referring to a 512x512 dataset with one band
loaded from :file:`utm.tif` might look like this:

.. code-block:: xml

    <VRTDataset rasterXSize="512" rasterYSize="512">
        <GeoTransform>440720.0, 60.0, 0.0, 3751320.0, 0.0, -60.0</GeoTransform>
        <VRTRasterBand dataType="Byte" band="1">
            <ColorInterp>Gray</ColorInterp>
            <SimpleSource>
            <SourceFilename relativeToVRT="1">utm.tif</SourceFilename>
            <SourceBand>1</SourceBand>
            <SrcRect xOff="0" yOff="0" xSize="512" ySize="512"/>
            <DstRect xOff="0" yOff="0" xSize="512" ySize="512"/>
            </SimpleSource>
        </VRTRasterBand>
    </VRTDataset>

Many aspects of the VRT file are a direct XML encoding of the
:ref:`raster_data_model` which should be reviewed
for understanding of the semantics of various elements.

VRT files can be produced by translating to VRT format.  The resulting file can
then be edited to modify mappings, add metadata or other purposes.  VRT files
can also be produced programmatically by various means.

This tutorial will cover the .vrt file format (suitable for users editing
.vrt files), and how .vrt files may be created and manipulated programmatically
for developers.

.. _raster_vrt_creation_options:

Creation options
----------------

|about-creation-options|
The following creation options are supported:

-  .. oo:: BLOCKXSIZE
      :since: 3.7

      Sets block width.

-  .. oo:: BLOCKYSIZE
      :since: 3.7

      Sets block height.

      Setting explicitly the block size is an advanced setting that should only be
      used when the user has determined that it is needed. By default the block size
      is set to:

      - 128x128 for a source-based VRT raster band. Unless the VRT is made of a single
        source and this single source is not subsetted, in which case the block size of
        the unique source will be set as the VRT raster band block size)

      - 512x128 for a warped VRT.

.vrt Format
-----------

A `XML schema of the GDAL VRT format <https://raw.githubusercontent.com/OSGeo/gdal/master/frmts/vrt/data/gdalvrt.xsd>`_
is available.

Note, .vrt files starting with
- <OGRVRTDataSource> open with ogrinfo, etc.
- <VRTDataset> open with gdalinfo, etc.

Virtual files stored on disk are kept in an XML format with the following
elements.

**VRTDataset**: This is the root element for the whole GDAL dataset. It must have the attributes rasterXSize and rasterYSize describing the width and height of the dataset in pixels. It may have a subClass attributes with values VRTWarpedDataset (:ref:`gdal_vrttut_warped`) or VRTPansharpenedDataset (:ref:`gdal_vrttut_pansharpen`). It may have SRS, GeoTransform, GCPList, Metadata, MaskBand and VRTRasterBand subelements.

.. code-block:: xml

    <VRTDataset rasterXSize="512" rasterYSize="512">

VRTDataset
++++++++++

The allowed subelements for VRTDataset are :

- **SRS**: This element contains the spatial reference system (coordinate system) in OGC WKT format.  Note that this must be appropriately escaped for XML, so items like quotes will have the ampersand escape sequences substituted. As well as WKT, valid input to the :cpp:func:`OGRSpatialReference::SetFromUserInput` method (such as well known GEOGCS names, and PROJ.4 format) is also allowed in the SRS element.

.. code-block:: xml

  <SRS dataAxisToSRSAxisMapping="1,2">PROJCS[&quot;NAD27 / UTM zone 11N&quot;,GEOGCS[&quot;NAD27&quot;,DATUM[&quot;North_American_Datum_1927&quot;,SPHEROID[&quot;Clarke 1866&quot;,6378206.4,294.9786982139006,AUTHORITY[&quot;EPSG&quot;,&quot;7008&quot;]],AUTHORITY[&quot;EPSG&quot;,&quot;6267&quot;]],PRIMEM[&quot;Greenwich&quot;,0],UNIT[&quot;degree&quot;,0.0174532925199433],AUTHORITY[&quot;EPSG&quot;,&quot;4267&quot;]],PROJECTION[&quot;Transverse_Mercator&quot;],PARAMETER[&quot;latitude_of_origin&quot;,0],PARAMETER[&quot;central_meridian&quot;,-117],PARAMETER[&quot;scale_factor&quot;,0.9996],PARAMETER[&quot;false_easting&quot;,500000],PARAMETER[&quot;false_northing&quot;,0],UNIT[&quot;metre&quot;,1,AUTHORITY[&quot;EPSG&quot;,&quot;9001&quot;]],AUTHORITY[&quot;EPSG&quot;,&quot;26711&quot;]]</SRS>

The **dataAxisToSRSAxisMapping** attribute is allowed since GDAL 3.0 to describe the relationship between the axis indicated in the CRS definition and the axis of the GeoTransform or GCP metadata. The value of the attribute is a comma separated list of integers. The number of elements of this list must be the number of axis of the CRS. Values start at 1. If m denotes the array values of this attribute, then m[0] is the data axis number for the first axis of the CRS. If the attribute is missing, then the OAMS_TRADITIONAL_GIS_ORDER data axis to CRS axis mapping strategy is implied.

- **GeoTransform**: This element contains a six value affine geotransformation for the dataset, mapping between pixel/line coordinates and georeferenced coordinates. Typically (geotransform[0], geotransform[3]) will be the (easting, northing) of the upper-left corner of the raster, geotransform[1] the horizontal resolution in geospatial coordinates/pixel, and geotransform[5] the vertical resolution in geospatial coordinates/pixel, as a negative value if the image is north-up oriented. See :ref:`raster_data_model_geotransform` for more details about that mapping.

.. code-block:: xml

  <GeoTransform>440720.0,  60,  0.0,  3751320.0,  0.0, -60.0</GeoTransform>

- **GCPList**: This element contains a list of Ground Control Points for the dataset, mapping between pixel/line coordinates and georeferenced coordinates. The Projection attribute should contain the SRS of the georeferenced coordinates in the same format as the SRS element. The dataAxisToSRSAxisMapping attribute is the same as in the SRS element.

.. code-block:: xml

    <GCPList Projection="EPSG:4326">
        <GCP Id="1" Info="a" Pixel="0.5" Line="0.5" X="0.0" Y="0.0" Z="0.0" />
        <GCP Id="2" Info="b" Pixel="13.5" Line="23.5" X="1.0" Y="2.0" Z="0.0" />
    </GCPList>

- **Metadata**: This element contains a list of metadata name/value pairs associated with the VRTDataset as a whole, or a VRTRasterBand. It has <MDI> (metadata item) subelements which have a "key" attribute and the value as the data of the element. The Metadata element can be repeated multiple times, in which case it must be accompanied with a "domain" attribute to indicate the name of the metadata domain.

.. code-block:: xml

  <Metadata>
    <MDI key="md_key">Metadata value</MDI>
  </Metadata>

- **MaskBand**: This element represents a mask band that is shared between all bands on the dataset (see GMF_PER_DATASET in RFC 15). It must contain a single VRTRasterBand child element, that is the description of the mask band itself.

.. code-block:: xml

  <MaskBand>
    <VRTRasterBand dataType="Byte">
      <SimpleSource>
        <SourceFilename relativeToVRT="1">utm.tif</SourceFilename>
        <SourceBand>mask,1</SourceBand>
        <SrcRect xOff="0" yOff="0" xSize="512" ySize="512"/>
        <DstRect xOff="0" yOff="0" xSize="512" ySize="512"/>
      </SimpleSource>
    </VRTRasterBand>
  </MaskBand>

- **OverviewList**: (GDAL >= 3.2.0, not valid for VRTPansharpenedDataset)
  This elements contains a list of overview factors, separated by space, to
  create "virtual overviews". For example ``2 4``. It can be used so that bands
  of the VRT datasets declare overviews. This only makes sense to use if the
  sources added in those bands have themselves overviews compatible with the
  declared factor. It is generally not necessary to use this mechanism, since
  downsampling pixel requests on a VRT dataset/band are able to use overviews of the
  sources, even when the VRT bands do not declare them. One situation where
  explicit overviews are needed at the VRT level is the warping of a VRT
  to a lower resolution.
  This element can also be used with an existing VRT dataset by running
  :cpp:func:`GDALDataset::BuildOverviews` or :program:`gdaladdo` with the
  :config:`VRT_VIRTUAL_OVERVIEWS` configuration option set to ``YES``:

  .. config:: VRT_VIRTUAL_OVERVIEWS
     :choices: YES, NO
     :default: NO

  Virtual overviews have the least priority compared to the **Overview** element
  at the **VRTRasterBand** level, or to materialized .vrt.ovr files.


- **VRTRasterBand**: This represents one band of a dataset.

VRTRasterBand
+++++++++++++

The attributes for VRTRasterBand are:

- **dataType** (optional): type of the pixel data associated with this band (use
  names Byte, Int8, UInt16, Int16, UInt32, Int32, UInt64, Int64, Float32, Float64, CInt16, CInt32, CFloat32 or CFloat64).
  If not specified, defaults to 1

- **band** (optional): band number this element represents (1 based).

- **blockXSize** (optional, GDAL >= 3.3): block width.
  If not specified, defaults to the minimum of the raster width and 128.

- **blockYSize** (optional, GDAL >= 3.3): block height.
  If not specified, defaults to the minimum of the raster height and 128.

This element may have Metadata, ColorInterp, NoDataValue, HideNoDataValue, ColorTable, GDALRasterAttributeTable, Description and MaskBand subelements as well as the various kinds of source elements such as SimpleSource, ComplexSource, AveragedSource, NoDataFromMaskSource, KernelFilteredSource and ArraySource.  A raster band may have many "sources" indicating where the actual raster data should be fetched from, and how it should be mapped into the raster bands pixel space.

The allowed subelements for VRTRasterBand are :

- **ColorInterp**: The data of this element should be the name of a color interpretation type.  One of Gray, Palette, Red, Green, Blue, Alpha, Hue, Saturation, Lightness, Cyan, Magenta, Yellow, Black, or Unknown.

.. code-block:: xml

  <ColorInterp>Gray</ColorInterp>:

- **NoDataValue**: Specifies the NoData value that the raster band will report.
  If not specified, NoData pixels from the band's sources (as well as pixels
  not covered by any source) will be set to zero. If the source rasters have
  their own NoData values that differ from the one specified in
  ``<NoDataValue>``, a ``ComplexSource`` with a ``<NODATA>`` element can be
  used to convert source NoData pixels to the value specified here.

.. code-block:: xml

  <NoDataValue>-100.0</NoDataValue>

- **HideNoDataValue**: If this value is ``YES``/``TRUE``/``1``, the NoData
  value specified by ``<NoDataValue>`` will not be reported.  Essentially, the
  reader will not be aware of a NoData pixel when it reads one.  Any datasets
  copied/translated from this will not have a NoData value.  This is useful
  when you want to specify a fixed background value for the dataset. (The
  background will be the value specified by the NoDataValue element.) Default
  value is ``NO`` when this element is absent.

.. code-block:: xml

  <HideNoDataValue>1</HideNoDataValue>

- **ColorTable**: This element is parent to a set of Entry elements defining the entries in a color table.  Currently only RGBA color tables are supported with c1 being red, c2 being green, c3 being blue and c4 being alpha.  The entries are ordered and will be assumed to start from color table entry 0.

.. code-block:: xml

    <ColorTable>
      <Entry c1="0" c2="0" c3="0" c4="255"/>
      <Entry c1="145" c2="78" c3="224" c4="255"/>
    </ColorTable>

- **GDALRasterAttributeTable**: (GDAL >=2.3) This element is parent to a set of FieldDefn elements defining the columns of a raster attribute table, followed by a set of Row elements defining the values of the columns of each row.

.. code-block:: xml

    <GDALRasterAttributeTable>
      <FieldDefn index="0">
        <Name>Value</Name>
        <Type>0</Type>
        <Usage>0</Usage>
      </FieldDefn>
      <FieldDefn index="1">
        <Name>Red</Name>
        <Type>0</Type>
        <Usage>6</Usage>
      </FieldDefn>
      <FieldDefn index="2">
        <Name>Green</Name>
        <Type>0</Type>
        <Usage>7</Usage>
      </FieldDefn>
      <FieldDefn index="3">
        <Name>Blue</Name>
        <Type>0</Type>
        <Usage>8</Usage>
      </FieldDefn>
      <Row index="0">
        <F>-500</F>
        <F>127</F>
        <F>40</F>
        <F>65</F>
      </Row>
      <Row index="1">
        <F>-400</F>
        <F>154</F>
        <F>168</F>
        <F>118</F>
      </Row>
    </GDALRasterAttributeTable>

- **Description**: This element contains the optional description of a raster band as its text value.

.. code-block:: xml

  <Description>Crop Classification Layer</Description>

- **UnitType**: This optional element contains the vertical units for elevation band data.  One of "m" for meters or "ft" for feet. Default assumption is meters.

.. code-block:: xml

  <UnitType>ft</UnitType>

- **Offset**: This optional element contains the offset that should be applied when computing "real" pixel values from scaled pixel values on a raster band.   The default is 0.0.

.. code-block:: xml

  <Offset>0.0</Offset>

- **Scale**: This optional element contains the scale that should be applied when computing "real" pixel values from scaled pixel values on a raster band.   The default is 1.0.

.. code-block:: xml

  <Scale>1.0</Scale>

- **Overview**: This optional element describes one overview level for the band.  It should have a child SourceFilename and SourceBand element.  The SourceFilename may have a relativeToVRT boolean attribute.  Multiple elements may be used to describe multiple overviews.

.. code-block:: xml

    <Overview>
      <SourceFilename relativeToVRT="1">yellowstone_2.1.ntf.r2</SourceFilename>
      <SourceBand>1</SourceBand>
    </Overview>

- **CategoryNames**: This optional element contains a list of Category subelements with the names of the categories for classified raster band.

.. code-block:: xml

  <CategoryNames>
    <Category>Missing</Category>
    <Category>Non-Crop</Category>
    <Category>Wheat</Category>
    <Category>Corn</Category>
    <Category>Soybeans</Category>
  </CategoryNames>

- **SimpleSource**: The SimpleSource_ indicates that raster data should be read from a separate dataset, indicating the dataset, and band to be read from, and how the data should map into this band's raster space.

- **AveragedSource**: The AveragedSource is derived from the SimpleSource and shares the same properties except that it uses an averaging resampling instead of a nearest neighbour algorithm as in SimpleSource, when the size of the destination rectangle is not the same as the size of the source rectangle. Note: a more general mechanism to specify resampling algorithms can be used. See above paragraph about the 'resampling' attribute.

- **NoDataFromMaskSource**: (GDAL >= 3.9) The NoDataFromMaskSource is derived from the SimpleSource and shares the same properties except that it replaces the value of the source with the value of the NODATA child element when the value of the mask band of the source is less or equal to the MaskValueThreshold child element. This is typically used to transform a R,G,B,A image into a R,G,B one with a NoData value.

- **ComplexSource**: The ComplexSource_ is derived from the SimpleSource (so it shares the SourceFilename, SourceBand, SrcRect and DstRect elements), but it provides support to rescale and offset the range of the source values. Certain regions of the source can be masked by specifying the NODATA value, or starting with GDAL 3.3, with the <UseMaskBand>true</UseMaskBand> element.

- **KernelFilteredSource**: The KernelFilteredSource_ is a pixel source derived from the Simple Source (so it shares the SourceFilename, SourceBand, SrcRect and DstRect elements, but it also passes the data through a simple filtering kernel specified with the Kernel element.

- **ArraySource**: The ArraySource_ indicates that raster data should be read from a 2D array using the multidimensional API.

- **MaskBand**: This element represents a mask band that is specific to the VRTRasterBand it contains. It must contain a single VRTRasterBand child element, that is the description of the mask band itself.

Sources
*******

SimpleSource
~~~~~~~~~~~~

The SimpleSource may have the SourceFilename, SourceBand, SrcRect, and DstRect
subelements.  The SrcRect element will indicate what rectangle on the indicated
source file should be read, and the DstRect element indicates how that
rectangle of source data should be mapped into the VRTRasterBands space.

SrcRect and DstRect are expressed in pixel/line coordinate space. Their
relationship with the geospatial coordinate space is given by the geotransform
matrix of the source for SrcRect, and of the VRT itself (**GeoTransform**
element) for DstRect.
See :ref:`raster_data_model_geotransform` for more details about that mapping.

The relativeToVRT attribute on the SourceFilename indicates whether the
filename should be interpreted as relative to the .vrt file (value is 1)
or not relative to the .vrt file (value is 0).  The default is 0.

Some characteristics of the source band can be specified in the optional
``SourceProperties`` element to enable the VRT driver to defer the opening of the source
dataset until it really needs to read data from it. This is particularly useful
when building VRTs with a big number of source datasets. The needed parameters are the
raster dimensions, the size of the blocks and the data type. If the SourceProperties
tag is not present, the source dataset will be opened at the same time as the VRT itself.

.. note::

    Starting with GDAL 3.4, the ``SourceProperties`` element is no longer necessary
    for deferred opening of the source datasets.

The content of the SourceBand subelement can refer to
a mask band. For example mask,1 means the mask band of the first band of the source.

.. code-block:: xml

    <SimpleSource>
      <SourceFilename relativeToVRT="1">utm.tif</SourceFilename>
      <SourceBand>1</SourceBand>
      <SourceProperties RasterXSize="512" RasterYSize="512" DataType="Byte" BlockXSize="128" BlockYSize="128"/>
      <SrcRect xOff="0" yOff="0" xSize="512" ySize="512"/>
      <DstRect xOff="0" yOff="0" xSize="512" ySize="512"/>
    </SimpleSource>

A OpenOptions subelement can be added to specify
the open options to apply when opening the source dataset. It has <OOI> (open option item)
subelements which have a "key" attribute and the value as the data of the element.

.. code-block:: xml

    <SimpleSource>
      <SourceFilename relativeToVRT="1">utm.tif</SourceFilename>
      <OpenOptions>
          <OOI key="OVERVIEW_LEVEL">0</OOI>
      </OpenOptions>
      <SourceBand>1</SourceBand>
      <SourceProperties RasterXSize="256" RasterYSize="256" DataType="Byte" BlockXSize="128" BlockYSize="128"/>
      <SrcRect xOff="0" yOff="0" xSize="256" ySize="256"/>
      <DstRect xOff="0" yOff="0" xSize="256" ySize="256"/>
    </SimpleSource>

A resampling attribute can be specified on a SimpleSource
or ComplexSource element to specified the resampling algorithm used when the
size of the destination rectangle is not the same as the size of the source
rectangle. The values allowed for that attribute are : nearest,bilinear,cubic,
cubicspline,lanczos,average,mode.

.. code-block:: xml

    <SimpleSource resampling="cubic">
      <SourceFilename relativeToVRT="1">utm.tif</SourceFilename>
      <SourceBand>1</SourceBand>
      <SourceProperties RasterXSize="256" RasterYSize="256" DataType="Byte" BlockXSize="128" BlockYSize="128"/>
      <SrcRect xOff="0" yOff="0" xSize="256" ySize="256"/>
      <DstRect xOff="0" yOff="0" xSize="128" ySize="128"/>
    </SimpleSource>


Starting with GDAL 3.11, it is also possible to use a in-line VRTDataset as
the source by using the VRTDataset element instead of SourceFilename.

.. code-block:: xml

    <SimpleSource>
      <VRTDataset rasterXSize="20" rasterYSize="20">
        <VRTRasterBand dataType="Byte" band="1">
          <SimpleSource>
            <SourceFilename relativeToVRT="1">../byte.tif</SourceFilename>
            <SourceBand>1</SourceBand>
            <SourceProperties RasterXSize="20" RasterYSize="20" DataType="Byte" BlockXSize="20" BlockYSize="20" />
            <SrcRect xOff="0" yOff="0" xSize="20" ySize="20" />
            <DstRect xOff="0" yOff="0" xSize="20" ySize="20" />
          </SimpleSource>
        </VRTRasterBand>
      </VRTDataset>
      <SourceBand>1</SourceBand>
      <SourceProperties RasterXSize="20" RasterYSize="20" DataType="Byte" BlockXSize="20" BlockYSize="20" />
      <SrcRect xOff="0" yOff="0" xSize="20" ySize="20" />
      <DstRect xOff="0" yOff="0" xSize="20" ySize="20" />
    </SimpleSource>


ComplexSource
~~~~~~~~~~~~~

Alternatively to linear scaling, non-linear
scaling using a power function can be used by specifying the Exponent,
SrcMin, SrcMax, DstMin and DstMax elements. If SrcMin and SrcMax are
not specified, they are computed from the source minimum and maximum
value (which might require analyzing the whole source dataset). Exponent
must be positive. (Those 5 values can be set with the -exponent and -scale
options of gdal_translate.)

The ComplexSource supports adding a custom lookup table to transform
the source values to the destination. The LUT can be specified using
the following form:

.. code-block:: xml

    <LUT>[src value 1]:[dest value 1],[src value 2]:[dest value 2],...</LUT>

The intermediary values are calculated using a linear interpolation
between the bounding destination values of the corresponding range.
Source values should be listed in a monotonically non-decreasing order.
If there is a Not-A-Number (NaN) source value, it should be the first one.
Clamping is performed for input pixel values outside of the range specified by the LUT. That is, if an
input pixel value is lower than the minimum source value, then the destination
value corresponding to that minimum source value is used as the output pixel value.
And similarly for an input pixel value that is greater than the maximum source value.
To remap values without interpolation, the "reclassify" pixel function can be used. See :ref:`vrt_derived_bands`.

The ComplexSource supports fetching a color component from a source raster
band that has a color table. The ColorTableComponent value is the index of the
color component to extract : 1 for the red band, 2 for the green band, 3 for
the blue band or 4 for the alpha band.

When transforming the source values the operations are executed
in the following order:

- Masking, if the NODATA element is set or, starting with GDAL 3.3,
  if the UseMaskBand is set to true and the source band has a mask band.
  Note that this is binary masking only, so no alpha blending is done if the
  mask band is actually an alpha band with non-0 or non-255 values.
- Color table expansion
- For linear scaling, applying the scale ratio, then scale offset
- For non-linear scaling, apply (DstMax-DstMin) * pow( (SrcValue-SrcMin) / (SrcMax-SrcMin), Exponent) + DstMin
- Table lookup

.. code-block:: xml

    <ComplexSource>
      <SourceFilename relativeToVRT="1">utm.tif</SourceFilename>
      <SourceBand>1</SourceBand>
      <ScaleOffset>0</ScaleOffset>
      <ScaleRatio>1</ScaleRatio>
      <ColorTableComponent>1</ColorTableComponent>
      <LUT>0:0,2345.12:64,56789.5:128,2364753.02:255</LUT>
      <NODATA>0</NODATA>  <!-- if the mask is a mask or alpha band, use <UseMaskBand>true</UseMaskBand> -->
      <SrcRect xOff="0" yOff="0" xSize="512" ySize="512"/>
      <DstRect xOff="0" yOff="0" xSize="512" ySize="512"/>
    </ComplexSource>

Non-linear scaling:

.. code-block:: xml

    <ComplexSource>
      <SourceFilename relativeToVRT="1">16bit.tif</SourceFilename>
      <SourceBand>1</SourceBand>
      <Exponent>0.75</Exponent>
      <SrcMin>0</SrcMin>
      <SrcMax>65535</SrcMax>
      <DstMin>0</DstMin>
      <DstMax>255</DstMax>
      <SrcRect xOff="0" yOff="0" xSize="512" ySize="512"/>
      <DstRect xOff="0" yOff="0" xSize="512" ySize="512"/>
    </ComplexSource>


KernelFilteredSource
~~~~~~~~~~~~~~~~~~~~

The Kernel element should have
two child elements, Size and Coefs and optionally the boolean attribute
normalized (defaults to false=0).  The size must always be an odd number,
and the Coefs must have Size * Size entries separated by spaces.  For now
kernel is not applied to sub-sampled or over-sampled data.

.. code-block:: xml

    <KernelFilteredSource>
      <SourceFilename>/debian/home/warmerda/openev/utm.tif</SourceFilename>
      <SourceBand>1</SourceBand>
      <Kernel normalized="1">
        <Size>3</Size>
        <Coefs>0.11111111 0.11111111 0.11111111 0.11111111 0.11111111 0.11111111 0.11111111 0.11111111 0.11111111</Coefs>
      </Kernel>
    </KernelFilteredSource>

Starting with GDAL 2.3, a separable kernel may also be used.  In this case the
number of Coefs entries should correspond to the Size.  The Coefs specify a
one-dimensional kernel which is applied along each axis in succession, resulting
in far quicker execution. Many common image-processing filters are separable.
For example, a Gaussian blur:

.. code-block:: xml

    <KernelFilteredSource>
      <SourceFilename>/debian/home/warmerda/openev/utm.tif</SourceFilename>
      <SourceBand>1</SourceBand>
      <Kernel normalized="1">
        <Size>13</Size>
        <Coefs>0.01111 0.04394 0.13534 0.32465 0.60653 0.8825 1.0 0.8825 0.60653 0.32465 0.13534 0.04394 0.01111</Coefs>
      </Kernel>
    </KernelFilteredSource>

NoDataFromMaskSource
~~~~~~~~~~~~~~~~~~~~

.. versionadded:: 3.9

The NoDataFromMaskSource is derived from the SimpleSource and shares the same properties except that it replaces the value of the source with the value of the NODATA child element when the value of the mask band of the source is less or equal to the MaskValueThreshold child element.
This is typically used to transform a R,G,B,A image into a R,G,B one with a NoData value.
An optional RemappedValue element can be set to specify the value onto which valid pixels whose value is the one of NODATA should be remapped to. When RemappedValue is not explicitly specified, for Byte bands, if NODATA=255, it is implicitly set to 254, otherwise it is set to NODATA+1.

.. code-block:: xml

    <NoDataFromMaskSource>
      <SourceFilename relativeToVRT="1">in.tif</SourceFilename>
      <SourceBand>1</SourceBand>
      <MaskValueThreshold>128</MaskValueThreshold> <!-- if the mask value is &lt;= 128, pixels are set to NODATA=0 -->
      <NODATA>0</NODATA>
      <RemappedValue>1</RemappedValue> <!-- valid/unmasked pixels at NODATA=0 are remapped to 1 -->
    </NoDataFromMaskSource>


ArraySource
~~~~~~~~~~~

.. versionadded:: 3.8

The ArraySource_ indicates that raster data should be read from a 2D array using
the multidimensional API. If the original array is not a 3D array, a DerivedArray
with a View step must be typically used to create a 2D slice.

ArraySource can have the following child elements:

* ``SingleSourceArray``, ``DerivedArray`` or ``Array``: required as first child element
* ``SrcRect`` (same syntax and semantics as in SimpleSource): optional
* ``DstRect`` (same syntax and semantics as in SimpleSource): optional

SingleSourceArray
^^^^^^^^^^^^^^^^^

SingleSourceArray references a multidimensional raster through a SourceFilename
element and an array through its full path within it with SourceArray. When
SingleSourceArray is used as a direct child of ArraySource, it must be a 2-dimensional
array.

Example:

.. code-block:: xml

    <ArraySource>
      <SingleSourceArray>
        <SourceFilename relativeToVRT="1">byte.nc</SourceFilename>
        <SourceArray>/my_array</SourceArray>
      </SingleSourceArray>
    </ArraySource>

Be careful that quite often arrays in the multidimensional API use a "bottom-up"
approach, and it might thus be needed to reverse the Y axis, with a View expression
with a DerivedArray (cf below example)

DerivedArray
^^^^^^^^^^^^

A DerivedArray starts with an array coming from a SingleSourceArray
(but it could also be a DerivedArray or a Array), which must be the first
child element, and applies different processing steps wrapped in zero or more
Step elements.

* View step: applies the :cpp:func:`GDALMDArray::GetView` method

The ``View`` element must have an ``expr`` attribute, which is the argument of
:cpp:func:`GDALMDArray::GetView` method

The following example slices the 3D dimension at index 0 and flips the vertical
axis (assuming 1st dimension is a non-spatial one, 2nd dimension is the Y
dimension and 3rd dimension is the X dimension)

.. code-block:: xml

    <ArraySource>
      <DerivedArray>
          <SingleSourceArray>
            <SourceFilename relativeToVRT="1">3d_array.nc</SourceFilename>
            <SourceArray>/my_array</SourceArray>
          </SingleSourceArray>
          <Step>
              <View expr="[0,::-1,...]"/>
          </Step>
       </DerivedArray>
    </ArraySource>

* ``Transpose`` step: applies the :cpp:func:`GDALMDArray::Transpose` method

The ``Transpose`` element must have a ``newOrder`` attribute, which is the argument of
:cpp:func:`GDALMDArray::Transpose` method

The following example transposes the 2 axis of a 2D array

.. code-block:: xml

    <ArraySource>
      <DerivedArray>
          <SingleSourceArray>
            <SourceFilename relativeToVRT="1">2d_array.nc</SourceFilename>
            <SourceArray>/my_array</SourceArray>
          </SingleSourceArray>
          <Step>
              <Transpose newOrder="[1,0]"/>
          </Step>
       </DerivedArray>
    </ArraySource>

* Resample step: applies the :cpp:func:`GDALMDArray::GetResampled` method

The ``Resample`` element may have the following child elements, which correspond
to the arguments of the :cpp:func:`GDALMDArray::GetResampled` method:

- ``Dimension``: with a ``name`` and ``size`` attribute. This element may be repeated.
- ``ResampleAlg``: whose value is ``NearestNeighbour`` (default), ``Cubic``, ``CubicSpline``, ``Bilinear``, ``Lanczos``, ``Average``, ``RMS`` or ``Mode``
- ``SRS``: target SRS (string recognized by :cpp:func:`OGRSpatialReference::SetFromUserInput`)
- ``Option`` with a ``name`` attribute and the value as the content of the element. This element may be repeated.

.. code-block:: xml

    <ArraySource>
      <DerivedArray>
          <SingleSourceArray>
            <SourceFilename relativeToVRT="1">test.nc</SourceFilename>
            <SourceArray>/Band1</SourceArray>
          </SingleSourceArray>
          <Step>
              <Resample>
                <Dimension name="Y" size="40"/>
                <Dimension name="X" size="40"/>
                <ResampleAlg>Bilinear</ResampleAlg>
                <SRS>EPSG:4267</SRS>
                <Option name="IGNORED">YES</Option>
              </Resample>
          </Step>
       </DerivedArray>
    </ArraySource>

* Grid step: applies the :cpp:func:`GDALMDArray::GetGridded` method

The ``Grid`` element may have the following child elements, which correspond
to the arguments of the :cpp:func:`GDALMDArray::GetGridded` method:

- ``GridOptions`` (required): name and options of the gridding algorithm
- ``XArray`` (optional): its child value should be a SingleSourceArray, DerivedArray or Array pointing to the array to use for the X dimension. This is needed if the source array does not have a ``coordinates`` attribute.
- ``YArray`` (optional): its child value should be a SingleSourceArray, DerivedArray or Array pointing to the array to use for the Y dimension. This is needed if the source array does not have a ``coordinates`` attribute.
- ``Option`` with a ``name`` attribute and the value as the content of the element. This element may be repeated.

.. code-block:: xml

    <ArraySource>
      <DerivedArray>
          <SingleSourceArray>
            <SourceFilename relativeToVRT="1">test.nc</SourceFilename>
            <SourceArray>/Band1</SourceArray>
          </SingleSourceArray>
          <Step>
              <Grid>
                  <GridOptions>invdist</GridOptions>
                  <XArray>
                      <SingleSourceArray>
                        <SourceFilename relativeToVRT="1">test.nc</SourceFilename>
                        <SourceArray>/varX</SourceArray>
                      </SingleSourceArray>
                  </XArray>
                  <YArray>
                      <SingleSourceArray>
                        <SourceFilename relativeToVRT="1">test.nc</SourceFilename>
                        <SourceArray>/varY</SourceArray>
                      </SingleSourceArray>
                  </YArray>
                  <Option name="IGNORED">YES</Option>
              </Grid>
          </Step>
       </DerivedArray>
    </ArraySource>

* ``GetUnscaled`` step: applies the :cpp:func:`GDALMDArray::GetUnscaled` method

It does not have any child elements.

.. code-block:: xml

    <ArraySource>
      <DerivedArray>
          <SingleSourceArray>
            <SourceFilename relativeToVRT="1">2d_array.nc</SourceFilename>
            <SourceArray>/my_array</SourceArray>
          </SingleSourceArray>
          <Step>
              <GetUnscaled/>
          </Step>
       </DerivedArray>
    </ArraySource>

* ``GetMask`` step: applies the :cpp:func:`GDALMDArray::GetMask` method

The ``GetMask`` element may have the following child elements, which correspond
to the arguments of the :cpp:func:`GDALMDArray::GetMask` method:

- ``Option`` with a ``name`` attribute and the value as the content of the element. This element may be repeated.

.. code-block:: xml

    <ArraySource>
      <DerivedArray>
          <SingleSourceArray>
            <SourceFilename relativeToVRT="1">2d_array.nc</SourceFilename>
            <SourceArray>/my_array</SourceArray>
          </SingleSourceArray>
          <Step>
              <GetMask>
                  <Option name="UNMASK_FLAGS">microwave,land</Option>
              </GetMask>
          </Step>
       </DerivedArray>
    </ArraySource>

Array
^^^^^

:ref:`Array <vrt_multidimensional>` can be used to mosaic several multidimensional arrays.


Overviews
---------

GDAL can make efficient use of overviews available in the sources that compose
the bands when dealing with RasterIO() requests that involve downsampling.
But in the general case, the VRT bands themselves will not expose overviews.

Except if (from top priority to lesser priority) :

- The **Overview** element is present in the VRTRasterBand element. See above.
- or external .vrt.ovr overviews are built
- (starting with GDAL 3.2) explicit virtual overviews, if a **OverviewList** element
  is declared in the VRTDataset element (see above).
  Those virtual overviews will be hidden by external .vrt.ovr overviews that might be built later.
- (starting with GDAL 2.1) implicit virtual overviews, if the VRTRasterBand are made of
  a single SimpleSource or ComplexSource that has overviews.
  Those virtual overviews will be hidden by external .vrt.ovr overviews that might be built later.

.. _vrtrawrasterband:

.vrt Descriptions for Raw Files
-------------------------------

.. warning:: Consult the :ref:`vrtrawrasterband_restricted_access` below
             section for potential security issues related to that functionality
             and how to restrict it.

So far we have described how to derive new virtual datasets from existing
files supported by GDAL.  However, it is also common to need to utilize
raw binary raster files for which the regular layout of the data is known
but for which no format specific driver exists.  This can be accomplished
by writing a .vrt file describing the raw file.

For example, the following .vrt describes a raw raster file containing
floating point complex pixels in a file called *l2p3hhsso.img*.  The
image data starts from the first byte (ImageOffset=0).  The byte offset
between pixels is 8 (PixelOffset=8), the size of a CFloat32.  The byte offset
from the start of one line to the start of the next is 9376 bytes
(LineOffset=9376) which is the width (1172) times the size of a pixel (8).

.. code-block:: xml

    <VRTDataset rasterXSize="1172" rasterYSize="1864">
        <VRTRasterBand dataType="CFloat32" band="1" subClass="VRTRawRasterBand">
            <SourceFilename relativetoVRT="1">l2p3hhsso.img</SourceFilename>
            <ImageOffset>0</ImageOffset>
            <PixelOffset>8</PixelOffset>
            <LineOffset>9376</LineOffset>
            <ByteOrder>MSB</ByteOrder>
        </VRTRasterBand>
    </VRTDataset>

Some things to note are that the VRTRasterBand has a subClass specifier
of "VRTRawRasterBand".  Also, the VRTRawRasterBand contains a number of
previously unseen elements but no "source" information.  VRTRawRasterBands
may never have sources (i.e. SimpleSource), but should contain the following
elements in addition to all the normal "metadata" elements previously
described which are still supported.

- **SourceFilename**: The name of the raw file containing the data for this band.  The relativeToVRT attribute can be used to indicate if the SourceFilename is relative to the .vrt file (1) or not (0).

- **ImageOffset**: The offset in bytes to the beginning of the first pixel of data of this image band.   Defaults to zero.

- **PixelOffset**: The offset in bytes from the beginning of one pixel and the next on the same line.  In packed single band data this will be the size of the **dataType** in bytes.

- **LineOffset**: The offset in bytes from the beginning of one scanline of data and the next scanline of data.  In packed single band data this will be PixelOffset * rasterXSize.

- **ByteOrder**: Defines the byte order of the data on disk. Either LSB (Least Significant Byte first) such as the natural byte order on Intel x86 systems or MSB (Most Significant Byte first) such as the natural byte order on Motorola or Sparc systems.  Defaults to being the local machine order.

A few other notes:

- The image data on disk is assumed to be of the same data type as the band **dataType** of the VRTRawRasterBand.

- All the non-source attributes of the VRTRasterBand are supported, including color tables, metadata, nodata values, and color interpretation.

- The VRTRawRasterBand supports in place update of the raster, whereas the source based VRTRasterBand is always read-only.

- Multiple bands in the one .vrt file can come from the same raw file. Just ensure that the ImageOffset, PixelOffset, and LineOffset definition for each band is appropriate for the pixels of that particular band.

Another example, in this case a 400x300 RGB pixel interleaved image.

.. code-block:: xml

    <VRTDataset rasterXSize="400" rasterYSize="300">
    <VRTRasterBand dataType="Byte" band="1" subClass="VRTRawRasterBand">
        <ColorInterp>Red</ColorInterp>
        <SourceFilename relativetoVRT="1">rgb.raw</SourceFilename>
        <ImageOffset>0</ImageOffset>
        <PixelOffset>3</PixelOffset>
        <LineOffset>1200</LineOffset>
    </VRTRasterBand>
    <VRTRasterBand dataType="Byte" band="2" subClass="VRTRawRasterBand">
        <ColorInterp>Green</ColorInterp>
        <SourceFilename relativetoVRT="1">rgb.raw</SourceFilename>
        <ImageOffset>1</ImageOffset>
        <PixelOffset>3</PixelOffset>
        <LineOffset>1200</LineOffset>
    </VRTRasterBand>
    <VRTRasterBand dataType="Byte" band="3" subClass="VRTRawRasterBand">
        <ColorInterp>Blue</ColorInterp>
        <SourceFilename relativetoVRT="1">rgb.raw</SourceFilename>
        <ImageOffset>2</ImageOffset>
        <PixelOffset>3</PixelOffset>
        <LineOffset>1200</LineOffset>
    </VRTRasterBand>
    </VRTDataset>

.. _vrtrawrasterband_restricted_access:

Restricting access to Raw Files
-------------------------------

Some usages of GDAL, for example its use on a server that allows users to upload
a (VRT) file, convert it to another format, and get the result back,
could be abused to read the content of local files. Starting with GDAL 3.12, it
is possible to restrict the use of the VRTRawRasterBand capability in several
ways:

- at build time, the CMake ``GDAL_VRT_ENABLE_RAWRASTERBAND`` variable can be set
  to ``OFF``, to complete disable VRTRawRasterBand.

- at runtime, with the following configuration options:

  * .. config:: GDAL_VRT_ENABLE_RAWRASTERBAND
       :choices: YES, NO
       :default: YES
       :since: 3.12

       Whether the VRTRawRasterBand capability is allowed at runtime.

  * .. config:: GDAL_VRT_RAWRASTERBAND_ALLOWED_SOURCE
       :choices: SIBLING_OR_CHILD_OF_VRT_PATH, ONLY_REMOTE, ALL, <path>
       :default: SIBLING_OR_CHILD_OF_VRT_PATH
       :since: 3.12

       Restricts which SourceFilename values are allowed:

       - if set to ``SIBLING_OR_CHILD_OF_VRT_PATH`` (which is the default value of
         that configuration option starting with GDAL 3.12), the ``relativeToVRT``
         attribute of ``SourceFilename`` will need to set to ``1``, and the path
         expressed by ``SourceFilename`` must not contain any `../` or `..\\` substring.
         Note that GDAL does not try to detect if one of the files, in the file
         hierarchy below the directory of the VRT, is a symbolic link pointing to
         elsewhere in the file system.

       - if set to ``ONLY_REMOTE``, only ``SourceFilename`` pointing to one of the
         :ref:`VSI network based file systems <network_based_file_systems>` will be
         allowed. Be careful though that this could still be used to access files
         accessible on a local network, depending on the network configuration of the
         machine on which GDAL is run.

       - if set to ``ALL`` there is no restriction on the value of  ``SourceFilename``.

       - if set to one absolute path (or several ones, separated by the ``;`` (semi-colon) character
         on Windows, or ``:`` (colon) on other operating systems), only ``SourceFilename`` that
         start with those allowed absolute paths will be accepted.

In versions before GDAL 3.12, disabling entirely the VRT driver by setting the
:config:`GDAL_SKIP` configuration option to ``VRT`` may be a workaround.

Creation of VRT Datasets
------------------------

The VRT driver supports several methods of creating VRT datasets.
The :file:`vrtdataset.h` include file should be installed with the core
GDAL include files, allowing direct access to the VRT classes.  However,
even without that most capabilities remain available through standard GDAL
interfaces.

To create a VRT dataset that is a clone of an existing dataset use the
:cpp:func:`GDALDriver::CreateCopy` method.  For example to clone
:file:`utm.tif` into a :file:`wrk.vrt` file in C++ the following could be used:

.. code-block:: cpp

  GDALDriver *poDriver = (GDALDriver *) GDALGetDriverByName( "VRT" );
  GDALDataset *poSrcDS, *poVRTDS;

  poSrcDS = (GDALDataset *) GDALOpenShared( "utm.tif", GA_ReadOnly );

  poVRTDS = poDriver->CreateCopy( "wrk.vrt", poSrcDS, FALSE, NULL, NULL, NULL );

  GDALClose((GDALDatasetH) poVRTDS);
  GDALClose((GDALDatasetH) poSrcDS);

Note the use of :cpp:func:`GDALOpenShared` when opening the source dataset. It is advised
to use :cpp:func:`GDALOpenShared` in this situation so that you are able to release
the explicit reference to it before closing the VRT dataset itself. In other
words, in the previous example, you could also invert the 2 last lines, whereas
if you open the source dataset with :cpp:func:`GDALOpen`, you'd need to close the VRT dataset
before closing the source dataset.

To obtain the resulting VRT XML of :file:`wrk.vrt` without having to read the text from an actual file,
you can modify the above code to open the new dataset with an empty filename and use the "xml:VRT"
metadata domain.

.. code-block:: cpp

  // no filename
  poVRTDS = poDriver->CreateCopy( "", poSrcDS, FALSE, NULL, NULL, NULL );

  // obtain the actual XML text that a VRT file would contain
  const char *xmlvrt = poVRTDS->GetMetadata("xml:VRT")[0];

To create a virtual copy of a dataset with some attributes added or changed
such as metadata or coordinate system that are often hard to change on other
formats, you might do the following.  In this case, the virtual dataset is
created "in memory" only by virtual of creating it with an empty filename, and
then used as a modified source to pass to a :cpp:func:`GDALDriver::CreateCopy` written out in TIFF
format.

.. code-block:: cpp

  poVRTDS = poDriver->CreateCopy( "", poSrcDS, FALSE, NULL, NULL, NULL );

  poVRTDS->SetMetadataItem( "SourceAgency", "United States Geological Survey");
  poVRTDS->SetMetadataItem( "SourceDate", "July 21, 2003" );

  poVRTDS->GetRasterBand( 1 )->SetNoDataValue( -999.0 );

  GDALDriver *poTIFFDriver = (GDALDriver *) GDALGetDriverByName( "GTiff" );
  GDALDataset *poTiffDS;

  poTiffDS = poTIFFDriver->CreateCopy( "wrk.tif", poVRTDS, FALSE, NULL, NULL, NULL );

  GDALClose((GDALDatasetH) poTiffDS);

In the above example the nodata value is set as -999. You can set the
HideNoDataValue element in the VRT dataset's band using :cpp:func:`GDALRasterBand::SetMetadataItem` on
that band.

.. code-block:: cpp

  poVRTDS->GetRasterBand( 1 )->SetMetadataItem( "HideNoDataValue" , "1" );

In this example a virtual dataset is created with the :cpp:func:`GDALDriver::Create` method, and
adding bands and sources programmatically, but still via the "generic" API.
A special attribute of VRT datasets is that sources can be added to the VRTRasterBand
(but not to VRTRawRasterBand) by passing the XML describing the source into :cpp:func:`GDALRasterBand::SetMetadataItem` on the special
domain target "new_vrt_sources".  The domain target "vrt_sources" may also be
used, in which case any existing sources will be discarded before adding the
new ones.  In this example we construct a simple averaging filter source
instead of using the simple source.

.. code-block:: cpp

    // construct XML for simple 3x3 average filter kernel source.
    const char *pszFilterSourceXML  =
    "<KernelFilteredSource>"
    "  <SourceFilename>utm.tif</SourceFilename><SourceBand>1</SourceBand>"
    "  <Kernel>"
    "    <Size>3</Size>"
    "    <Coefs>0.111 0.111 0.111 0.111 0.111 0.111 0.111 0.111 0.111</Coefs>"
    "  </Kernel>"
    "</KernelFilteredSource>";

    // Create the virtual dataset.
    poVRTDS = poDriver->Create( "", 512, 512, 1, GDT_Byte, NULL );
    poVRTDS->GetRasterBand(1)->SetMetadataItem("source_0",pszFilterSourceXML,
                                                "new_vrt_sources");

A more general form of this that will produce a 3x3 average filtered clone
of any input datasource might look like the following.  In this case we
deliberately set the filtered datasource as in the "vrt_sources" domain
to override the SimpleSource created by the `cpp:func:`GDALDriver::CreateCopy` method.  The fact
that we used `cpp:func:GDALDriver::CreateCopy` ensures that all the other metadata, georeferencing
and so forth is preserved from the source dataset ... the only thing we are
changing is the data source for each band.

.. code-block:: cpp

    int   nBand;
    GDALDriver *poDriver = (GDALDriver *) GDALGetDriverByName( "VRT" );
    GDALDataset *poSrcDS, *poVRTDS;

    poSrcDS = (GDALDataset *) GDALOpenShared( pszSourceFilename, GA_ReadOnly );

    poVRTDS = poDriver->CreateCopy( "", poSrcDS, FALSE, NULL, NULL, NULL );

    for( nBand = 1; nBand <= poVRTDS->GetRasterCount(); nBand++ )
    {
        char szFilterSourceXML[10000];

        GDALRasterBand *poBand = poVRTDS->GetRasterBand( nBand );

        sprintf( szFilterSourceXML,
            "<KernelFilteredSource>"
            "  <SourceFilename>%s</SourceFilename><SourceBand>%d</SourceBand>"
            "  <Kernel>"
            "    <Size>3</Size>"
            "    <Coefs>0.111 0.111 0.111 0.111 0.111 0.111 0.111 0.111 0.111</Coefs>"
            "  </Kernel>"
            "</KernelFilteredSource>",
            pszSourceFilename, nBand );

        poBand->SetMetadataItem( "source_0", szFilterSourceXML, "vrt_sources" );
    }

The :cpp:class:`VRTDataset` class is one of the few dataset implementations that supports the :cpp:func:`GDALDataset::AddBand`
method. The options passed to the :cpp:func:`GDALDataset::AddBand` method can be used to control the type of the
band created (VRTRasterBand, VRTRawRasterBand, VRTDerivedRasterBand), and in the case of
the VRTRawRasterBand to set its various parameters. For standard VRTRasterBand, sources
should be specified with the above :cpp:func:`GDALRasterBand::SetMetadataItem` examples.

.. code-block:: cpp

  GDALDriver *poDriver = (GDALDriver *) GDALGetDriverByName( "VRT" );
  GDALDataset *poVRTDS;

  poVRTDS = poDriver->Create( "out.vrt", 512, 512, 0, GDT_Byte, NULL );
  char** papszOptions = NULL;
  papszOptions = CSLAddNameValue(papszOptions, "subclass", "VRTRawRasterBand"); // if not specified, default to VRTRasterBand
  papszOptions = CSLAddNameValue(papszOptions, "SourceFilename", "src.tif"); // mandatory
  papszOptions = CSLAddNameValue(papszOptions, "ImageOffset", "156"); // optional. default = 0
  papszOptions = CSLAddNameValue(papszOptions, "PixelOffset", "2"); // optional. default = size of band type
  papszOptions = CSLAddNameValue(papszOptions, "LineOffset", "1024"); // optional. default = size of band type * width
  papszOptions = CSLAddNameValue(papszOptions, "ByteOrder", "LSB"); // optional. default = machine order
  papszOptions = CSLAddNameValue(papszOptions, "relativeToVRT", "true"); // optional. default = false
  poVRTDS->AddBand(GDT_Byte, papszOptions);
  CSLDestroy(papszOptions);

  delete poVRTDS;

.. _vrt_derived_bands:

Derived Bands (pixel functions)
-------------------------------

A "derived" band is a specialized type of band that derives its pixel
values from its source bands using a "pixel function" at the time
that values are read from the raster.
Using derived bands you can create VRT datasets that manipulate bands on
the fly without having to create new band files on disk.
GDAL provides a number of
:ref:`built-in pixel functions <builtin_pixel_functions>`;
additional pixel functions can be defined in :ref:`C++ <cpp_pixel_functions>`
or :ref:`Python <python_pixel_functions>` and registered with GDAL using
a unique key.

A VRTRasterBand can be made a VRTDerivedRasterBand by setting attribute subClass="VRTDerivedRasterBand".

Some of the common subelements for VRTRasterBand (whose subClass="VRTDerivedRasterBand") are listed here. They can be used with built-in, C++, or Python pixel functions.

- **PixelFunctionType**: (required): A pixel function with this name must be defined.
- **SkipNonContributingSources**: (optional, added in GDAL 3.7, defaults to false) = true/false: Whether sources that do not intersect the VRTRasterBand RasterIO() requested region should be omitted. By default, data for all sources, including ones that do not intersect it, are passed to the pixel function. By setting this parameter to true, only sources that intersect the requested region will be passed.

.. example::
   :title: Calculating a derived band
   :id: vrt-derived-1

For example, you
might want to generate a band using four source bands from a nine-band input
dataset (x0, x3, x4, and x8) and some constant y:

.. code-block:: c

  band_value = sqrt((x3*x3+x4*x4)/(x0*x8)) + y;

This can be accomplished using the built-in "expression" pixel function.
The following VRT XML can be used:

.. code-block:: xml

    <VRTDataset rasterXSize="1000" rasterYSize="1000">
        <VRTRasterBand dataType="Float32" band="1" subClass="VRTDerivedRasterBand">
            <Description>Magnitude</Description>
            <PixelFunctionType>expression</PixelFunctionType>
            <PixelFunctionArguments expression="sqrt((x3*x3+x4*x4)/(x0*x8)) + 4" />
            <SimpleSource name="x0">
                <SourceFilename relativeToVRT="1">nine_band.dat</SourceFilename>
                <SourceBand>1</SourceBand>
            </SimpleSource>
            <SimpleSource name="x3">
                <SourceFilename relativeToVRT="1">nine_band.dat</SourceFilename>
                <SourceBand>4</SourceBand>
            </SimpleSource>
            <SimpleSource name="x4">
                <SourceFilename relativeToVRT="1">nine_band.dat</SourceFilename>
                <SourceBand>5</SourceBand>
            </SimpleSource>
            <SimpleSource name="x8">
                <SourceFilename relativeToVRT="1">nine_band.dat</SourceFilename>
                <SourceBand>9</SourceBand>
            </SimpleSource>
        </VRTRasterBand>
    </VRTDataset>

.. seealso::
   :ref:`VRT processed datasets <vrt_processed_dataset>`, which provide a means of calculating multiple bands at once.


.. _builtin_pixel_functions:

Built-in Pixel Functions
++++++++++++++++++++++++

GDAL provides a set of default pixel functions that can be used without writing new code:


.. list-table::
   :widths: 15 10 10 65
   :header-rows: 1

   * - PixelFunctionType
     - Number of input sources
     - PixelFunctionArguments
     - Description
   * - **argmax**
     - >= 1
     - ``propagateNoData`` (optional, default=false)
     - (GDAL >= 3.12) Index (1-based, contrary to ``numpy.argmax``) of band with the maximum value
   * - **argmin**
     - >= 1
     - ``propagateNoData`` (optional, default=false)
     - (GDAL >= 3.12) Index (1-based, contrary to ``numpy.argmin``) of band with the minimum value
   * - **cmul**
     - 2
     - -
     - Multiply the first band for the complex conjugate of the second
   * - **complex**
     - 2
     - -
     - Make a complex band merging two bands used as real and imag values
   * - **conj**
     - 1
     - -
     - Computes the complex conjugate of a single raster band

       (just a copy if the input is non-complex)
   * - **dB**
     - 1
     - ``fact`` (optional)
     - Perform conversion to dB of the abs of a single raster band (real or complex):

       ``20. * log10( abs( x ) )``.

       The optional ``fact`` parameter can be set to ``10``

       to get the alternative formula: ``10. * log10( abs( x ) )``
   * - **dB2amp**
     - 1
     - -
     - Perform scale conversion from logarithmic to linear (amplitude) (i.e. ``10 ^ ( x / 20 )`` )
       of a single raster band (real only).

       Deprecated in GDAL 3.5. Please use the ``exp`` pixel function with
       ``base = 10.`` and ``fact = 0.05`` i.e. ``1./20``
   * - **dB2pow**
     - 1
     - -
     - Perform scale conversion from logarithmic to linear (power) (i.e. ``10 ^ ( x / 10 )`` )
       of a single raster band (real only).

       Deprecated in GDAL 3.5. Please use the ``exp`` pixel function with
       ``base = 10.`` and ``fact = 0.1`` i.e. ``1./10``
   * - **diff**
     - 2
     - -
     - Computes the difference between 2 raster bands (``b1 - b2``).

       Starting with GDAL 3.12, if either ``b1`` or ``b2`` is equal to the

       derived band's NoData value (set with ``<NoDataValue>``), the result

       will be the NoData value.
   * - **div**
     - 2
     - -
     - Divide one raster band by another (``b1 / b2``).

       Starting with GDAL 3.12, if either ``b1`` or ``b2`` is equal to the

       derived band's NoData value (set with ``<NoDataValue>``), the result

       will be the NoData value.
   * - **exp**
     - 1
     - ``base`` (optional)

       ``fact`` (optional)
     - Computes the exponential of each element in the input band ``x`` (of real values): ``e ^ x``.

       The function also accepts two optional parameters: ``base`` and ``fact``

       that allow to compute the generalized formula: ``base ^ ( fact * x )``.

       Note: this function is the recommended one to perform conversion form

       logarithmic scale (dB): `` 10. ^ (x / 20.)``, in this case

       ``base = 10.`` and ``fact = 0.05`` i.e. ``1. / 20``

       Starting with GDAL 3.12, if ``x`` is equal to the derived band's NoData value

       (set with ``<NoDataValue>``), the result will be the NoData value.

   * - **expression**
     - 1
     - ``expression``

       ``dialect`` (optional)

       ``propagateNoData`` (GDAL >= 3.12, optional, default=false)

     - Evaluate a specified expression using `muparser <https://beltoforion.de/en/muparser/>`__ (default)
       or `ExprTk <https://www.partow.net/programming/exprtk/index.html>`__.

       The expression is specified using the "expression" argument.
       The dialect may be specified using the "dialect" argument.

       If the optional ``propagateNoData`` parameter is set to ``true``, then

       if a NoData pixel is found in one of the bands, if will be propagated to

       the output value. Otherwise, NoData pixels will be passed to the expression

       as-is. The expression can then use the ``NODATA`` variable (or ``isnodata``

       muparser function) to test for these pixels and handle them accordingly.

       Within the expression, band values can be accessed:

       - through the variables ``B1``, ``B2``, etc.

       - by giving a name to a source band (e.g., ``<SimpleSource name="NIR">``)

       - or through the ``BANDS`` vector.
         With ExprTk, ``BANDS`` is exposed as a standard (0-indexed) vector.

         With muparser, it is expanded into a list of all input bands.

       Starting with GDAL 3.12, the variables ``_CENTER_X_`` and ``_CENTER_Y_`` can be

       included in the expression to access cell center coordinates. The variable ``NODATA``

       can be included in the expression if the derived band has a NoData value.

       ExprTk and muparser support a number of built-in functions and control structures.

       Refer to the documentation of those libraries for details.
   * - **geometric_mean**
     - >= 1
     - ``propagateNoData`` (optional, default=false)
     - (GDAL >= 3.12) Geometric mean of input raster bands.

       If the optional ``propagateNoData`` parameter is set to ``true``, then

       if a NoData pixel is found in one of the bands, if will be propagated to

       the output value. Otherwise, NoData pixels will be ignored.
   * - **harmonic_mean**
     - >= 1
     - ``propagateNoData`` (optional, default=false)

       ``propagateZero`` (optional, default=false)
     - (GDAL >= 3.12) Harmonic mean of input raster bands.

       If the optional ``propagateNoData`` parameter is set to ``true``, then

       if a NoData pixel is found in one of the bands, if will be propagated to

       the output value. Otherwise, NoData pixels will be ignored.

       If zero values are encountered in one of the bands, the output will

       be set to NoData unless ``propagateZero`` is set to ``true``.

   * - **imag**
     - 1
     - -
     - Extract imaginary part from a single raster band (0 for non-complex)
   * - **intensity**
     - 1
     - -
     - Computes the intensity ``Re( x * conj(x) )`` of a single raster band (real or complex)
   * - **interpolate_exp**
     - >= 2
     - ``t0``, ``dt``, ``t``
     - Interpolate a value at time (or position) ``t`` given input sources

       beginning at position ``t0`` with spacing ``dt`` using exponential interpolation.

       Starting with GDAL 3.12, if either input source bounding ``t`` is equal to the NoData

       value of the derived band (set with ``<NoDataValue>``), the result will be the

       NoData value.
   * - **interpolate_linear**
     - >= 2
     - ``t0``, ``dt``, ``t``
     - Interpolate a value at time (or position) ``t`` given input sources

       beginning at ``t0`` with spacing ``dt`` using linear interpolation.

       Starting with GDAL 3.12, if either input source bounding ``t`` is equal to the NoData

       value of the derived band (set with ``<NoDataValue>``), the result will be the

       NoData value.
   * - **inv**
     - 1
     - ``k`` (optional)
     - Inverse (``1./x``). If the optional ``k`` parameter is set,

       then the result is multiplied by ``k`` (``k / x``).

       Starting with GDAL 3.12, if ``x`` is equal to the derived band's NoData value

       (set with ``<NoDataValue>``), the result will be the NoData value.
   * - **log10**
     - 1
     - -
     - Compute the logarithm (base 10) of the abs of a single raster band

       (real or complex): ``log10( abs( x ) )``

       Starting with GDAL 3.12, if ``x`` is equal to the derived band's NoData value

       (set with ``<NoDataValue>``), the result will be the NoData value.
   * - **max**
     - >= 1
     - ``propagateNoData`` (optional, default=false)

       ``k``: constant (optional)
     - (GDAL >= 3.8) Maximum of raster band(s) and an optional constant

       If the optional ``propagateNoData`` parameter is set to ``true``, then

       if a NoData pixel is found in one of the bands, if will be propagated to

       the output value. Otherwise, NoData pixels will be ignored.
   * - **mean**
     - >= 1
     - ``propagateNoData`` (optional, default=false)
     - (GDAL >= 3.12) Arithmetic mean of input raster bands.

       If the optional ``propagateNoData`` parameter is set to ``true``, then

       if a NoData pixel is found in one of the bands, if will be propagated to

       the output value. Otherwise, NoData pixels will be ignored.
   * - **median**
     - >= 1
     - ``propagateNoData`` (optional, default=false)
     - (GDAL >= 3.12) Median of input raster bands.

       If the optional ``propagateNoData`` parameter is set to ``true``, then

       if a NoData pixel is found in one of the bands, if will be propagated to

       the output value. Otherwise, NoData pixels will be ignored.
   * - **min**
     - >= 1
     - ``propagateNoData`` (optional, default=false)

       ``k``: constant (optional)
     - (GDAL >= 3.8) Minimum of raster band(s) and an optional constant

       If the optional ``propagateNoData`` parameter is set to ``true``, then

       if a NoData pixel is found in one of the bands, if will be propagated to

       the output value. Otherwise, NoData pixels will be ignored.
   * - **mod**
     - 1
     - -
     - Extract module from a single raster band (real or complex)
   * - **mode**
     - >= 1
     - ``propagateNoData`` (optional, default=false)
     - (GDAL >= 3.12) Mode (most common value) of input raster bands.

       If the optional ``propagateNoData`` parameter is set to ``true``, then

       if a NoData pixel is found in one of the bands, if will be propagated to

       the output value. Otherwise, NoData pixels will be ignored.
   * - **mul**
     - >= 1
     - ``k`` (optional)

       ``propagateNoData`` (optional, default=false)
     - Multiply 1 or more raster bands.

       If the optional ``k`` parameter is provided then the result is

       multiplied by the scalar ``k``.

       Starting with GDAL 3.12, if ``propagateNoData`` is true, any input pixel

       equal to the derived band's NoData value (set with ``<NoDataValue>``)

       will cause the result to be the NoData value. If ``propagateNoData`` is

       false, input NoData values will be ignored.
   * - **norm_diff**
     - 2
     - -
     - Computes the normalized difference between two raster bands: ``(b1 - b2)/(b1 + b2)``

       Starting with GDAL 3.12, if either ``b1`` or ``b2`` is equal to the

       derived band's NoData value (set with ``<NoDataValue>``), the result

       will be the NoData value.
   * - **phase**
     - 1
     - -
     - Extract phase from a single raster band [-PI,PI] (0 or PI for non-complex)
   * - **polar**
     - 2
     - ``amplitude_type`` (optional)
     - Make a complex band using input bands for amplitude and phase values ``b1 * exp( j * b2 )``.

       The optional (string) parameter ``amplitude_type`` can be:

       - ``AMPLITUDE`` (default),

       - ``INTENSITY`` or

       - ``dB``.

       Note: if ``amplitude_type`` is set to ``INTENSITY`` then negative values are clipped to zero.
   * - **pow**
     - 1
     - ``power``
     - Raise a single raster band to a constant power, specified with argument ``power`` (real only)

       Starting with GDAL 3.12, if the input is equal to the derived band's NoData value

       (set with ``<NoDataValue>``), the result will be the NoData value.
   * - **real**
     - 1
     - -
     - Extract real part from a single raster band (just a copy if the input is non-complex)
   * - **reclassify**
     - = 1
     - ``mapping``

       ``default``
     - Reclassify values according to a mapping provided by ``mapping``.

       The format of the mapping is ``SOURCE=DEST;SOURCE=DEST;...`` where each
       ``SOURCE`` element is:

       - either a single value

       - an interval (e.g., ``(-inf,30]``), ``NO_DATA``, or ``DEFAULT``.

       ``DEST`` may be:

       - any constant,

       - ``PASS_THROUGH`` (to skip reclassification for certain values),

       - or ``NO_DATA``.

       An error will be raised if a pixel value is not covered by any interval.

       A similar functionality, but with interpolation, is offered by the ``ComplexSource``.
   * - **replace_nodata**
     - = 1
     - ``to`` (optional)
     - Convert incoming ``NoData`` values to a new value, IEEE 754 `nan` by default
   * - **scale**
     - = 1
     - -
     - Perform scaling according to the ``offset`` and ``scale`` values of the raster band

       Starting with GDAL 3.12, if the input is equal to the derived band's NoData value

       (set with ``<NoDataValue>``), the result will be the NoData value.
   * - **sqrt**
     - 1
     - -
     - Perform the square root of a single raster band (real only).

       Starting with GDAL 3.12, if the input is equal to the derived band's NoData value

       (set with ``<NoDataValue>``), the result will be the NoData value.
   * - **sum**
     - >= 1
     - ``k`` (optional)

       ``propagateNoData`` (optional, default=``false``)
     - Sum 1 or more raster bands. If the optional ``k`` parameter is provided

       then it is added to each element of the result.

       Starting with GDAL 3.12, if ``propagateNoData`` is true, any input pixel

       equal to the derived band's NoData value (set with ``<NoDataValue>``)

       will cause the result to be the NoData value. If ``propagateNoData`` is

       false, input NoData values will be ignored.

.. example::
   :title: VRT expression with a simple condition

   This example uses a simple ``if`` block to calculate the derived band
   value depending on the value of an input band. Note the use of ``&gt;``
   to escape the ``>`` character in the XML attribute. Using the same
   VRT as :example:`vrt-derived-1`, the following expression can be
   used:

   .. code-block:: xml

      <PixelFunctionType>expression</PixelFunctionType>
      <PixelFunctionArguments dialect="muparser" expression="B1 ? 1.5*B3 : B1" />

   or

   .. code-block:: xml

      <PixelFunctionType>expression</PixelFunctionType>
      <PixelFunctionArguments dialect="exprtk" expression="if (B1 &gt; 1) 1.5*B3 ; else B1" />

.. example::
   :title: VRT expression using all bands

   The ``BANDS`` variable provides access to all of the input bands, either
   individually (``BANDS[0]`` for the first band) or collectively. Here,
   we concisely evaluate the value of a band relative to the sum of other
   bands. Using the same VRT as :example:`vrt-derived-1`:

   .. code-block:: xml

      <PixelFunctionType>expression</PixelFunctionType>
      <PixelFunctionArguments expression="B1 / sum(BANDS)" />


.. _cpp_pixel_functions:

Writing Pixel Functions in C/C++
++++++++++++++++++++++++++++++++

If the built-in pixel functions are not suitable, a custom function can be written in C or C++. In this case, the
You could write the pixel function to compute this value and then
register it with GDAL with a name such as "MyFirstFunction".
To register this function with GDAL (prior to accessing any VRT datasets
with derived bands that use this function), an application calls
:cpp:func:`GDALAddDerivedBandPixelFuncWithArgs` with a key and a :cpp:type:`GDALDerivedPixelFuncWithArgs`:

.. code-block:: cpp

    static const char pszMetadata[] =
    "<PixelFunctionArgumentsList>"
    "   <Argument name='y' description='y' type='double' mandatory='1' />"
    "   <Argument type='builtin' value='offset' />"
    "   <Argument type='builtin' value='scale' />"
    "   <Argument type='builtin' value='NoData' />"
    "   <Argument name='customConstant' type='constant' value='42'>"
    "</PixelFunctionArgumentsList>";
    GDALAddDerivedBandPixelFuncWithArgs("MyFirstFunction", TestFunction, pszMetadata);

A good time to do this is at the beginning of an application when the
GDAL drivers are registered. ``pszMetadata`` is optional and can be ``nullptr``.
It can be used to declare the function signature to the user and to request additional
parameters aside from the ones from the Dataset.

A :cpp:type:`GDALDerivedPixelFuncWithArgs` is defined with a signature similar to :cpp:func:`GDALRasterBand::IRasterIO`:


.. cpp:function:: CPLErr TestFunction(void** papoSources, int nSources, void* pData, int nBufXSize, int nBufYSize, GDALDataType eSrcType, GDALDataType eBufType, int nPixelSpace, int nLineSpace, CSLConstList papszArgs)

   :param papoSources: A pointer to packed rasters; one per source.  The
    datatype of all will be the same, specified in the ``eSrcType`` parameter.

   :param nSources: The number of source rasters.

   :param pData: The buffer into which the data should be read, or from which
    it should be written.  This buffer must contain at least ``nBufXSize *
    nBufYSize`` words of type eBufType.  It is organized in left to right, top
    to bottom pixel order.  Spacing is controlled by the ``nPixelSpace`` and
    ``nLineSpace`` parameters.

   :param nBufXSize: The width of the buffer image into which the desired
    region is to be read, or from which it is to be written.

   :param nBufYSize: The height of the buffer image into which the desired
    region is to be read, or from which it is to be written.

   :param eSrcType: The type of the pixel values in the ``papoSources`` raster
    array.

   :param eBufType: The type of the pixel values that the pixel function must
    generate in the ``pData`` data buffer.

   :param nPixelSpace: The byte offset from the start of one pixel value in
    ``pData`` to the start of the next pixel value within a scanline.  If
    defaulted (0) the size of the datatype eBufType is used.

   :param nLineSpace: The byte offset from the start of one scanline in
    pData to the start of the next.

   :param papszArgs: An optional string list of named function arguments (e.g. ``y=4``)


It is also possible to register a :cpp:type:`GDALDerivedPixelFunc` (which omits the final :cpp:type:`CSLConstList` argument) using :cpp:func:`GDALAddDerivedBandPixelFunc`.

The following is an implementation of the pixel function:

.. code-block:: cpp

    #include "gdal.h"

    CPLErr TestFunction(void **papoSources, int nSources, void *pData,
                        int nXSize, int nYSize,
                        GDALDataType eSrcType, GDALDataType eBufType,
                        int nPixelSpace, int nLineSpace,
                        CSLConstList papszArgs)
    {
        int ii, iLine, iCol;
        double pix_val;
        double x0, x3, x4, x8;

        // ---- Init ----
        if (nSources != 4) return CE_Failure;

        const char *pszY = CSLFetchNameValue(papszArgs, "y");
        if (pszY == nullptr) return CE_Failure;

        double NoData = NAN;
        const char *pszNoData = CSLFetchNameValue(papszArgs, "NoData");
        if (pszNoData != nullptr)
        {
            NoData = std::strtod(pszNoData, &end);
            if (end == pszNoData) return CE_Failure; // Could not parse
        }

        char *end = nullptr;
        double y = std::strtod(pszY, &end);
        if (end == pszY) return CE_Failure; // Could not parse

        // ---- Set pixels ----
        for( iLine = 0; iLine < nYSize; iLine++ )
        {
            for( iCol = 0; iCol < nXSize; iCol++ )
            {
                ii = iLine * nXSize + iCol;
                /* Source raster pixels may be obtained with SRCVAL macro */
                x0 = SRCVAL(papoSources[0], eSrcType, ii);
                x3 = SRCVAL(papoSources[1], eSrcType, ii);
                x4 = SRCVAL(papoSources[2], eSrcType, ii);
                x8 = SRCVAL(papoSources[3], eSrcType, ii);

                if (x0 == NoData || x3 == NoData || x4 == NoData || x8 == NoData)
                    pix_val = NAN;
                else
                    pix_val = sqrt((x3*x3+x4*x4)/(x0*x8)) + y;

                GDALCopyWords(&pix_val, GDT_Float64, 0,
                            ((GByte *)pData) + nLineSpace * iLine + iCol * nPixelSpace,
                            eBufType, nPixelSpace, 1);
            }
        }

        // ---- Return success ----
        return CE_None;
    }

Setting the working data type
*****************************

In addition to the subclass specification (VRTDerivedRasterBand) and
the PixelFunctionType value, there is another new parameter that can come
in handy: SourceTransferType.  Typically the source rasters are obtained
using the data type of the derived band.  There might be times,
however, when you want the pixel function to have access to
higher resolution source data than the data type being generated.
For example, you might have a derived band of type "Float", which takes
a single source of type "CFloat32" or "CFloat64", and returns the imaginary
portion.  To accomplish this, set the SourceTransferType to "CFloat64".
Otherwise the source would be converted to "Float" prior to
calling the pixel function, and the imaginary portion would be lost.

.. code-block:: xml

    <VRTDataset rasterXSize="1000" rasterYSize="1000">
        <VRTRasterBand dataType="Float32" band="1" subClass="VRTDerivedRasterBand">
            <Description>Magnitude</Description>
            <PixelFunctionType>MyFirstFunction</PixelFunctionType>
            <SourceTransferType>CFloat64</SourceTransferType>
            ...

.. _python_pixel_functions:

Writing Pixel Functions in Python
+++++++++++++++++++++++++++++++++

In addition to pixel functions written in C/C++ as
documented in the :ref:`vrt_derived_bands` section, it is possible to use
pixel functions written in Python. Both `CPython <https://www.python.org/>`_
and `NumPy <http://www.numpy.org/>`_ are requirements at run-time.

.. note::

   Python pixel functions are not enabled by default; see :ref:`raster_vrt_security_implications`.


The subelements for VRTRasterBand (whose subclass specification must be
set to VRTDerivedRasterBand) are :

- **PixelFunctionType** (required): Must be set to a function name that will be defined as a inline Python module in PixelFunctionCode element or as the form "module_name.function_name" to refer to a function in an external Python module

- **PixelFunctionLanguage** (required): Must be set to Python.

- **PixelFunctionCode** (required if PixelFunctionType is of the form "function_name", ignored otherwise). The in-lined code of a Python module, that must be at least have a function whose name is given by PixelFunctionType.

- **BufferRadius** (optional, defaults to 0): Amount of extra pixels, with respect to the original RasterIO() request to satisfy, that are fetched at the left, right, bottom and top of the input and output buffers passed to the pixel function. Note that the values of the output buffer in this buffer zone will be ignored.

The signature of the Python pixel function must have the following arguments:

- **in_ar**: list of input NumPy arrays. One NumPy array for each source. If SkipNonContributingSources=true, only contributing sources will be passed.
- **out_ar**: output NumPy array to fill. The array is initialized at the right dimensions and with the VRTRasterBand.dataType.
- **xoff**: pixel offset to the top left corner of the accessed region of the band. Generally not needed except if the processing depends on the pixel position in the raster.
- **yoff** line offset to the top left corner of the accessed region of the band. Generally not needed.
- **xsize**: width of the region of the accessed region of the band. Can be used together with out_ar.shape[1] to determine the horizontal resampling ratio of the request.
- **ysize**: height of the region of the accessed region of the band. Can be used together with out_ar.shape[0] to determine the vertical resampling ratio of the request.
- **raster_xsize**: total width of the raster band. Generally not needed.
- **raster_ysize**: total height of the raster band. Generally not needed.
- **buf_radius**: radius of the buffer (in pixels) added to the left, right, top and bottom of in_ar / out_ar. This is the value of the optional BufferRadius element that can be set so that the original pixel request is extended by a given amount of pixels.
- **gt**: geotransform. Array of 6 double values.
- **kwargs**: dictionary with user arguments defined in PixelFunctionArguments

The provided ``out_ar`` array must be modified in-place. Any value currently
returned by the pixel function is ignored.

.. note::

    If wanting to fill ``out_ar`` from another array, use the ``out_ar[:] = ...``
    syntax.

Python pixel function examples
++++++++++++++++++++++++++++++

.. example::
   :title: VRT that multiplies the values of the source file by a factor of 1.5

    .. code-block:: xml

        <VRTDataset rasterXSize="20" rasterYSize="20">
            <SRS>EPSG:26711</SRS>
            <GeoTransform>440720,60,0,3751320,0,-60</GeoTransform>
            <VRTRasterBand dataType="Byte" band="1" subClass="VRTDerivedRasterBand">
                <PixelFunctionType>multiply</PixelFunctionType>
                <PixelFunctionLanguage>Python</PixelFunctionLanguage>
                <PixelFunctionArguments factor="1.5"/>
                <PixelFunctionCode><![CDATA[
                    import numpy as np
                    def multiply(in_ar, out_ar, xoff, yoff, xsize, ysize, raster_xsize,
                                    raster_ysize, buf_radius, gt, **kwargs):
                        factor = float(kwargs['factor'])
                        out_ar[:] = np.round_(np.clip(in_ar[0] * factor,0,255))
                    ]]>
                </PixelFunctionCode>
                <SimpleSource>
                    <SourceFilename relativeToVRT="1">byte.tif</SourceFilename>
                </SimpleSource>
            </VRTRasterBand>
        </VRTDataset>

.. example::
   :title: VRT that adds 2 (or more) rasters

   .. code-block:: xml

       <VRTDataset rasterXSize="20" rasterYSize="20">
           <SRS>EPSG:26711</SRS>
           <GeoTransform>440720,60,0,3751320,0,-60</GeoTransform>
           <VRTRasterBand dataType="Byte" band="1" subClass="VRTDerivedRasterBand">
               <PixelFunctionType>add</PixelFunctionType>
               <PixelFunctionLanguage>Python</PixelFunctionLanguage>
               <PixelFunctionCode><![CDATA[
                   import numpy as np
                   def add(in_ar, out_ar, xoff, yoff, xsize, ysize, raster_xsize,
                                   raster_ysize, buf_radius, gt, **kwargs):
                       np.round_(np.clip(np.sum(in_ar, axis = 0, dtype = 'uint16'),0,255),
                               out = out_ar)
                   ]]>
               </PixelFunctionCode>
               <SimpleSource>
                   <SourceFilename relativeToVRT="1">byte.tif</SourceFilename>
               </SimpleSource>
               <SimpleSource>
                   <SourceFilename relativeToVRT="1">byte2.tif</SourceFilename>
               </SimpleSource>
           </VRTRasterBand>
       </VRTDataset>

.. example::
   :title: VRT that computes hillshading using an external library

   .. code-block:: xml

       <VRTDataset rasterXSize="121" rasterYSize="121">
           <SRS>EPSG:4326</SRS>
           <GeoTransform>-80.004166666666663,0.008333333333333,0,
           44.004166666666663,0,-0.008333333333333</GeoTransform>
           <VRTRasterBand dataType="Byte" band="1" subClass="VRTDerivedRasterBand">
               <ColorInterp>Gray</ColorInterp>
               <SimpleSource>
                   <SourceFilename relativeToVRT="1">n43.dt0</SourceFilename>
               </SimpleSource>
               <PixelFunctionLanguage>Python</PixelFunctionLanguage>
               <PixelFunctionType>hillshading.hillshade</PixelFunctionType>
               <PixelFunctionArguments scale="111120" z_factor="30" />
               <BufferRadius>1</BufferRadius>
               <SourceTransferType>Int16</SourceTransferType>
           </VRTRasterBand>
       </VRTDataset>

   with :file:`hillshading.py`:

   .. code-block:: python

       # Licence: MIT
       # Copyright 2016, Even Rouault
       import math

       def hillshade_int(in_ar, out_ar, xoff, yoff, xsize, ysize, raster_xsize,
                               raster_ysize, radius, gt, z, scale):
           ovr_scale_x = float(out_ar.shape[1] - 2 * radius) / xsize
           ovr_scale_y = float(out_ar.shape[0] - 2 * radius) / ysize
           ewres = gt[1] / ovr_scale_x
           nsres = gt[5] / ovr_scale_y
           inv_nsres = 1.0 / nsres
           inv_ewres = 1.0 / ewres

           az = 315
           alt = 45
           degreesToRadians = math.pi / 180

           sin_alt = math.sin(alt * degreesToRadians)
           azRadians = az * degreesToRadians
           z_scale_factor = z / (8 * scale)
           cos_alt_mul_z_scale_factor = \
                   math.cos(alt * degreesToRadians) * z_scale_factor
           cos_az_mul_cos_alt_mul_z_scale_factor_mul_254 = \
                       254 * math.cos(azRadians) * cos_alt_mul_z_scale_factor
           sin_az_mul_cos_alt_mul_z_scale_factor_mul_254 = \
                       254 * math.sin(azRadians) * cos_alt_mul_z_scale_factor
           square_z_scale_factor = z_scale_factor * z_scale_factor
           sin_alt_mul_254 = 254.0 * sin_alt

           for j in range(radius, out_ar.shape[0]-radius):
               win_line = in_ar[0][j-radius:j+radius+1,:]
               for i in range(radius, out_ar.shape[1]-radius):
                   win = win_line[:,i-radius:i+radius+1].tolist()
                   x = inv_ewres * ((win[0][0] + win[1][0] + win[1][0] + win[2][0])-\
                                   (win[0][2] + win[1][2] + win[1][2] + win[2][2]))
                   y = inv_nsres * ((win[2][0] + win[2][1] + win[2][1] + win[2][2])-\
                                   (win[0][0] + win[0][1] + win[0][1] + win[0][2]))
                   xx_plus_yy = x * x + y * y
                   cang_mul_254 = (sin_alt_mul_254 - \
                       (y * cos_az_mul_cos_alt_mul_z_scale_factor_mul_254 - \
                           x * sin_az_mul_cos_alt_mul_z_scale_factor_mul_254)) / \
                       math.sqrt(1 + square_z_scale_factor * xx_plus_yy)
                   if cang_mul_254 < 0:
                       out_ar[j,i] = 1
                   else:
                       out_ar[j,i] = 1 + round(cang_mul_254)

       def hillshade(in_ar, out_ar, xoff, yoff, xsize, ysize, raster_xsize,
                   raster_ysize, radius, gt, **kwargs):
           z = float(kwargs['z_factor'])
           scale= float(kwargs['scale'])
           hillshade_int(in_ar, out_ar, xoff, yoff, xsize, ysize, raster_xsize,
                       raster_ysize, radius, gt, z, scale)

.. note::

   When importing modules from inline Python code or when relying on out-of-line
   code (PixelFunctionType of the form "module_name.function_name"), you need
   to make sure the modules are accessible through the python path. Note that
   contrary to the Python interactive interpreter, the current path is not
   automatically added when used from GDAL. So you may need to define the
   **PYTHONPATH** environment variable if you get ModuleNotFoundError exceptions.

.. _raster_vrt_security_implications:

Security implications
+++++++++++++++++++++

The ability to run Python code potentially opens the door to many potential
vulnerabilities if the user of GDAL may process untrusted datasets. To avoid
such issues, by default, execution of Python pixel function will be disabled.
The execution policy can be controlled with the following
configuration options:

-  .. config:: GDAL_VRT_ENABLE_PYTHON
      :choices: YES, NO, TRUSTED_MODULES
      :default: TRUSTED_MODULES

      Determine what Python code can be called from GDAL.

      - YES: all VRT scripts are considered as trusted and their Python pixel functions will be run when pixel operations are involved.
      - NO: all VRT scripts are considered untrusted, and none Python pixelfunction will be run.
      - TRUSTED_MODULES (default setting): all VRT scripts with inline Python code in their PixelFunctionCode elements will be considered untrusted and will not be run. VRT scripts that use a PixelFunctionType of the form "module_name.function_name" will be considered as trusted, only if "module_name" is allowed in the :config:`GDAL_VRT_PYTHON_TRUSTED_MODULES` configuration option.

-  .. config:: GDAL_VRT_PYTHON_TRUSTED_MODULES

      The value of this configuration option is a comma separated listed of trusted module names. The '*' wildcard can be used at the name of a string to match all strings beginning with the substring before the '*' character. For example 'every*' will make 'every.thing' or 'everything' module trusted. '*' can also be used to make all modules to be trusted. The ".*" wildcard can also be used to match exact modules or submodules names. For example 'every.*' will make 'every' and 'every.thing' modules trusted, but not 'everything'.

.. _linking_mechanism_to_python_interpreter:

Linking mechanism to a Python interpreter
*****************************************

Currently only CPython 3 is supported. The GDAL shared object
is not explicitly linked at build time to any of the CPython library. When GDAL
will need to run Python code, it will first determine if the Python interpreter
is loaded in the current process (which is the case if the program is a Python
interpreter itself, or if another program, e.g. QGIS, has already loaded the
CPython library). Otherwise it will look if the :config:`PYTHONSO` configuration option is
defined. This option can be set to point to the name of the Python library to
use, either as a shortname like "libpython3.10.so" if it is accessible through
the Linux dynamic loader (so typically in one of the paths in /etc/ld.so.conf or
LD_LIBRARY_PATH) or as a full path name like "/usr/lib/x86_64-linux-gnu/libpython3.10.so".
The same holds on Windows will shortnames like "python310.dll" if accessible through
the PATH or full path names like "c:\\python310\\python310.dll". If the :config:`PYTHONSO`
configuration option is not defined, it will look for a "python" binary in the
directories of the PATH and will try to determine the related shared object
(it will retry with "python3" if no "python" has been found). If the above
was not successful, then a predefined list of shared objects names
will be tried. As of GDAL 3.12, the order of versions searched is
3.8, 3.9, 3.10, 3.11, 3.12, 3.13, 3.14, 3.7, 3.6, 3.5.
Enabling debug information (:config:`CPL_DEBUG=ON`) will show which Python version is used.

Just-in-time compilation
++++++++++++++++++++++++

The use of a just-in-time compiler may significantly speed up execution times.
`Numba <http://numba.pydata.org/>`_ has been successfully tested. For
better performance, it is recommended to use a offline pixel function so that
the just-in-time compiler may cache its compilation.

Given the following :file:`mandelbrot.py` file :

.. code-block:: python

    # Trick for compatibility with and without numba
    try:
        from numba import jit
        #print('Using numba')
        g_max_iterations = 100
    except Exception:
        class jit:
            def __init__(self, nopython = True, nogil = True):
                pass

            def __call__(self, f):
                return f

        #print('Using non-JIT version')
        g_max_iterations = 25

    # Use a wrapper for the entry point regarding GDAL, since GDAL cannot access
    # the jit decorated function with the expected signature.
    def mandelbrot(in_ar, out_ar, xoff, yoff, xsize, ysize, raster_xsize,
                            raster_ysize, r, gt, **kwargs):
        mandelbrot_jit(out_ar, xoff, yoff, xsize, ysize, raster_xsize, raster_ysize,
    g_max_iterations)

    # Will make sure that the code is compiled to pure native code without Python
    # fallback.
    @jit(nopython=True, nogil=True, cache=True)
    def mandelbrot_jit(out_ar, xoff, yoff, xsize, ysize, raster_xsize,
                            raster_ysize, max_iterations):
        ovr_factor_y = float(out_ar.shape[0]) / ysize
        ovr_factor_x = float(out_ar.shape[1]) / xsize
        for j in range( out_ar.shape[0]):
            y0 = 2.0 * (yoff + j / ovr_factor_y) / raster_ysize - 1
            for i in range(out_ar.shape[1]):
                x0 = 3.5 * (xoff + i / ovr_factor_x) / raster_xsize - 2.5
                x = 0.0
                y = 0.0
                x2 = 0.0
                y2 = 0.0
                iteration = 0
                while x2 + y2 < 4 and iteration < max_iterations:
                    y = 2*x*y + y0
                    x = x2 - y2 + x0
                    x2 = x * x
                    y2 = y * y
                    iteration += 1

                out_ar[j][i] = iteration * 255 / max_iterations

The following VRT file can be used (to be opened with QGIS for example)

.. code-block:: xml

    <VRTDataset rasterXSize="100000000" rasterYSize="100000000">
        <VRTRasterBand dataType="Byte" band="1" subClass="VRTDerivedRasterBand">
            <PixelFunctionLanguage>Python</PixelFunctionLanguage>
            <PixelFunctionType>mandelbrot.mandelbrot</PixelFunctionType>
            <Metadata>
            <MDI key="STATISTICS_MAXIMUM">255</MDI>
            <MDI key="STATISTICS_MEAN">127</MDI>
            <MDI key="STATISTICS_MINIMUM">0</MDI>
            <MDI key="STATISTICS_STDDEV">127</MDI>
            </Metadata>
            <ColorInterp>Gray</ColorInterp>
            <Histograms>
            <HistItem>
                <HistMin>-0.5</HistMin>
                <HistMax>255.5</HistMax>
                <BucketCount>256</BucketCount>
                <IncludeOutOfRange>0</IncludeOutOfRange>
                <Approximate>1</Approximate>
                <HistCounts>0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|
        0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|
        0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|
        0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|
        0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|
        0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|
        0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0|0</HistCounts>
            </HistItem>
            </Histograms>
        </VRTRasterBand>
    </VRTDataset>

.. _gdal_vrttut_warped:

Warped VRT
----------

A warped VRT is a VRTDataset with subClass="VRTWarpedDataset". It has a
GDALWarpOptions element which describe the warping options.

.. code-block:: xml

    <VRTDataset rasterXSize="20" rasterYSize="20" subClass="VRTWarpedDataset">
        <SRS>PROJCS["NAD27 / UTM zone 11N",GEOGCS["NAD27",DATUM["North_American_Datum_1927",SPHEROID["Clarke 1866",6378206.4,294.9786982138982,AUTHORITY["EPSG","7008"]],AUTHORITY["EPSG","6267"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AUTHORITY["EPSG","4267"]],PROJECTION["Transverse_Mercator"],PARAMETER["latitude_of_origin",0],PARAMETER["central_meridian",-117],PARAMETER["scale_factor",0.9996],PARAMETER["false_easting",500000],PARAMETER["false_northing",0],UNIT["metre",1,AUTHORITY["EPSG","9001"]],AXIS["Easting",EAST],AXIS["Northing",NORTH],AUTHORITY["EPSG","26711"]]</SRS>
        <GeoTransform>  4.4072000000000000e+05,  6.0000000000000000e+01,  0.0000000000000000e+00,  3.7513200000000000e+06,  0.0000000000000000e+00, -6.0000000000000000e+01</GeoTransform>
        <Metadata>
            <MDI key="AREA_OR_POINT">Area</MDI>
        </Metadata>
        <VRTRasterBand dataType="Byte" band="1" subClass="VRTWarpedRasterBand">
            <ColorInterp>Gray</ColorInterp>
        </VRTRasterBand>
        <BlockXSize>20</BlockXSize>
        <BlockYSize>20</BlockYSize>
        <GDALWarpOptions>
            <WarpMemoryLimit>6.71089e+07</WarpMemoryLimit>
            <ResampleAlg>NearestNeighbour</ResampleAlg>
            <WorkingDataType>Byte</WorkingDataType>
            <Option name="INIT_DEST">0</Option>
            <SourceDataset relativeToVRT="1">byte.vrt</SourceDataset>
            <Transformer>
            <ApproxTransformer>
                <MaxError>0.125</MaxError>
                <BaseTransformer>
                <GenImgProjTransformer>
                    <SrcGeoTransform>440720,60,0,3751320,0,-60</SrcGeoTransform>
                    <SrcInvGeoTransform>-7345.33333333333303,0.0166666666666666664,0,62522,0,-0.0166666666666666664</SrcInvGeoTransform>
                    <DstGeoTransform>440720,60,0,3751320,0,-60</DstGeoTransform>
                    <DstInvGeoTransform>-7345.33333333333303,0.0166666666666666664,0,62522,0,-0.0166666666666666664</DstInvGeoTransform>
                </GenImgProjTransformer>
                </BaseTransformer>
            </ApproxTransformer>
            </Transformer>
            <BandList>
            <BandMapping src="1" dst="1" />
            </BandList>
        </GDALWarpOptions>
    </VRTDataset>

.. _gdal_vrttut_pansharpen:

Pansharpened VRT
----------------

.. versionadded:: 2.1

A VRT can describe a dataset resulting from a
`pansharpening operation <https://en.wikipedia.org/wiki/Pansharpened_image>`_
The pansharpening VRT combines a panchromatic band with several spectral bands
of lower resolution to generate output spectral bands of the same resolution as
the panchromatic band.

VRT pansharpening assumes that the panchromatic and spectral bands have the same
projection (or no projection). If that is not the case, reprojection must be done in a prior step.
Bands might have different geotransform matrices, in which case, by default, the
resulting dataset will have as extent the union of all extents.

Currently the only supported pansharpening algorithm is a "weighted" Brovey algorithm.
The general principle of this algorithm is that, after resampling the spectral bands
to the resolution of the panchromatic band, a pseudo panchromatic intensity is computed
from a weighted average of the spectral bands. Then the output value of the spectral
band is its input value multiplied by the ratio of the real panchromatic intensity
over the pseudo panchromatic intensity.

Corresponding pseudo code:

::

    pseudo_panchro[pixel] = sum(weight[i] * spectral[pixel][i] for i=0 to nb_spectral_bands-1)
    ratio = panchro[pixel] / pseudo_panchro[pixel]
    for i=0 to nb_spectral_bands-1:
        output_value[pixel][i] = input_value[pixel][i] * ratio

A valid pansharpened VRT must declare subClass="VRTPansharpenedDataset" as an
attribute of the VRTDataset top element. The VRTDataset element must have a
child **PansharpeningOptions** element. This PansharpeningOptions element must have
a **PanchroBand** child element and one of several **SpectralBand** elements.
PanchroBand and SpectralBand elements must have at least a **SourceFilename** child
element to specify the name of the dataset. They may also have a **SourceBand** child
element to specify the number of the band in the dataset (starting with 1). If not
specify, the first band will be assumed.
**OpenOptions** can also be specified

The SpectralBand element must generally have a **dstBand** attribute to specify the
number of the output band (starting with 1) to which the input spectral band must be mapped.
If the attribute is not specified, the spectral band will be taken into account
in the computation of the pansharpening, but not exposed as an output band.

Panchromatic and spectral bands should generally come from different datasets,
since bands of a GDAL dataset are assumed to have all the same dimensions.
Spectral bands themselves can come from one or several datasets. The only
constraint is that they have all the same dimensions and geotransform.

An example of a minimalist working VRT is the following. It will generates a dataset with 3 output
bands corresponding to the 3 input spectral bands of multispectral.tif, pansharpened
with panchromatic.tif.

.. code-block:: xml

    <VRTDataset subClass="VRTPansharpenedDataset">
        <PansharpeningOptions>
            <PanchroBand>
                <SourceFilename relativeToVRT="1">panchromatic.tif</SourceFilename>
                <OpenOptions>
                    <OOI key="NUM_THREADS">ALL_CPUS</OOI>
                </OpenOptions>
                <SourceBand>1</SourceBand>
            </PanchroBand>
            <SpectralBand dstBand="1">
                <SourceFilename relativeToVRT="1">multispectral.tif</SourceFilename>
                <OpenOptions>
                    <OOI key="NUM_THREADS">ALL_CPUS</OOI>
                </OpenOptions>
                <SourceBand>1</SourceBand>
            </SpectralBand>
            <SpectralBand dstBand="2">
                <SourceFilename relativeToVRT="1">multispectral.tif</SourceFilename>
                <OpenOptions>
                    <OOI key="NUM_THREADS">ALL_CPUS</OOI>
                </OpenOptions>
                <SourceBand>2</SourceBand>
            </SpectralBand>
            <SpectralBand dstBand="3">
                <SourceFilename relativeToVRT="1">multispectral.tif</SourceFilename>
                <OpenOptions>
                    <OOI key="NUM_THREADS">ALL_CPUS</OOI>
                </OpenOptions>
                <SourceBand>3</SourceBand>
            </SpectralBand>
        </PansharpeningOptions>
    </VRTDataset>

In the above example, 3 output pansharpend bands will be created from the 3 declared
input spectral bands. The weights will be 1/3. Cubic resampling will be used. The
projection and geotransform from the panchromatic band will be reused for the VRT
dataset.

It is possible to create more explicit and declarative pansharpened VRT, allowing
for example to only output part of the input spectral bands (e.g. only RGB when
the input multispectral dataset is RGBNir). It is also possible to add "classic"
VRTRasterBands, in addition to the pansharpened bands.

In addition to the above mentioned required PanchroBand and SpectralBand elements,
the PansharpeningOptions element may have the following children elements :

- **Algorithm**: to specify the pansharpening algorithm. Currently, only WeightedBrovey is supported.
- **AlgorithmOptions**: to specify the options of the pansharpening algorithm. With WeightedBrovey algorithm, the only supported option is a **Weights** child element whose content must be a comma separated list of real values assigning the weight of each of the declared input spectral bands. There must be as many values as declared input spectral bands.
- **Resampling**: the resampling kernel used to resample the spectral bands to the resolution of the panchromatic band. Can be one of Cubic (default), Average, Near, CubicSpline, Bilinear, Lanczos.
- **NumThreads**: Number of worker threads. Integer number or ALL_CPUS. If this option is not set, the :config:`GDAL_NUM_THREADS` configuration option will be queried (its value can also be set to an integer or ALL_CPUS)
- **BitDepth**: Can be used to specify the bit depth of the panchromatic and spectral bands (e.g. 12). If not specified, the NBITS metadata item from the panchromatic band will be used if it exists.
- **NoData**: Nodata value to take into account for panchromatic and spectral bands. It will be also used as the output nodata value. If not specified and all input bands have the same nodata value, it will be implicitly used (unless the special None value is put in NoData to prevent that).
- **SpatialExtentAdjustment**: Can be one of **Union** (default), **Intersection**, **None** or **NoneWithoutWarning**. Controls the behavior when panchromatic and spectral bands have not the same geospatial extent. By default, Union will take the union of all spatial extents. Intersection the intersection of all spatial extents. None will not proceed to any adjustment at all, but will emit a warning. NoneWithoutWarning is the same as None, but in a silent way.

The below examples creates a VRT dataset with 4 bands. The first band is the
panchromatic band. The 3 following bands are than red, green, blue pansharpened
bands computed from a multispectral raster with red, green, blue and near-infrared
bands. The near-infrared bands is taken into account for the computation of the
pseudo panchromatic intensity, but not bound to an output band.

.. code-block:: xml

    <VRTDataset rasterXSize="800" rasterYSize="400" subClass="VRTPansharpenedDataset">
        <SRS>WGS84</SRS>
        <GeoTransform>-180, 0.45, 0, 90, 0, -0.45</GeoTransform>
        <Metadata>
            <MDI key="DESCRIPTION">Panchromatic band + pan-sharpened red, green and blue bands</MDI>
        </Metadata>
        <VRTRasterBand dataType="Byte" band="1" >
            <SimpleSource>
                <SourceFilename relativeToVRT="1">world_pan.tif</SourceFilename>
                <SourceBand>1</SourceBand>
            </SimpleSource>
        </VRTRasterBand>
        <VRTRasterBand dataType="Byte" band="2" subClass="VRTPansharpenedRasterBand">
            <ColorInterp>Red</ColorInterp>
        </VRTRasterBand>
        <VRTRasterBand dataType="Byte" band="3" subClass="VRTPansharpenedRasterBand">
            <ColorInterp>Green</ColorInterp>
        </VRTRasterBand>
        <VRTRasterBand dataType="Byte" band="4" subClass="VRTPansharpenedRasterBand">
            <ColorInterp>Blue</ColorInterp>
        </VRTRasterBand>
        <BlockXSize>256</BlockXSize>
        <BlockYSize>256</BlockYSize>
        <PansharpeningOptions>
            <Algorithm>WeightedBrovey</Algorithm>
            <AlgorithmOptions>
                <Weights>0.25,0.25,0.25,0.25</Weights>
            </AlgorithmOptions>
            <Resampling>Cubic</Resampling>
            <NumThreads>ALL_CPUS</NumThreads>
            <BitDepth>8</BitDepth>
            <NoData>0</NoData>
            <SpatialExtentAdjustment>Union</SpatialExtentAdjustment>
            <PanchroBand>
                <SourceFilename relativeToVRT="1">world_pan.tif</SourceFilename>
                <SourceBand>1</SourceBand>
            </PanchroBand>
            <SpectralBand dstBand="2">
                <SourceFilename relativeToVRT="1">world_rgbnir.tif</SourceFilename>
                <SourceBand>1</SourceBand>
            </SpectralBand>
            <SpectralBand dstBand="3">
                <SourceFilename relativeToVRT="1">world_rgbnir.tif</SourceFilename>
                <SourceBand>2</SourceBand>
            </SpectralBand>
            <SpectralBand dstBand="4">
                <SourceFilename relativeToVRT="1">world_rgbnir.tif</SourceFilename>
                <SourceBand>3</SourceBand>
            </SpectralBand>
            <SpectralBand> <!-- note the absence of the dstBand attribute, to indicate
                                that the NIR band is not bound to any output band -->
                <SourceFilename relativeToVRT="1">world_rgbnir.tif</SourceFilename>
                <SourceBand>4</SourceBand>
            </SpectralBand>
        </PansharpeningOptions>
    </VRTDataset>

Multidimensional VRT
---------------------

.. versionadded:: 3.1

See the dedicated :ref:`vrt_multidimensional` page.

.. toctree::
   :maxdepth: 1
   :hidden:

   vrt_multidimensional

Processed dataset VRT
---------------------

.. versionadded:: 3.9

A VRT processed dataset is a specific variant of the :ref:`raster.vrt` format,
to apply chained processing steps that may apply to several bands at the same time.

See the dedicated :ref:`vrt_processed_dataset` page.

.. toctree::
   :maxdepth: 1
   :hidden:

   vrt_processed_dataset

vrt:// connection string
------------------------

.. versionadded:: 3.1

In some contexts, it might be useful to benefit from features of VRT without
having to create a file or to provide the rather verbose VRT XML content as
the connection string. For that purpose, the following URI syntax is supported for
the dataset name since GDAL 3.1

::

    vrt://{path_to_gdal_dataset}?[bands=num1,...,numN]

::

    vrt://{path_to_gdal_dataset}?[a_srs=srs_def]

::

    vrt://{path_to_gdal_dataset}?[a_ullr=ulx,uly,lrx,lry]


For example:

::

    vrt://my.tif?bands=3,2,1

::

    vrt://my.tif?a_srs=OGC:CRS84

::

    vrt://my.tif?a_ullr=0,1,1,-1

::

    vrt://my.tif?bands=2&ovr=4

::

    vrt://my.nc?sd_name=sds


The supported options currently are ``bands``, ``a_nodata``, ``a_srs``, ``a_ullr``, ``ovr``, ``expand``,
``a_scale``, ``a_offset``, ``ot``, ``gcp``, ``if``, ``scale``, ``exponent``, ``outsize``, ``projwin``,
``projwin_srs``, ``tr``, ``r``, ``srcwin``, ``a_gt``, ``oo``, ``unscale``, ``a_coord_epoch``, ``nogcp``, ``epo``, ``eco``, ``sd_name``, and ``sd``.

Other options may be added in the future.

The effect of the ``bands`` option is to change the band composition. The values specified
are the source band numbers (between 1 and N), possibly out-of-order or with repetitions.
The ``mask`` value can be used to specify the global mask band. This can also
be seen as an equivalent of running `gdal_translate -of VRT -b num1 ... -b numN`.

The effect of the ``a_nodata`` option (added in GDAL 3.9) is to assign (override) the nodata
value of the source in the same way as (:ref:`gdal_translate`).

The effect of the ``a_srs`` option (added in GDAL 3.7) is to assign (override) the coordinate
reference system of the source in the same way as (:ref:`gdal_translate`), it may be missing,
or incorrect. The value provided for ``a_srs`` may be a string or a file containing a srs
definition.

The effect of the ``a_ullr`` option (added in GDAL 3.7) is to assign (override) the georeferenced
bounds of the source in the same way as (:ref:`gdal_translate`). The value consists of four numeric
values separated by commas, in the order 'xmin,ymax,xmax,ymin' (upper left x,y, lower right x,y).

The effect of the ``ovr`` option (added in GDAL 3.7) is to specify which overview
level of source file must be used, with the first overview level being 0 (:ref:`gdal_translate`).

The effect of the ``expand`` option (added in GDAL 3.7) is to expose a dataset with 1 band with
a color table as a dataset with 3 (RGB) or 4 (RGBA) bands, as with (:ref:`gdal_translate`).

The effect of the ``a_scale`` option (added in GDAL 3.7) is to set band scaling value (no
modification of pixel values is done), as with (:ref:`gdal_translate`).

The effect of the ``a_offset`` option (added in GDAL 3.7) is to set band offset value (no
modification of pixel values is done), as with (:ref:`gdal_translate`).

The effect of the ``ot`` option (added in GDAL 3.7) is to force the output image bands to have a
specific data type supported by the driver as with (:ref:`gdal_translate`).

The effect of the ``gcp`` option (added in GDAL 3.7) is to add the indicated ground control point
to the output dataset. Values are a set of numbers as per (:ref:`gdal_translate`) ``pixel,line,easting,northing[,elevation]``.
Multiple entries may be included. This can also be seen as an equivalent of running
`gdal_translate -of VRT -gcp pixel1 line1 easting1 northing1 [elevation1] -gcp pixel2 line2 easting2 northing2 [elevation2] ... -gcp pixelN lineN eastingN northingN [elevationN]`.

The effect of the ``if`` option (added in GDAL 3.7) is to specify the format/driver name/s
to be attempted to open the input file (:ref:`gdal_translate`). Values may be repeated separated by comma
This can also be seen as an equivalent of running `gdal_translate -of VRT -if DRV1 -if DRV2 ... -if DRVN`.

The effect of the ``scale`` option (added in GDAL 3.7) is to rescale the input pixel values from the
range **src_min** to **src_max** to the range **dst_min** to **dst_max**  ``src_min,src_max[,dst_min,dst_max]``
either 2 or 4 comma separated values. The same rules apply for the source and destination ranges, and ``scale_bn`` syntax may be used as it is with (:ref:`gdal_translate`).  The option ``scale=true`` (default if unspecified is ``scale=false``) may also be used without value arguments (added in GDAL 3.8), where it results in the output range 0,255 from whatever the source range is. Do consider the need for also using ``ot`` option in order to accommodate the intended output range.

The effect of the ``exponent`` option (added in GDAL 3.7) is to apply non-linear scaling with a power function,
a single value to be used with the ``scale`` option. The same ``exponent_bn`` syntax may be used in combination with ``scale_bn`` to
target specific band/s as per (:ref:`gdal_translate`).

The effect of the ``outsize`` option (added in GDAL 3.7) is to set the size of the output, in numbers `pixel,line` or in fraction `pixel%,line%` as per (:ref:`gdal_translate`).

The effect of the ``projwin`` option (added in GDAL 3.8) is to select a subwindow from the source image in georeferenced
coordinates in the same way as (:ref:`gdal_translate`). The value consists of four numeric values separated by commas, in
the order 'xmin,ymax,xmax,ymin', these are in the native georeferenced coordinates of the source unless ``projwin_srs`` is also
provided.

The effect of the ``projwin_srs`` option (added in GDAL 3.8) is to specify the SRS in which to interpret the coordinates given with ``projwin`` in the same way as (:ref:`gdal_translate`). This option only applies if ``projwin`` is also supplied.

The effect of the ``tr`` option (added in GDAL 3.8) is to set the target resolution, two positive values in georeferenced coordinates, applied in the same way as (:ref:`gdal_translate`). The value consists of two numeric values separated by commas in the order 'xres,yres'.

The effect of the ``r`` option (added in GDAL 3.8) is to set the resampling algorithm used, with 'nearest' as the default. This is applied in the same way as (:ref:`gdal_translate`).

The effect of the ``srcwin`` option (added in GDAL 3.8) is to select a subwindow from the source image based on pixel/line location as with (:ref:`gdal_translate`). The value consists of four integer values separated by commas, in
the order 'xoff,yoff,xsize,ysize'.

The effect of the ``a_gt`` option (added in GDAL 3.8) is to override/assign the geotransform of the output as with (:ref:`gdal_translate`). The value consists of six numeric values separated by commas, in
the order 'gt(0),gt(1),gt(2),gt(3),gt(4),gt(5)'.

The effect of the ``oo`` option (added in GDAL 3.8) is to set driver-specific dataset open options, multiple values are allowed. The value
consists of string key value pairs with multiple pairs separated by commas e.g. ``oo=<key>=<val>`` or . ``oo=<key1>=<val1>,<key2>=<val2>,...``. This is applied in the same way as (:ref:`gdal_translate`).

The effect of the ``unscale`` option (added in GDAL 3.8) is to apply the scale/offset metadata for the bands to convert scaled values to unscaled values. To apply this use syntax ``unscale=true``, or ``unscale=false`` (which is the default if not specified). Do consider the need for also using ``ot`` option in order to accommodate the intended output range, see more details for the same argument as with (:ref:`gdal_translate`).

The effect of the ``a_coord_epoch`` option (added in GDAL 3.8) is to assign a coordinate epoch, linked with the output SRS as
with (:ref:`gdal_translate`).

The effect of the ``nogcp`` option (added in GDAL 3.8) is to not copy the GCPs in the source dataset to the output dataset (:ref:`gdal_translate`). To apply this
use syntax ``nogcp=true``, or ``nogcp=false`` (which is the default if not specified).

The effect of the ``epo`` option (added in GDAL 3.8) is that ``srcwin`` or ``projwin`` values that fall partially outside the source raster extent will be considered as an error as per (:ref:`gdal_translate`). To apply this use syntax ``epo=true``, or ``epo=false`` (which is the default if not specified).

The effect of the ``eco`` option (added in GDAL 3.8) is that ``srcwin`` or ``projwin`` values that fall completely outside the source raster extent will be considered as an error as per (:ref:`gdal_translate`). To apply this use syntax ``eco=true``, or ``eco=false`` (which is the default if not specified).

The effect of the ``sd_name`` option (added in GDAL 3.9) is to choose an individual subdataset by
name for sources that have multiple subdatasets. This means that rather than a fully-qualified description
such as "NETCDF:myfile.nc:somearray" we may use "vrt://myfile.nc?sd_name=somearray". This option
is mutually exclusive with ``sd``, and with ``transpose``.

The effect of the ``sd`` option (added in GDAL 3.9) is to choose an individual subdataset by
number for sources that have multiple subdatasets. This means that rather than a fully-qualified
description such as "NETCDF:myfile.nc:somearray" we may use "vrt://myfile.nc?sd=<n>" where "<n>"
is between 1 and the number of subdatasets. Note that there is no guarantee of the order of the
subdatasets within a source between GDAL versions (or in some cases between file series in datasets). This
mode is for convenience only, please use ``sd_name`` to choose a subdataset by name explicitly.
This option is mutually exclusive with ``sd_name``, and with ``transpose``.

The effect of the ``transpose`` option (added in GDAL 3.12) is to specify just one array by name from a
multidimensional dataset and nominate the indexes for the two axes that define the 2D dataset. This is
an interface to the function :cpp:func:`GDALMDArray::AsClassicDataset` in the multidimensional raster model.
This can be valuable for reorienting an array that presents X and Y in YX or some other order. (There's a
possible added advantage that a valid geotransform may be provided that the classic 2D model doesn't yet infer,
because the multidimensional model can derive one from coordinates referenced in that form).
The usage syntax is ``vrt://somefile.extension?transpose=varname:iXDim,iYDim`` with a no-op case
``vrt://somefile.extension?transpose=varname:0,1`` and ``vrt://somefile.extension?transpose=varname:1,0`` would be a
transpose on the first two axes. There must be two unique axis indexes with values between 0 and the maximum available.
This option is mutually exclusive with ``sd_name`` and ``sd``.

The options may be chained together separated by '&'. (Beware the need for quoting to protect
the ampersand).

::

    "vrt://my.tif?a_srs=OGC:CRS84&bands=2,1&a_ullr=-180,90,180,-90"



Multi-threading optimizations
-----------------------------

Starting with GDAL 3.6, the ComputeStatistics() implementation can benefit from
multi-threading if the sources are not overlapping and belong to different
datasets. This can be enabled by setting the :config:`GDAL_NUM_THREADS`
configuration option to an integer or ``ALL_CPUS``.

Starting with GDAL 3.10, the :oo:`NUM_THREADS` open option can
be set to control specifically the multi-threading of VRT datasets.
It defaults to ``ALL_CPUS``, and when set, overrides :config:`GDAL_NUM_THREADS`
or :config:`VRT_NUM_THREADS`. It applies to
ComputeStatistics() and band-level and dataset-level RasterIO().
For band-level RasterIO(), multi-threading is only available if more than 1
million pixels are requested and if the VRT is made of only non-overlapping
SimpleSource or ComplexSource belonging to different datasets.
For dataset-level RasterIO(), multi-threading is only available if more than 1
million pixels are requested and if the VRT is made of only non-overlapping
SimpleSource belonging to different datasets.

-  .. oo:: NUM_THREADS
      :choices: integer, ALL_CPUS
      :default: ALL_CPUS

      Determines the number of threads used when an operation reads from
      multiple sources.

This can also be specified globally with the :config:`VRT_NUM_THREADS`
configuration option.

-  .. config:: VRT_NUM_THREADS
      :choices: integer, ALL_CPUS
      :default: ALL_CPUS

      Determines the number of threads used when an operation reads from
      multiple sources.

Note that the number of threads actually used is also limited by the
:config:`GDAL_MAX_DATASET_POOL_SIZE` configuration option.

Multi-threading issues
----------------------

.. warning::

    The below section applies to GDAL <= 2.2. Starting with GDAL 2.3, the use
    of VRT datasets is subject to the standard GDAL dataset multi-threaded rules
    (that is a VRT dataset handle may only be used by a same thread at a time,
    but you may open several dataset handles on the same VRT file and use them
    in different threads)

When using VRT datasets in a multi-threading environment, you should be
careful to open the VRT dataset by the thread that will use it afterwards. The
reason for that is that the VRT dataset uses :cpp:func:`GDALOpenShared` when opening the
underlying datasets. So, if you open twice the same VRT dataset by the same
thread, both VRT datasets will share the same handles to the underlying
datasets.

The shared attribute, on the SourceFilename indicates whether the
dataset should be shared (value is 1) or not (value is 0). The default is 1.
If several VRT datasets referring to the same underlying sources are used in a multithreaded context,
shared should be set to 0. Alternatively, the :config:`VRT_SHARED_SOURCE` configuration
option can be set to ``NO`` to force non-shared mode:

-  .. config:: VRT_SHARED_SOURCE
      :choices: YES, NO
      :default: YES

      Determines whether a VRT dataset should open its underlying sources in
      shared mode, for ``SourceFilename`` elements that do not specify a
      ``shared`` attribute. When the ``shared`` attribute is present this
      configuration option is ignored.

Performance considerations
--------------------------

A VRT can reference many (hundreds, thousands, or more) datasets. Due to
operating system limitations, and for performance at opening time, it is
not reasonable/possible to open them all at the same time. GDAL has a "pool"
of datasets opened by VRT files whose maximum limit is 100 by default. When it
needs to access a dataset referenced by a VRT, it checks if it is already in
the pool of open datasets. If not, when the pool has reached its limit, it closes
the least recently used dataset to be able to open the new one. This maximum
limit of the pool can be increased by setting the :config:`GDAL_MAX_DATASET_POOL_SIZE`
configuration option to a bigger value. Note that a typical user process on
Linux is limited to 1024 simultaneously opened files, and you should let some
margin for shared libraries, etc...
gdal_translate and gdalwarp, by default, increase the pool size to 450.

Starting with GDAL 3.7, the :config:`GDAL_MAX_DATASET_POOL_RAM_USAGE`
configuration option to a number of bytes, to limit the RAM usage of opened
datasets in the pool.

Driver capabilities
-------------------

.. supports_createcopy::

.. supports_create::

.. supports_georeferencing::

.. supports_virtualio::
