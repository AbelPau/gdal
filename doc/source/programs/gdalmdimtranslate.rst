.. _gdalmdimtranslate:

================================================================================
gdalmdimtranslate
================================================================================

.. only:: html

    .. versionadded:: 3.1

    Converts multidimensional data between different formats, and perform subsetting.

.. Index:: gdalmdimtranslate

Synopsis
--------

.. program-output:: gdalmdimtranslate --help-doc

Description
-----------

:program:`gdalmdimtranslate` program converts multidimensional raster between
different formats, and/or can perform selective conversion of specific arrays
and groups, and/or subsetting operations.

The following command line parameters can appear in any order.

.. program:: gdalmdimtranslate

.. include:: options/help_and_help_general.rst

.. include:: options/if.rst

.. option:: -of <format>

    Select the output format. This can be a format that supports multidimensional
    output (such as :ref:`raster.netcdf`, :ref:`vrt_multidimensional`), or a "classic" 2D formats, if only one single 2D array
    results of the other specified conversion operations. When this option is
    not specified, the format is guessed when possible from the extension of the
    destination filename.

.. option:: -co <NAME>=<VALUE>

    Many formats have one or more optional creation options that can be
    used to control particulars about the file created.

    The creation options available vary by format driver, and some
    simple formats have no creation options at all. A list of options
    supported for a format can be listed with the
    :ref:`--format <raster_common_options_format>`
    command line option but the documentation for the format is the
    definitive source of information on driver creation options.
    See :ref:`raster_drivers` format
    specific documentation for legal creation options for each format.

    Array-level creation options may be passed by prefixing them with ``ARRAY:``.
    See :cpp:func:`GDALGroup::CopyFrom` for further details regarding such options.

.. option:: -array <array_spec>

    Instead of converting the whole dataset, select one array, and possibly
    perform operations on it. This option can be specified several times to
    operate on different arrays.

    <array_spec> may be just an array name, potentially using a fully qualified
    syntax (/group/subgroup/array_name). Or it can be a combination of options
    with the syntax:
    name={src_array_name}[,dstname={dst_array_name}][,resample=yes][,transpose=[{axis1},{axis2},...][,view={view_expr}]

    The following options are processed in that order:

    - ``resample=yes`` asks for the array to run through :cpp:func:`GDALMDArray::GetResampled`.

    - [{axis1},{axis2},...] is the argument of  :cpp:func:`GDALMDArray::Transpose`.
       For example, transpose=[1,0] switches the axis order of a 2D array.
       See :example:`transpose`.

    - {view_expr} is the value of the *viewExpr* argument of :cpp:func:`GDALMDArray::GetView`.
      See :example:`reorder`.

    When specifying a view_expr that performs a slicing or subsetting on a dimension, the
    equivalent operation will be applied to the corresponding indexing variable.

.. option:: -arrayoption <NAME>=<VALUE>

    .. versionadded:: 3.9

    Option passed to :cpp:func:`GDALGroup::GetMDArrayNames` to filter reported
    arrays. Such option is format specific. Consult driver documentation.
    This option may be used several times.

.. option:: -group <group_spec>

    Instead of converting the whole dataset, select one group, and possibly
    perform operations on it. This option can be specified several times to
    operate on different groups. If only one group is specified, its content will be
    copied directly to the target root group. If several ones are specified,
    they are copied under the target root group

    <group_spec> may be just a group name, potentially using a fully qualified
    syntax (/group/subgroup/subsubgroup_name). Or it can be a combination of options
    with the syntax:
    name={src_group_name}[,dstname={dst_group_name}][,recursive=no]

.. option:: -subset <subset_spec>

    Performs a subsetting (trimming or slicing) operation along a dimension,
    provided that it is indexed by a 1D variable of numeric or string data type,
    and whose values are monotonically sorted.
    <subset_spec> follows exactly the `OGC WCS 2.0 KVP encoding <https://portal.opengeospatial.org/files/09-147r3>`__
    for subsetting.

    That is dim_name(min_val,max_val) or dim_name(sliced_val)
    The first syntax will subset the dimension dim_name to values in the
    [min_val,max_val] range. The second syntax will slice the dimension dim_name
    to value sliced_val (and this dimension will be removed from the arrays
    that reference to it)

    Using -subset is incompatible with specifying a *view* option in -array.

    See :example:`subset-1`.

.. option:: -scaleaxes <scaleaxes_spec>

    Applies a integral scale factor to one or several dimensions, that is
    extract 1 value every N values (without resampling).

    <scaleaxes_spec> follows exactly the syntax of the KVP encoding of the
    SCALEAXES parameter of
    `OGC WCS 2.0 Scaling Extension <https://portal.opengeospatial.org/files/12-039>`__,
    but limited to integer scale factors.

    That is <dim1_name>(<scale_factor>)[,<dim2_name>(<scale_factor>)]...

    Using -scaleaxes is incompatible with specifying a *view* option in -array.

    See :example:`subsample-1`.

.. option:: -strict

    By default, some failures during the translation are tolerated, such as not
    being able to write group attributes. When setting this option, such
    failures will cause the process to fail.

.. option:: -oo <NAME>=<VALUE>

    .. versionadded:: 3.4

    Source dataset open option (format specific)

.. option:: <src_dataset>

    The source dataset name.

.. option:: <dst_dataset>

    The destination file name.

C API
-----

This utility is also callable from C with :cpp:func:`GDALMultiDimTranslate`.

Examples
--------

.. example::
   :title: Convert a netCDF file to a multidimensional VRT file

   .. code-block:: bash

      gdalmdimtranslate in.nc out.vrt

.. example::
   :title: Extract a 2D slice of a time,Y,X array
   :id: subset-1

   .. code-block:: bash

       gdalmdimtranslate in.nc out.tif -subset 'time("2010-01-01")' -array temperature

.. example::
   :title: Subsample along X and Y axis
   :id: subsample-1

   .. code-block:: bash

       gdalmdimtranslate in.nc out.nc -scaleaxes "X(2),Y(2)"

.. example::
   :title: Reorder the values of an array
   :id: reorder

   Reorder the values of the time,Y,X array along the Y axis from top-to-bottom
   to bottom-to-top (or the reverse)

   .. code-block:: bash

      gdalmdimtranslate in.nc out.nc -array "name=temperature,view=[:,::-1,:]"

.. example::
   :title: Transpose an array that has X,Y,time dimension order to time,Y,X
   :id: transpose


   .. code-block:: bash

       gdalmdimtranslate in.nc out.nc -array "name=temperature,transpose=[2,1,0]"
