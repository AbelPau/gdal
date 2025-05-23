.. _vector_api_tut:

================================================================================
Vector API tutorial
================================================================================

This document is intended to document using the OGR C++ classes to read
and write data from a file.  It is strongly advised that the reader first
review the :ref:`vector_data_model` document describing
the key classes and their roles in OGR.

It also includes code snippets for the corresponding functions in C and Python.

Reading From OGR
----------------

For purposes of demonstrating reading with OGR, we will construct a small
utility for dumping point layers from an OGR data source to stdout in
comma-delimited format.

Initially it is necessary to register all the format drivers that are desired.
This is normally accomplished by calling :cpp:func:`GDALAllRegister` which registers
all format drivers built into GDAL/OGR.

.. tabs::

   .. code-tab:: c++

      #include "ogrsf_frmts.h"

      int main()

      {
          GDALAllRegister();


   .. code-tab:: c

      #include "gdal.h"

      int main()

      {
          GDALAllRegister();

   .. code-tab:: python

      from osgeo import gdal
      # when importing gdal in Python
      # GDALAllRegister() is automatically called

Next we need to open the input OGR datasource.  Datasources can be files,
RDBMSes, directories full of files, or even remote web services depending on
the driver being used.  However, the datasource name is always a single
string.  In this case we are hardcoded to open a particular shapefile.
The second argument (GDAL_OF_VECTOR) tells the :cpp:func:`OGROpen` method
that we want a vector driver to be use and that don't require update access.
On failure NULL is returned, and
we report an error.

.. tabs::

   .. code-tab:: c++

      GDALDataset       *poDS;

      poDS = (GDALDataset*) GDALOpenEx( "point.shp", GDAL_OF_VECTOR, NULL, NULL, NULL );
      if( poDS == NULL )
      {
          printf( "Open failed.\n" );
          exit( 1 );
      }

   .. code-tab:: c

      GDALDatasetH hDS;

      hDS = GDALOpenEx( "point.shp", GDAL_OF_VECTOR, NULL, NULL, NULL );
      if( hDS == NULL )
      {
          printf( "Open failed.\n" );
          exit( 1 );
      }

   .. code-tab:: python

      ds = gdal.OpenEx("point", gdal.OF_VECTOR)

A GDALDataset can potentially have many layers associated with it.  The
number of layers available can be queried with :cpp:func:`GDALDataset::GetLayerCount`
and individual layers fetched by index using :cpp:func:`GDALDataset::GetLayer`.
However, we will just fetch the layer by name.

.. tabs::

   .. code-tab:: c++

      OGRLayer  *poLayer;

      poLayer = poDS->GetLayerByName( "point" );

   .. code-tab:: c

      OGRLayerH hLayer;

      hLayer = GDALDatasetGetLayerByName( hDS, "point" );

   .. code-tab:: python

      lyr = ds.GetLayerByName("point")

Now we want to start reading features from the layer.  Before we start we
could assign an attribute or spatial filter to the layer to restrict the set
of feature we get back, but for now we are interested in getting all features.

.. tabs::

   .. code-tab:: c++

      for( auto& poFeature: poLayer )
      {
            // do something with each feature
      }

   .. code-tab:: c

      OGR_FOR_EACH_FEATURE_BEGIN(hFeature, hLayer)
      {
           // do something, including continue, break;
           // do not explicitly destroy the feature (unless you use return or goto
           // outside of the loop, in which case use OGR_F_Destroy(hFeat))
      }
      OGR_FOR_EACH_FEATURE_END(hFeat)

   .. code-tab:: python

      for feat in lyr:
        # do something with each feature

In order to dump all the attribute fields of the feature, it is helpful
to get the :cpp:class:`OGRFeatureDefn`.  This is an object, associated with the layer,
containing the definitions of all the fields.  We loop over all the fields,
and fetch and report the attributes based on their type.

.. tabs::

   .. code-tab:: c++

      for( auto&& oField: *poFeature )
      {
          if( oField.IsUnset() )
          {
              printf("(unset),");
              continue;
          }
          if( oField.IsNull() )
          {
              printf("(null),");
              continue;
          }
          switch( oField.GetType() )
          {
              case OFTInteger:
                  printf( "%d,", oField.GetInteger() );
                  break;
              case OFTInteger64:
                  printf( CPL_FRMT_GIB ",", oField.GetInteger64() );
                  break;
              case OFTReal:
                  printf( "%.3f,", oField.GetDouble() );
                  break;
              case OFTString:
                  // GetString() returns a C string
                  printf( "%s,", oField.GetString() );
                  break;
              default:
                  // Note: we use GetAsString() and not GetString(), since
                  // the later assumes the field type to be OFTString while the
                  // former will do a conversion from the original type to string.
                  printf( "%s,", oField.GetAsString() );
                  break;
          }
      }

   .. code-tab:: c

      OGRFeatureDefnH hFDefn = OGR_L_GetLayerDefn(hLayer);
      int iField;
   
      for( iField = 0; iField < OGR_FD_GetFieldCount(hFDefn); iField++ )
      {
          OGRFieldDefnH hFieldDefn = OGR_FD_GetFieldDefn( hFDefn, iField );
   
          if( !OGR_F_IsFieldSet(hFeature, iField) )
          {
              printf("(unset),");
              continue;
          }
          if( OGR_F_IsFieldNull(hFeature, iField) )
          {
              printf("(null),");
              continue;
          }

          switch( OGR_Fld_GetType(hFieldDefn) )
          {
              case OFTInteger:
                  printf( "%d,", OGR_F_GetFieldAsInteger( hFeature, iField ) );
                  break;
              case OFTInteger64:
                  printf( CPL_FRMT_GIB ",", OGR_F_GetFieldAsInteger64( hFeature, iField ) );
                  break;
              case OFTReal:
                  printf( "%.3f,", OGR_F_GetFieldAsDouble( hFeature, iField) );
                  break;
              case OFTString:
                  printf( "%s,", OGR_F_GetFieldAsString( hFeature, iField) );
                  break;
              default:
                  printf( "%s,", OGR_F_GetFieldAsString( hFeature, iField) );
                  break;
          }
      }

   .. code-tab:: python

      feat_defn = lyr.GetLayerDefn()
      for i in range(feat_defn.GetFieldCount()):
          field_defn = feat_defn.GetFieldDefn(i)

          # Tests below can be simplified with just :
          # print feat.GetField(i)
          if (
              field_defn.GetType() == ogr.OFTInteger
              or field_defn.GetType() == ogr.OFTInteger64
          ):
              print("%d" % feat.GetFieldAsInteger64(i))
          elif field_defn.GetType() == ogr.OFTReal:
              print("%.3f" % feat.GetFieldAsDouble(i))
          elif field_defn.GetType() == ogr.OFTString:
              print("%s" % feat.GetFieldAsString(i))
          else:
              print("%s" % feat.GetFieldAsString(i))


There are a few more field types than those explicitly handled above, but
a reasonable representation of them can be fetched with the
:cpp:func:`OGRFeature::GetFieldAsString` method.  In fact we could shorten the above
by using GetFieldAsString() for all the types.

Next we want to extract the geometry from the feature, and write out the point
geometry x and y.   Geometries are returned as a generic :cpp:class:`OGRGeometry` pointer.
We then determine the specific geometry type, and if it is a point, we
cast it to point and operate on it.  If it is something else we write
placeholders.

.. tabs::

   .. code-tab:: c++

      OGRGeometry *poGeometry;

      poGeometry = poFeature->GetGeometryRef();
      if( poGeometry != NULL
              && wkbFlatten(poGeometry->getGeometryType()) == wkbPoint )
      {
      #if GDAL_VERSION_NUM >= GDAL_COMPUTE_VERSION(2,3,0)
          OGRPoint *poPoint = poGeometry->toPoint();
      #else
          OGRPoint *poPoint = (OGRPoint *) poGeometry;
      #endif

          printf( "%.3f,%3.f\n", poPoint->getX(), poPoint->getY() );
      }
      else
      {
          printf( "no point geometry\n" );
      }

   .. code-tab:: c

      OGRGeometryH hGeometry;

      hGeometry = OGR_F_GetGeometryRef(hFeature);
      if( hGeometry != NULL
              && wkbFlatten(OGR_G_GetGeometryType(hGeometry)) == wkbPoint )
      {
          printf( "%.3f,%3.f\n", OGR_G_GetX(hGeometry, 0), OGR_G_GetY(hGeometry, 0) );
      }
      else
      {
          printf( "no point geometry\n" );
      }

   .. code-tab:: python

      geom = feat.GetGeometryRef()
      if geom is not None and geom.GetGeometryType() == ogr.wkbPoint:
          print("%.3f, %.3f" % (geom.GetX(), geom.GetY()))
      else:
          print("no point geometry")

The :cpp:func:`wkbFlatten` macro is used above to convert the type for a wkbPoint25D
(a point with a z coordinate) into the base 2D geometry type code (wkbPoint).
For each 2D geometry type there is a corresponding 2.5D type code.  The 2D
and 2.5D geometry cases are handled by the same C++ class, so our code will
handle 2D or 3D cases properly.

Several geometry fields can be associated to a feature.

.. tabs::

   .. code-tab:: c++

      OGRGeometry *poGeometry;
      int iGeomField;
      int nGeomFieldCount;

      nGeomFieldCount = poFeature->GetGeomFieldCount();
      for(iGeomField = 0; iGeomField < nGeomFieldCount; iGeomField ++ )
      {
          poGeometry = poFeature->GetGeomFieldRef(iGeomField);
          if( poGeometry != NULL
                  && wkbFlatten(poGeometry->getGeometryType()) == wkbPoint )
          {
      #if GDAL_VERSION_NUM >= GDAL_COMPUTE_VERSION(2,3,0)
              OGRPoint *poPoint = poGeometry->toPoint();
      #else
              OGRPoint *poPoint = (OGRPoint *) poGeometry;
      #endif
     
              printf( "%.3f,%3.f\n", poPoint->getX(), poPoint->getY() );
          }
          else
          {
              printf( "no point geometry\n" );
          }
      }

   .. code-tab:: c

      OGRGeometryH hGeometry;
      int iGeomField;
      int nGeomFieldCount;

      nGeomFieldCount = OGR_F_GetGeomFieldCount(hFeature);
      for(iGeomField = 0; iGeomField < nGeomFieldCount; iGeomField ++ )
      {
          hGeometry = OGR_F_GetGeomFieldRef(hFeature, iGeomField);
          if( hGeometry != NULL
                  && wkbFlatten(OGR_G_GetGeometryType(hGeometry)) == wkbPoint )
          {
              printf( "%.3f,%3.f\n", OGR_G_GetX(hGeometry, 0),
                      OGR_G_GetY(hGeometry, 0) );
          }
          else
          {
              printf( "no point geometry\n" );
          }
      }

   .. code-tab:: python

      nGeomFieldCount = feat.GetGeomFieldCount()
      for iGeomField in range(nGeomFieldCount):
          geom = feat.GetGeomFieldRef(iGeomField)
          if geom is not None and geom.GetGeometryType() == ogr.wkbPoint:
              print("%.3f, %.3f" % ( geom.GetX(), geom.GetY() ))
          else:
              print("no point geometry\n")

Note that :cpp:func:`OGRFeature::GetGeometryRef` and :cpp:func:`OGRFeature::GetGeomFieldRef`
return a pointer to
the internal geometry owned by the OGRFeature.  There we don't actually
delete the return geometry.

The OGRLayer returned by :cpp:func:`GDALDataset::GetLayerByName` is also a reference
to an internal layer owned by the GDALDataset so we don't need to delete
it.  But we do need to delete the datasource in order to close the input file.
Once again we do this with a custom delete method to avoid special win32
heap issues.

.. tabs::

   .. code-tab:: c++

      GDALClose( poDS );
      }

   .. code-tab:: c

      GDALClose( poDS );
      }

   .. code-tab:: python

      ds.Close()

All together our program looks like this.

.. tabs::

   .. code-tab:: c++

      #include "ogrsf_frmts.h"

      int main()

      {
          GDALAllRegister();

          GDALDatasetUniquePtr poDS(GDALDataset::Open( "point.shp", GDAL_OF_VECTOR));
          if( poDS == nullptr )
          {
              printf( "Open failed.\n" );
              exit( 1 );
          }

          for( const OGRLayer* poLayer: poDS->GetLayers() )
          {
              for( const auto& poFeature: *poLayer )
              {
                  for( const auto& oField: *poFeature )
                  {
                      if( oField.IsUnset() )
                      {
                          printf("(unset),");
                          continue;
                      }
                      if( oField.IsNull() )
                      {
                          printf("(null),");
                          continue;
                      }
                      switch( oField.GetType() )
                      {
                          case OFTInteger:
                              printf( "%d,", oField.GetInteger() );
                              break;
                          case OFTInteger64:
                              printf( CPL_FRMT_GIB ",", oField.GetInteger64() );
                              break;
                          case OFTReal:
                              printf( "%.3f,", oField.GetDouble() );
                              break;
                          case OFTString:
                              // GetString() returns a C string
                              printf( "%s,", oField.GetString() );
                              break;
                          default:
                              // Note: we use GetAsString() and not GetString(), since
                              // the later assumes the field type to be OFTString while the
                              // former will do a conversion from the original type to string.
                              printf( "%s,", oField.GetAsString() );
                              break;
                      }
                  }

                  const OGRGeometry *poGeometry = poFeature->GetGeometryRef();
                  if( poGeometry != nullptr
                          && wkbFlatten(poGeometry->getGeometryType()) == wkbPoint )
                  {
                      const OGRPoint *poPoint = poGeometry->toPoint();

                      printf( "%.3f,%3.f\n", poPoint->getX(), poPoint->getY() );
                  }
                  else
                  {
                      printf( "no point geometry\n" );
                  }
              }
          }
          return 0;
      }

   .. code-tab:: c

      #include "gdal.h"

      int main()

      {
          GDALAllRegister();

          GDALDatasetH hDS;
          OGRLayerH hLayer;
          OGRFeatureH hFeature;
          OGRFeatureDefnH hFDefn;

          hDS = GDALOpenEx( "point.shp", GDAL_OF_VECTOR, NULL, NULL, NULL );
          if( hDS == NULL )
          {
              printf( "Open failed.\n" );
              exit( 1 );
          }

          hLayer = GDALDatasetGetLayerByName( hDS, "point" );
          hFDefn = OGR_L_GetLayerDefn(hLayer);

          OGR_L_ResetReading(hLayer);
          while( (hFeature = OGR_L_GetNextFeature(hLayer)) != NULL )
          {
              int iField;
              OGRGeometryH hGeometry;

              for( iField = 0; iField < OGR_FD_GetFieldCount(hFDefn); iField++ )
              {
                  OGRFieldDefnH hFieldDefn = OGR_FD_GetFieldDefn( hFDefn, iField );

                  if( !OGR_F_IsFieldSet(hFeature, iField) )
                  {
                      printf("(unset),");
                      continue;
                  }
                  if( OGR_F_IsFieldNull(hFeature, iField) )
                  {
                      printf("(null),");
                      continue;
                  }

                  switch( OGR_Fld_GetType(hFieldDefn) )
                  {
                      case OFTInteger:
                          printf( "%d,", OGR_F_GetFieldAsInteger( hFeature, iField ) );
                          break;
                      case OFTInteger64:
                          printf( CPL_FRMT_GIB ",", OGR_F_GetFieldAsInteger64( hFeature, iField ) );
                          break;
                      case OFTReal:
                          printf( "%.3f,", OGR_F_GetFieldAsDouble( hFeature, iField) );
                          break;
                      case OFTString:
                          printf( "%s,", OGR_F_GetFieldAsString( hFeature, iField) );
                          break;
                      default:
                          printf( "%s,", OGR_F_GetFieldAsString( hFeature, iField) );
                          break;
                  }
              }

              hGeometry = OGR_F_GetGeometryRef(hFeature);
              if( hGeometry != NULL
                  && wkbFlatten(OGR_G_GetGeometryType(hGeometry)) == wkbPoint )
              {
                  printf( "%.3f,%3.f\n", OGR_G_GetX(hGeometry, 0), OGR_G_GetY(hGeometry, 0) );
              }
              else
              {
                  printf( "no point geometry\n" );
              }

              OGR_F_Destroy( hFeature );
          }

          GDALClose( hDS );
      }

   .. tab:: Python

      .. literalinclude :: code/vector_api_tut.py
         :language: python


.. _vector_api_tut_arrow_stream:

Reading From OGR using the Arrow C Stream data interface
--------------------------------------------------------

.. versionadded:: 3.6

Instead of retrieving features one at a time, it is also possible to retrieve
them by batches, with a column-oriented memory layout, using the
:cpp:func:`OGRLayer::GetArrowStream` method. Note that this method is more
difficult to use than the traditional :cpp:func:`OGRLayer::GetNextFeature` approach,
and is only advised when compatibility with the
`Apache Arrow C Stream interface <https://arrow.apache.org/docs/format/CStreamInterface.html>`_
is needed, or when column-oriented consumption of layers is required.

Pending using an helper library, consumption of the Arrow C Stream interface
requires reading of the following documents:

- `Arrow C Stream interface <https://arrow.apache.org/docs/format/CStreamInterface.html>`_
- `Arrow C data interface <https://arrow.apache.org/docs/format/CDataInterface.html>`_
- `Arrow Columnar Format <https://arrow.apache.org/docs/format/Columnar.html>`_.

The Arrow C Stream interface interface consists of a set of C structures, ArrowArrayStream, that provides
two main callbacks to get:

- a ArrowSchema with the get_schema() callback. A ArrowSchema describes a set of
  field descriptions (name, type, metadata). All OGR data types have a corresponding
  Arrow data type.

- a sequence of ArrowArray with the get_next() callback. A ArrowArray captures
  a set of values for a specific column/field in a subset of features.
  This is the equivalent of a
  `Series <https://arrow.apache.org/docs/python/pandas.html#series>`_ in a Pandas DataFrame.
  This is a potentially hierarchical structure that can aggregate
  sub arrays, and in OGR usage, the main array will be a StructArray which is
  the collection of OGR attribute and geometry fields.
  The layout of buffers and children arrays per data type is detailed in the
  `Arrow Columnar Format <https://arrow.apache.org/docs/format/Columnar.html>`_.

If a layer consists of 4 features with 2 fields (one of integer type, one of
floating-point type), the representation as a ArrowArray is *conceptually* the
following one:

.. code-block:: c

    array.children[0].buffers[1] = { 1, 2, 3, 4 };
    array.children[1].buffers[1] = { 1.2, 2.3, 3.4, 4.5 };

The content of a whole layer can be seen as a sequence of record batches, each
record batches being an ArrowArray of a subset of features. Instead of iterating
over individual features, one iterates over a batch of several features at
once.

The ArrowArrayStream, ArrowSchema, ArrowArray structures are defined in a
ogr_recordbatch.h public header file, directly derived from
https://github.com/apache/arrow/blob/main/cpp/src/arrow/c/abi.h
to get API/ABI compatibility with Apache Arrow C++. This header file must be
explicitly included when the related array batch API is used.

The GetArrowStream() method has the following signature:

  .. code-block:: cpp

        virtual bool OGRLayer::GetArrowStream(struct ArrowArrayStream* out_stream,
                                              CSLConstList papszOptions = nullptr);

It is also available in the C API as :cpp:func:`OGR_L_GetArrowStream`.

out_stream is a pointer to a ArrowArrayStream structure, that can be in a uninitialized
state (the method will ignore any initial content).

On successful return, and when the stream interfaces is no longer needed, it must must
be freed with out_stream->release(out_stream).

There are extra precautions to take into account in a OGR context. Unless
otherwise specified by a particular driver implementation, the ArrowArrayStream
structure, and the ArrowSchema or ArrowArray objects its callbacks have returned,
should no longer be used (except for potentially being released) after the
OGRLayer from which it was initialized has been destroyed (typically at dataset
closing). Furthermore, unless otherwise specified by a particular driver
implementation, only one ArrowArrayStream can be active at a time on
a given layer (that is the last active one must be explicitly released before
a next one is asked). Changing filter state, ignored columns, modifying the schema
or using ResetReading()/GetNextFeature() while using a ArrowArrayStream is
strongly discouraged and may lead to unexpected results. As a rule of thumb,
no OGRLayer methods that affect the state of a layer should be called on a
layer, while an ArrowArrayStream on it is active.

The papszOptions that may be provided is a NULL terminated list of key=value
strings, that may be driver specific.

OGRLayer has a base implementation of GetArrowStream() that is such:

- The get_schema() callback returns a schema whose top-level object returned is
  of type Struct, and whose children consist of the FID column, all OGR attribute
  fields and geometry fields to Arrow fields.
  The FID column may be omitted by providing the INCLUDE_FID=NO option.

  When get_schema() returns 0, and the schema is no longer needed, it must
  be released with the following procedure, to take into account that it might
  have been released by other code, as documented in the Arrow C data
  interface:

  .. code-block:: c

          if( out_schema->release )
              out_schema->release(out_schema)


- The get_next() callback retrieve the next record batch over the layer.

  out_array is a pointer to a ArrowArray structure, that can be in a uninitialized
  state (the method will ignore any initial content).

  The default implementation uses GetNextFeature() internally to retrieve batches
  of up to 65,536 features (configurable with the MAX_FEATURES_IN_BATCH=num option).
  The starting address of buffers allocated by the
  default implementation is aligned on 64-byte boundaries.

  The default implementation outputs geometries as WKB in a binary field,
  whose corresponding entry in the schema is marked with the metadata item
  ``ARROW:extension:name`` set to ``ogc.wkb``. Specialized implementations may output
  by default other formats (particularly the Arrow driver that can return geometries
  encoded according to the GeoArrow specification (using a list of coordinates).
  The GEOMETRY_ENCODING=WKB option can be passed to force the use of WKB (through
  the default implementation)

  The method may take into account ignored fields set with SetIgnoredFields() (the
  default implementation does), and should take into account filters set with
  SetSpatialFilter() and SetAttributeFilter(). Note however that specialized implementations
  may fallback to the default (slower) implementation when filters are set.

  Mixing calls to GetNextFeature() and get_next() is not recommended, as
  the behavior will be unspecified (but it should not crash).

  When get_next() returns 0, and the array is no longer needed, it must
  be released with the following procedure, to take into account that it might
  have been released by other code, as documented in the Arrow C data
  interface:

  .. code-block:: c

          if( out_array->release )
              out_array->release(out_array)

Drivers that have a specialized implementation advertise the
new OLCFastGetArrowStream layer capability.

Using directly (as a producer or a consumer) a ArrowArray is admittedly not
trivial, and requires good intimacy with the Arrow C data interface and columnar
array specifications, to know, in which buffer of an array, data is to be read,
which data type void* buffers should be cast to, how to use buffers that contain
null/not_null information, how to use offset buffers for data types of type List, etc.
The study of the gdal_array._RecordBatchAsNumpy() method of the SWIG Python
bindings (:source_file:`swig/include/gdal_array.i`)
can give a good hint of how to use an ArrowArray object, in conjunction
with the associated ArrowSchema.

The below example illustrates how to read the content of a layer that consists
of a integer field and a geometry field:


.. code-block:: c++

    #include "gdal_priv.h"
    #include "ogr_api.h"
    #include "ogrsf_frmts.h"
    #include "ogr_recordbatch.h"
    #include <cassert>

    int main(int argc, char* argv[])
    {
        GDALAllRegister();
        GDALDataset* poDS = GDALDataset::Open(argv[1]);
        if( poDS == nullptr )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Open() failed\n");
            exit(1);
        }
        OGRLayer* poLayer = poDS->GetLayer(0);
        OGRLayerH hLayer = OGRLayer::ToHandle(poLayer);

        // Get the Arrow stream
        struct ArrowArrayStream stream;
        if( !OGR_L_GetArrowStream(hLayer, &stream, nullptr))
        {
            CPLError(CE_Failure, CPLE_AppDefined, "OGR_L_GetArrowStream() failed\n");
            delete poDS;
            exit(1);
        }

        // Get the schema
        struct ArrowSchema schema;
        if( stream.get_schema(&stream, &schema) != 0 )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "get_schema() failed\n");
            stream.release(&stream);
            delete poDS;
            exit(1);
        }

        // Check that the returned schema consists of one int64 field (for FID),
        // one int32 field and one binary/wkb field
        if( schema.n_children != 3 ||
            strcmp(schema.children[0]->format, "l") != 0 || // int64 -> FID
            strcmp(schema.children[1]->format, "i") != 0 || // int32
            strcmp(schema.children[2]->format, "z") != 0 )  // binary for WKB
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Layer has not the expected schema required by this example.");
            schema.release(&schema);
            stream.release(&stream);
            delete poDS;
            exit(1);
        }
        schema.release(&schema);

        // Iterate over batches
        while( true )
        {
            struct ArrowArray array;
            if( stream.get_next(&stream, &array) != 0 ||
                array.release == nullptr )
            {
                break;
            }

            assert(array.n_children == 3);

            // Cast the array->children[].buffers[] to the appropriate data types
            const auto int_child = array.children[1];
            assert(int_child->n_buffers == 2);
            const uint8_t* int_field_not_null = static_cast<const uint8_t*>(int_child->buffers[0]);
            const int32_t* int_field = static_cast<const int32_t*>(int_child->buffers[1]);

            const auto wkb_child = array.children[2];
            assert(wkb_child->n_buffers == 3);
            const uint8_t* wkb_field_not_null = static_cast<const uint8_t*>(wkb_child->buffers[0]);
            const int32_t* wkb_offset = static_cast<const int32_t*>(wkb_child->buffers[1]);
            const uint8_t* wkb_field = static_cast<const uint8_t*>(wkb_child->buffers[2]);

            // Lambda to check if a field is set for a given feature index
            const auto IsSet = [](const uint8_t* buffer_not_null, int i)
            {
                return buffer_not_null == nullptr || (buffer_not_null[i/8] >> (i%8)) != 0;
            };

            // Iterate through features of a batch
            for( long long i = 0; i < array.length; i++ )
            {
                if( IsSet(int_field_not_null, i) )
                    printf("int_field[%lld] = %d\n", i, int_field[i]);
                else
                    printf("int_field[%lld] = null\n", i);

                if( IsSet(wkb_field_not_null, i) )
                {
                    const void* wkb = wkb_field + wkb_offset[i];
                    const int32_t length = wkb_offset[i+1] - wkb_offset[i];
                    char* wkt = nullptr;
                    OGRGeometry* geom = nullptr;
                    OGRGeometryFactory::createFromWkb(wkb, nullptr, &geom, length);
                    if( geom )
                    {
                        geom->exportToWkt(&wkt);
                    }
                    printf("wkb_field[%lld] = %s\n", i, wkt ? wkt : "invalid geometry");
                    CPLFree(wkt);
                    delete geom;
                }
                else
                {
                    printf("wkb_field[%lld] = null\n", i);
                }
            }

            // Release memory taken by the batch
            array.release(&array);
        }

        // Release stream and dataset
        stream.release(&stream);
        delete poDS;
        return 0;
    }


To write features by batches using an ArrowArray, consult :ref:`vector_api_tut_arrow_write`.

Writing To OGR
--------------

As an example of writing through OGR, we will do roughly the opposite of the
above.  A short program that reads comma separated values from input text
will be written to a point shapefile via OGR.

As usual, we start by registering all the drivers, and then fetch the
Shapefile driver as we will need it to create our output file.

.. tabs::

   .. code-tab:: c++

      #include "ogrsf_frmts.h"

      int main()
      {
          const char *pszDriverName = "ESRI Shapefile";
          GDALDriver *poDriver;

          GDALAllRegister();

          poDriver = GetGDALDriverManager()->GetDriverByName(pszDriverName );
          if( poDriver == NULL )
          {
              printf( "%s driver not available.\n", pszDriverName );
              exit( 1 );
          }

   .. code-tab:: c

      #include "ogr_api.h"

      int main()
      {
          const char *pszDriverName = "ESRI Shapefile";
          GDALDriver *poDriver;

          GDALAllRegister();

          poDriver = (GDALDriver*) GDALGetDriverByName(pszDriverName );
          if( poDriver == NULL )
          {
              printf( "%s driver not available.\n", pszDriverName );
              exit( 1 );
          }

   .. code-tab:: python

      from osgeo import gdal

      gdal.UseExceptions()
      driverName = "ESRI Shapefile"
      drv = gdal.GetDriverByName(driverName)
      if drv is None:
          print("%s driver not available." % driverName)
          sys.exit(1)

Next we create the datasource.  The ESRI Shapefile driver allows us to create
a directory full of shapefiles, or a single shapefile as a datasource.  In
this case we will explicitly create a single file by including the extension
in the name.  Other drivers behave differently.
The second, third, fourth and fifth argument are related to raster dimensions
(in case the driver has raster capabilities). The last argument to
the call is a list of option values, but we will just be using defaults in
this case.  Details of the options supported are also format specific.

.. tabs::

   .. code-tab:: c++

      GDALDataset *poDS;

      poDS = poDriver->Create( "point_out.shp", 0, 0, 0, GDT_Unknown, NULL );
      if( poDS == NULL )
      {
          printf( "Creation of output file failed.\n" );
          exit( 1 );
      }

   .. code-tab:: c

      GDALDatasetH hDS;

      hDS = GDALCreate( hDriver, "point_out.shp", 0, 0, 0, GDT_Unknown, NULL );
      if( hDS == NULL )
      {
          printf( "Creation of output file failed.\n" );
          exit( 1 );
      }

   .. code-tab:: python

      ds = drv.Create("point_out.shp", 0, 0, 0, gdal.GDT_Unknown)

Now we create the output layer.  In this case since the datasource is a
single file, we can only have one layer.  We pass wkbPoint to specify the
type of geometry supported by this layer.  In this case we aren't passing
any coordinate system information or other special layer creation options.

.. tabs::

   .. code-tab:: c++

      OGRLayer *poLayer;

      poLayer = poDS->CreateLayer( "point_out", NULL, wkbPoint, NULL );
      if( poLayer == NULL )
      {
          printf( "Layer creation failed.\n" );
          exit( 1 );
      }

   .. code-tab:: c

      OGRLayerH hLayer;

      hLayer = GDALDatasetCreateLayer( hDS, "point_out", NULL, wkbPoint, NULL );
      if( hLayer == NULL )
      {
          printf( "Layer creation failed.\n" );
          exit( 1 );
      }

   .. code-tab:: python

      lyr = ds.CreateLayer("point_out", None, ogr.wkbPoint)

Now that the layer exists, we need to create any attribute fields that should
appear on the layer.  Fields must be added to the layer before any features
are written.  To create a field we initialize an :cpp:union:`OGRField` object with the
information about the field.  In the case of Shapefiles, the field width and
precision is significant in the creation of the output .dbf file, so we
set it specifically, though generally the defaults are OK.  For this example
we will just have one attribute, a name string associated with the x,y point.

Note that the template OGRField we pass to :cpp:func:`OGRLayer::CreateField` is copied internally.
We retain ownership of the object.

.. tabs::

   .. code-tab:: c++

      OGRFieldDefn oField( "Name", OFTString );

      oField.SetWidth(32);

      if( poLayer->CreateField( &oField ) != OGRERR_NONE )
      {
          printf( "Creating Name field failed.\n" );
          exit( 1 );
      }

   .. code-tab:: c

      OGRFieldDefnH hFieldDefn;

      hFieldDefn = OGR_Fld_Create( "Name", OFTString );

      OGR_Fld_SetWidth( hFieldDefn, 32);

      if( OGR_L_CreateField( hLayer, hFieldDefn, TRUE ) != OGRERR_NONE )
      {
          printf( "Creating Name field failed.\n" );
          exit( 1 );
      }

      OGR_Fld_Destroy(hFieldDefn);

   .. code-tab:: python

      field_defn = ogr.FieldDefn("Name", ogr.OFTString)
      field_defn.SetWidth(32)

      lyr.CreateField(field_defn)

The following snipping loops reading lines of the form "x,y,name" from stdin,
and parsing them.

.. tabs::

   .. code-tab:: c++

      double x, y;
      char szName[33];

      while( !feof(stdin)
             && fscanf( stdin, "%lf,%lf,%32s", &x, &y, szName ) == 3 )
      {

   .. code-tab:: c++

      double x, y;
      char szName[33];

      while( !feof(stdin)
             && fscanf( stdin, "%lf,%lf,%32s", &x, &y, szName ) == 3 )
      {

   .. code-tab:: python

      # Expected format of user input: x y name
      linestring = input()
      linelist = linestring.split()

      while len(linelist) == 3:
        ...

To write a feature to disk, we must create a local OGRFeature, set attributes
and attach geometry before trying to write it to the layer.  It is
imperative that this feature be instantiated from the OGRFeatureDefn
associated with the layer it will be written to.

.. tabs::

   .. code-tab:: c++

          OGRFeature *poFeature;

          poFeature = OGRFeature::CreateFeature( poLayer->GetLayerDefn() );
          poFeature->SetField( "Name", szName );

   .. code-tab:: c

          OGRFeatureH hFeature;

          hFeature = OGR_F_Create( OGR_L_GetLayerDefn( hLayer ) );
          OGR_F_SetFieldString( hFeature, OGR_F_GetFieldIndex(hFeature, "Name"), szName );

   .. code-tab:: python

      feat = ogr.Feature(lyr.GetLayerDefn())
      feat.SetField("Name", name)

We create a local geometry object, and assign its copy (indirectly) to the feature.
The :cpp:func:`OGRFeature::SetGeometryDirectly` differs from :cpp:func:`OGRFeature::SetGeometry`
in that the direct method gives ownership of the geometry to the feature.
This is generally more efficient as it avoids an extra deep object copy
of the geometry.

.. tabs::

   .. code-tab:: c++

      OGRPoint pt;
      pt.setX( x );
      pt.setY( y );

      poFeature->SetGeometry( &pt );

   .. code-tab:: c

      OGRGeometryH hPt;
      hPt = OGR_G_CreateGeometry(wkbPoint);
      OGR_G_SetPoint_2D(hPt, 0, x, y);

      OGR_F_SetGeometry( hFeature, hPt );
      OGR_G_DestroyGeometry(hPt);

   .. code-tab:: python

      pt = ogr.Geometry(ogr.wkbPoint)
      pt.SetPoint_2D(0, x, y)

      feat.SetGeometry(pt)

Now we create a feature in the file.  The :cpp:func:`OGRLayer::CreateFeature` does not
take ownership of our feature so we clean it up when done with it.

.. tabs::

   .. code-tab:: c++

          if( poLayer->CreateFeature( poFeature ) != OGRERR_NONE )
          {
              printf( "Failed to create feature in shapefile.\n" );
             exit( 1 );
          }

          OGRFeature::DestroyFeature( poFeature );
     }

   .. code-tab:: c

          if( OGR_L_CreateFeature( hLayer, hFeature ) != OGRERR_NONE )
          {
              printf( "Failed to create feature in shapefile.\n" );
             exit( 1 );
          }

          OGR_F_Destroy( hFeature );
     }

   .. code-tab:: python

      lyr.CreateFeature(feat)


Finally we need to close down the datasource in order to ensure headers
are written out in an orderly way and all resources are recovered.

.. tabs::

   .. code-tab:: c++

        GDALClose( poDS );
      }

   .. code-tab:: c

        GDALClose( poDS );
      }

   .. code-tab:: python

      ds.Close()

The same program all in one block looks like this:

.. tabs::

   .. code-tab:: c++

      #include "ogrsf_frmts.h"

      int main()
      {
          const char *pszDriverName = "ESRI Shapefile";
          GDALDriver *poDriver;

          GDALAllRegister();

          poDriver = GetGDALDriverManager()->GetDriverByName(pszDriverName );
          if( poDriver == NULL )
          {
              printf( "%s driver not available.\n", pszDriverName );
              exit( 1 );
          }

          GDALDataset *poDS;

          poDS = poDriver->Create( "point_out.shp", 0, 0, 0, GDT_Unknown, NULL );
          if( poDS == NULL )
          {
              printf( "Creation of output file failed.\n" );
              exit( 1 );
          }

          OGRLayer *poLayer;

          poLayer = poDS->CreateLayer( "point_out", NULL, wkbPoint, NULL );
          if( poLayer == NULL )
          {
              printf( "Layer creation failed.\n" );
              exit( 1 );
          }

          OGRFieldDefn oField( "Name", OFTString );

          oField.SetWidth(32);

          if( poLayer->CreateField( &oField ) != OGRERR_NONE )
          {
              printf( "Creating Name field failed.\n" );
              exit( 1 );
          }

          double x, y;
          char szName[33];

          while( !feof(stdin)
              && fscanf( stdin, "%lf,%lf,%32s", &x, &y, szName ) == 3 )
          {
              OGRFeature *poFeature;

             poFeature = OGRFeature::CreateFeature( poLayer->GetLayerDefn() );
             poFeature->SetField( "Name", szName );

              OGRPoint pt;

              pt.setX( x );
              pt.setY( y );

              poFeature->SetGeometry( &pt );

              if( poLayer->CreateFeature( poFeature ) != OGRERR_NONE )
              {
                  printf( "Failed to create feature in shapefile.\n" );
                  exit( 1 );
              }

              OGRFeature::DestroyFeature( poFeature );
          }

          GDALClose( poDS );
      }

   .. code-tab:: c

      #include "gdal.h"

      int main()
      {
          const char *pszDriverName = "ESRI Shapefile";
          GDALDriverH hDriver;
          GDALDatasetH hDS;
          OGRLayerH hLayer;
          OGRFieldDefnH hFieldDefn;
          double x, y;
          char szName[33];

          GDALAllRegister();

          hDriver = GDALGetDriverByName( pszDriverName );
          if( hDriver == NULL )
          {
              printf( "%s driver not available.\n", pszDriverName );
              exit( 1 );
          }

          hDS = GDALCreate( hDriver, "point_out.shp", 0, 0, 0, GDT_Unknown, NULL );
          if( hDS == NULL )
          {
              printf( "Creation of output file failed.\n" );
              exit( 1 );
          }

          hLayer = GDALDatasetCreateLayer( hDS, "point_out", NULL, wkbPoint, NULL );
          if( hLayer == NULL )
          {
              printf( "Layer creation failed.\n" );
              exit( 1 );
          }

          hFieldDefn = OGR_Fld_Create( "Name", OFTString );

          OGR_Fld_SetWidth( hFieldDefn, 32);

          if( OGR_L_CreateField( hLayer, hFieldDefn, TRUE ) != OGRERR_NONE )
          {
              printf( "Creating Name field failed.\n" );
              exit( 1 );
          }

          OGR_Fld_Destroy(hFieldDefn);

          while( !feof(stdin)
              && fscanf( stdin, "%lf,%lf,%32s", &x, &y, szName ) == 3 )
          {
              OGRFeatureH hFeature;
              OGRGeometryH hPt;

              hFeature = OGR_F_Create( OGR_L_GetLayerDefn( hLayer ) );
              OGR_F_SetFieldString( hFeature, OGR_F_GetFieldIndex(hFeature, "Name"), szName );

              hPt = OGR_G_CreateGeometry(wkbPoint);
              OGR_G_SetPoint_2D(hPt, 0, x, y);

              OGR_F_SetGeometry( hFeature, hPt );
              OGR_G_DestroyGeometry(hPt);

              if( OGR_L_CreateFeature( hLayer, hFeature ) != OGRERR_NONE )
              {
              printf( "Failed to create feature in shapefile.\n" );
              exit( 1 );
              }

              OGR_F_Destroy( hFeature );
          }

          GDALClose( hDS );
      }

   .. tab:: Python

      .. literalinclude :: code/vector_api_tut2.py
         :language: python


Several geometry fields can be associated to a feature. This capability
is just available for a few file formats, such as PostGIS.

To create such datasources, geometry fields must be first created.
Spatial reference system objects can be associated to each geometry field.

.. tabs::

   .. code-tab:: c++

      OGRGeomFieldDefn oPointField( "PointField", wkbPoint );
      OGRSpatialReference* poSRS = new OGRSpatialReference();
      poSRS->importFromEPSG(4326);
      oPointField.SetSpatialRef(poSRS);
      poSRS->Release();

      if( poLayer->CreateGeomField( &oPointField ) != OGRERR_NONE )
      {
          printf( "Creating field PointField failed.\n" );
          exit( 1 );
      }

      OGRGeomFieldDefn oFieldPoint2( "PointField2", wkbPoint );
      poSRS = new OGRSpatialReference();
      poSRS->importFromEPSG(32631);
      oPointField2.SetSpatialRef(poSRS);
      poSRS->Release();

      if( poLayer->CreateGeomField( &oPointField2 ) != OGRERR_NONE )
      {
          printf( "Creating field PointField2 failed.\n" );
          exit( 1 );
      }

   .. code-tab:: c

      OGRGeomFieldDefnH hPointField;
      OGRGeomFieldDefnH hPointField2;
      OGRSpatialReferenceH hSRS;

      hPointField = OGR_GFld_Create( "PointField", wkbPoint );
      hSRS = OSRNewSpatialReference( NULL );
      OSRImportFromEPSG(hSRS, 4326);
      OGR_GFld_SetSpatialRef(hPointField, hSRS);
      OSRRelease(hSRS);

      if( OGR_L_CreateGeomField( hLayer, hPointField ) != OGRERR_NONE )
      {
          printf( "Creating field PointField failed.\n" );
          exit( 1 );
      }

      OGR_GFld_Destroy( hPointField );

      hPointField2 = OGR_GFld_Create( "PointField2", wkbPoint );
      OSRImportFromEPSG(hSRS, 32631);
      OGR_GFld_SetSpatialRef(hPointField2, hSRS);
      OSRRelease(hSRS);

      if( OGR_L_CreateGeomField( hLayer, hPointField2 ) != OGRERR_NONE )
      {
          printf( "Creating field PointField2 failed.\n" );
          exit( 1 );
      }

      OGR_GFld_Destroy( hPointField2 );


To write a feature to disk, we must create a local OGRFeature, set attributes
and attach geometries before trying to write it to the layer.  It is
imperative that this feature be instantiated from the OGRFeatureDefn
associated with the layer it will be written to.

.. tabs::

   .. code-tab:: c++

          OGRFeature *poFeature;
          OGRGeometry *poGeometry;
          char* pszWKT;

          poFeature = OGRFeature::CreateFeature( poLayer->GetLayerDefn() );

          pszWKT = (char*) "POINT (2 49)";
          OGRGeometryFactory::createFromWkt( &pszWKT, NULL, &poGeometry );
          poFeature->SetGeomFieldDirectly( "PointField", poGeometry );

          pszWKT = (char*) "POINT (500000 4500000)";
          OGRGeometryFactory::createFromWkt( &pszWKT, NULL, &poGeometry );
          poFeature->SetGeomFieldDirectly( "PointField2", poGeometry );

          if( poLayer->CreateFeature( poFeature ) != OGRERR_NONE )
          {
              printf( "Failed to create feature.\n" );
              exit( 1 );
          }

          OGRFeature::DestroyFeature( poFeature );

   .. code-tab:: c

          OGRFeatureH hFeature;
          OGRGeometryH hGeometry;
          char* pszWKT;

          poFeature = OGR_F_Create( OGR_L_GetLayerDefn(hLayer) );

          pszWKT = (char*) "POINT (2 49)";
          OGR_G_CreateFromWkt( &pszWKT, NULL, &hGeometry );
          OGR_F_SetGeomFieldDirectly( hFeature,
              OGR_F_GetGeomFieldIndex(hFeature, "PointField"), hGeometry );

          pszWKT = (char*) "POINT (500000 4500000)";
          OGR_G_CreateFromWkt( &pszWKT, NULL, &hGeometry );
          OGR_F_SetGeomFieldDirectly( hFeature,
              OGR_F_GetGeomFieldIndex(hFeature, "PointField2"), hGeometry );

          if( OGR_L_CreateFeature( hFeature ) != OGRERR_NONE )
          {
              printf( "Failed to create feature.\n" );
              exit( 1 );
          }

          OGR_F_Destroy( hFeature );

   .. code-tab:: python

          feat = ogr.Feature( lyr.GetLayerDefn() )

          feat.SetGeomFieldDirectly( "PointField",
              ogr.CreateGeometryFromWkt( "POINT (2 49)" ) )
          feat.SetGeomFieldDirectly( "PointField2",
              ogr.CreateGeometryFromWkt( "POINT (500000 4500000)" ) )

          if lyr.CreateFeature( feat ) != 0:
              print( "Failed to create feature.\n" );
              sys.exit( 1 );

.. _vector_api_tut_arrow_write:

Writing to OGR using the Arrow C Data interface
-----------------------------------------------

.. versionadded:: 3.8

Instead of writing features one at a time, it is also possible to write
them by batches, with a column-oriented memory layout, using the
:cpp:func:`OGRLayer::WriteArrowBatch` method. Note that this method is more
difficult to use than the traditional :cpp:func:`OGRLayer::CreateFeature` approach,
and is only advised when compatibility with the
`Apache Arrow C Data interface <https://arrow.apache.org/docs/format/CDataInterface.html>`_
is needed, or when column-oriented writing of layers is required.

Pending using an helper library, generation of the Arrow C Data interface
requires reading of the following documents:

- `Arrow C data interface <https://arrow.apache.org/docs/format/CDataInterface.html>`_
- `Arrow Columnar Format <https://arrow.apache.org/docs/format/Columnar.html>`_.

Consult :ref:`vector_api_tut_arrow_stream` for introduction to the ArrowSchema and ArrowArray
basic types involved for batch writing.

The WriteArrowBatch() method has the following signature:

  .. code-block:: cpp

        /** Writes a batch of rows from an ArrowArray.
         *
         * @param schema Schema of array
         * @param array Array of type struct. It may be released (array->release==NULL)
         *              after calling this method.
         * @param papszOptions Options. Null terminated list, or nullptr.
         * @return true in case of success
         */
        virtual bool OGRLayer::WriteArrowBatch(const struct ArrowSchema *schema,
                                               struct ArrowArray *array,
                                               CSLConstList papszOptions = nullptr);

It is also available in the C API as :cpp:func:`OGR_L_WriteArrowBatch`.

This is semantically close to calling :cpp:func:`OGRLayer::CreateFeature()`
with multiple features at once.

The ArrowArray must be of type struct (format=+s), and its children generally
map to a OGR attribute or geometry field (unless they are struct themselves).

Method :cpp:func:`OGRLayer::IsArrowSchemaSupported` can be called to determine
if the schema will be supported by WriteArrowBatch().

OGR fields for the corresponding children arrays must exist and be of a
compatible type. For attribute fields, they should be created with
:cpp:func:`OGRLayer::CreateFieldFromArrowSchema`.

Arrays for geometry columns should be of binary or large binary type and
contain WKB geometry.

Note that the passed array may be set to a released state
(array->release==NULL) after this call (not by the base implementation,
but in specialized ones such as Parquet or Arrow for example)

Supported options of the base implementation are:

- FID=name. Name of the FID column in the array. If not provided,
  GetFIDColumn() is used to determine it. The special name
  OGRLayer::DEFAULT_ARROW_FID_NAME is also recognized if neither FID nor
  GetFIDColumn() are set.
  The corresponding ArrowArray must be of type int32 (i) or int64 (l).
  On input, values of the FID column are used to create the feature.
  On output, the values of the FID column may be set with the FID of the
  created feature (if the array is not released).

- GEOMETRY_NAME=name. Name of the geometry column. If not provided,
  GetGeometryColumn() is used. The special name
  OGRLayer::DEFAULT_ARROW_GEOMETRY_NAME is also recognized if neither
  GEOMETRY_NAME nor GetGeometryColumn() are set.
  Geometry columns are also identified if they have
  ARROW:extension:name=ogc.wkb as a field metadata.
  The corresponding ArrowArray must be of type binary (w) or large
  binary (W).

Drivers that have a specialized implementation (such as :ref:`vector.parquet`
and :ref:`vector.arrow`) advertise the OLCFastWriteArrowBatch layer capability.

The following example in Python demonstrates how to copy a layer from one format to
another one (assuming it has at most a single geometry column):

.. code-block:: python

    def copy_layer(src_lyr, out_filename, out_format, lcos = {}):
        stream = src_lyr.GetArrowStream()
        schema = stream.GetSchema()

        # If the source layer has a FID column and the output driver supports
        # a FID layer creation option, set it to the source FID column name.
        if src_lyr.GetFIDColumn():
            creationOptions = gdal.GetDriverByName(out_format).GetMetadataItem(
                "DS_LAYER_CREATIONOPTIONLIST"
            )
            if creationOptions and '"FID"' in creationOptions:
                lcos["FID"] = src_lyr.GetFIDColumn()

        with ogr.GetDriverByName(out_format).CreateDataSource(out_filename) as out_ds:
            if src_lyr.GetLayerDefn().GetGeomFieldCount() > 1:
                out_lyr = out_ds.CreateLayer(
                    src_lyr.GetName(), geom_type=ogr.wkbNone, options=lcos
                )
                for i in range(src_lyr.GetLayerDefn().GetGeomFieldCount()):
                    out_lyr.CreateGeomField(src_lyr.GetLayerDefn().GetGeomFieldDefn(i))
            else:
                out_lyr = out_ds.CreateLayer(
                    src_lyr.GetName(),
                    geom_type=src_lyr.GetGeomType(),
                    srs=src_lyr.GetSpatialRef(),
                    options=lcos,
                )

            success, error_msg = out_lyr.IsArrowSchemaSupported(schema)
            assert success, error_msg

            src_geom_field_names = [
                src_lyr.GetLayerDefn().GetGeomFieldDefn(i).GetName()
                for i in range(src_lyr.GetLayerDefn().GetGeomFieldCount())
            ]
            for i in range(schema.GetChildrenCount()):
                # GetArrowStream() may return "OGC_FID" for a unnamed source FID
                # column and "wkb_geometry" for a unnamed source geometry column.
                # Also test GetFIDColumn() and src_geom_field_names if they are
                # named.
                if (
                    schema.GetChild(i).GetName()
                    not in ("OGC_FID", "wkb_geometry", src_lyr.GetFIDColumn())
                    and schema.GetChild(i).GetName() not in src_geom_field_names
                ):
                    out_lyr.CreateFieldFromArrowSchema(schema.GetChild(i))

            write_options = []
            if src_lyr.GetFIDColumn():
                write_options.append("FID=" + src_lyr.GetFIDColumn())
            if (
                src_lyr.GetLayerDefn().GetGeomFieldCount() == 1
                and src_lyr.GetGeometryColumn()
            ):
                write_options.append("GEOMETRY_NAME=" + src_lyr.GetGeometryColumn())

            while True:
                array = stream.GetNextRecordBatch()
                if array is None:
                    break
                out_lyr.WriteArrowBatch(schema, array, write_options)


For the Python bindings, in addition to the above ogr.Layer.IsArrowSchemaSupported(),
ogr.Layer.CreateFieldFromArrowSchema() and ogr.Layer.WriteArrowBatch() methods,
3 similar methods exist using the `PyArrow <https://arrow.apache.org/docs/python/index.html>`__
data types:

.. code-block:: python

    class Layer:

        def IsPyArrowSchemaSupported(self, pa_schema, options=[]):
            """Returns whether the passed pyarrow Schema is supported by the layer, as a tuple (success: bool, errorMsg: str).

        def CreateFieldFromPyArrowSchema(self, pa_schema, options=[]):
            """Create a field from the passed pyarrow Schema."""

        def WritePyArrow(self, pa_batch, options=[]):
            """Write the content of the passed PyArrow batch (either a pyarrow.Table, a pyarrow.RecordBatch or a pyarrow.StructArray) into the layer."""
