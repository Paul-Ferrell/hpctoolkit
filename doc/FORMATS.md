A full HPCToolkit database consists of the following files and directories:

    database/
    |-- FORMATS.md     This file
    |-- metrics/       Taxonomic metric descriptions for analysis presentations
    |   |-- METRICS.yaml.ex  Documentation for the metric taxonomy YAML format
    |   `-- default.yaml     Default metric taxonomy, suitable for most cases
    |-- meta.db        Properties of the measured application execution
    |-- profile.db     Performance measurements arranged by application thread
    |-- cct.db         Performance measurements arranged by calling context
    |-- trace.db       Time-centric execution traces
    `-- src/           Relevant application source files

This file describes the format of the `*.db` files in an HPCToolkit database.
See `metrics/METRICS.yaml.ex` for a description of the format for defining
performance metric taxonomies.

Table of contents:
  - [`meta.db` v4.0](#metadb-version-40)
  - [`profile.db` v4.0](#profiledb-version-40)
  - [`cct.db` v4.0](#cctdb-version-40)
  - [`trace.db` v4.0](#tracedb-version-40)

### Formats legend ###

All of the `*.db` files use custom binary formats comprised of a header and a
series of sub-structures placed throughout the file. Each structure is described
as a table where each row describes a single field:

 Hex | Type | Name | Description
 ---:| ---- | ---- | -----------
`00:`|Ty|`field1`  | Description of the value in `field1`
|    |
`13:`|Ty2|`field2` | Description of the value in `field2`
`15:`|| **END**

 - **Hex** lists the byte-offset from the beginning of the structure to the
   beginning of the field, in hexadecimal. Fields are normally packed with no
   padding in-between, if there may be a gap between fields an empty row is
   inserted into the table for readability.

   If the offset is variable and cannot be determined, a `*` is used instead.

 - **Type** lists the interpretation of the field's bytes, along with
   (implicitly) its size. The standard types are as follows:
   * u`N` is a unsigned integer of `N` bits.
     Multi-byte integers are laid out in little-endian order.
   * f64 is an IEEE 754 double-precision floating-point number, laid out in
     little-endian order (ie. sign byte comes last).
   * `Ty`x`N` is an array of `N` elements of type `Ty`. Unless otherwise noted,
     there is no padding between elements (ie. the stride is equal to the
     total size of `Ty`).
   * `Ty`xN is an array of elements of type `Ty`, however the number of elements
     is not constant. The **Description** of the field indicates the size.
   * `Ty`*(A) (where `Ty` is any other valid type) is a u64, but additionally
     the value is the offset of a structure of type `Ty` (ie. a pointer). Unless
     otherwise noted, this offset is relative to the beginning of the file.
     The (A) suffix indicates the alignment of the pointer value, see
     [Alignment properties](#alignment-properties) for details.
   * char* is a u64, but additionally the value is the offset of the start byte
     of a null-terminated UTF-8 string. Unless otherwise noted, this offset is
     relative to the beginning of the file.

 - **Name** gives a short name to refer to the field.

 - **Description** describes the value of the field. Larger descriptions may be
   listed after the table for readability.

 - The final **END** row lists the total size of the structure. In general this
   is also the offset of the subsequent field or array element.

### Alignment properties ###

Most of the fields within the `*.db` file formats are designed for efficient
access through an mmapped segment, to support this most fields are aligned to a
boundary equivalent to their size (eg. u64 to an 8-byte boundary, u32 to 4-byte,
etc.). Structures are aligned based on the maximum required alignment among
their fields.

Pointers containing aligned values have an additional (A) suffix indicating
the alignment: u8\*(8) is aligned to an 8-byte boundary, u32*(4) is aligned to
a 4-byte boundary, etc. Note that the alignment may not be the same as the
minimum alignment required by the type.

The following fields/structures are not fully aligned in the current format, see
the notes in these sections for recommendations on how to adapt:
 - Performance data arrays in [Profile-Major][PSVB] and
   [Context-Major Sparse Value Block][CSVB], and
 - The array in a [trace line][THsec].

[PSVB]: #profile-major-sparse-value-block
[CSVB]: #context-major-sparse-value-block
[THsec]: #tracedb-trace-headers-section

### A word on compatibility ###

All of the `*.db` file formats are designed to allow readers to implement
*forward compatibility* within a major version, where the minor version of the
`*.db` format is larger ("after") the version the reader was implemented for.
By "forward compatibility", we mean that in general data present in previous
versions can still be read and interpreted, but fields added later will not in
general be available. Note that fields may be added later in regions previously
classified as "gaps" in structures.

One common exception to the above rule are enumerations -- integers with values
that have specific well-defined meanings. New values may be added in later
versions of the formats, it is up to the reader to error or otherwise ignore
data that depends on an unknown enumeration value. No fields will become
inaccessible due to an unknown enumeration value.

If required, it is up to readers to implement *backward compatibility*, where
the version of the `*.db` format is smaller ("before") the version the reader
was implemented for. Usually this works by selectively disabling the access of
fields that were not present in the previous version of the format.

Finally, the major/minor version numbers listed in every `*.db` file follow the
[Semantic Versioning](https://semver.org) scheme: the major version is
incremented whenever a format change would break forward compatibility, and the
minor version if not. The major version of all files in a single database is
always consistent, even if no change occurred for some formats.

* * *

`meta.db` version 4.0
===================================
`meta.db` is a binary file listing various metadata for the database, including:
  - Performance metrics for the metrics measured at application run-time,
  - Calling contexts for metric values listed in sibling `*.db` files, and
  - A human-readable description of the database's contents.

The `meta.db` file starts with the following header:

 Hex | Type | Name       | Description
 ---:| ---- | ---------- | ----------------------------------------------------
`00:`|u8x14|`magic`      | Format identifier, reads `HPCTOOLKITmeta` in ASCII
`0e:`|u8|`majorVersion`  | Major version number, currently 4
`0f:`|u8|`minorVersion`  | Minor version number, currently 0
`10:`|u64|`szGeneral`    | Size of the [General Properties section][GPsec]
`18:`|u8*(8)|`pGeneral`  | Pointer to the [General Properties section][GPsec]
`20:`|u64|`szIdNames`    | Size of the [Identifier Names section][INsec]
`28:`|u8*(8)|`pIdNames`  | Pointer to the [Identifier Names section][INsec]
`30:`|u64|`szMetrics`    | Size of the [Performance Metrics section][PMsec]
`38:`|u8*(8)|`pMetrics`  | Pointer to the [Performance Metrics section][PMsec]
`40:`|u64|`szContext`    | Size of the [Context Tree section][CTsec]
`48:`|u8*(8)|`pContext`  | Pointer to the [Context Tree section][CTsec]
`50:`|u64|`szStrings`    | Size of the Common String Table section
`58:`|u8*(1)|`pStrings`  | Pointer to the Common String Table section
`60:`|u64|`szModules`    | Size of the [Load Modules section][LMsec]
`68:`|u8*(8)|`pModules`  | Pointer to the [Load Modules section][LMsec]
`70:`|u64|`szFiles`      | Size of the [Source Files section][SFsec]
`78:`|u8*(8)|`pFiles`    | Pointer to the [Source Files section][SFsec]
`80:`|u64|`szFunctions`  | Size of the [Functions section][Fnsec]
`88:`|u8*(8)|`pFunctions`| Pointer to the [Functions section][Fnsec]
`90:`|| **END**

[GPsec]: #metadb-general-properties-section
[INsec]: #metadb-hierarchical-identifier-names-section
[PMsec]: #metadb-performance-metrics-section
[CTsec]: #metadb-context-tree-section
[LMsec]: #metadb-load-modules-section
[SFsec]: #metadb-source-files-section
[Fnsec]: #metadb-functions-section

The `meta.db` file ends with an 8-byte footer, reading `_meta.db` in ASCII.

Additional notes:
 - The Common String Table section has no particular interpretation, it is used
   as a section to store strings for the [Load Modules section][LMsec],
   the [Source Files section][SFsec], and the [Functions section][Fnsec].

`meta.db` General Properties section
-----------------------------------------
The General Properties section starts with the following header:

 Hex | Type | Name        | Description
 ---:| ---- | ----------- | ----------------------------------------------------
`00:`|char*|`pTitle`      | Title of the database. May be provided by the user.
`08:`|char*|`pDescription`| Human-readable Markdown description of the database.
`10:`|| **END**

`description` provides information about the measured execution and subsequent
analysis that may be of interest to users. The exact layout and the information
contained may change without warning.

Additional notes:
 - The strings pointed to by `pTitle` and `pDescription` are fully contained
   within the General Properties section, including the terminating NUL byte.


`meta.db` Hierarchical Identifier Names section
-----------------------------------------------
> In future versions of HPCToolkit new functionality may be added that requires
> new values for the the `kind` field of a [Hierarchical Identifier Tuple][HIT].
> The Hierarchical Identifier Names section provides human-readable names for
> all possible values for forward compatibility.

The Hierarchical Identifier Names section starts with the following header:

 Hex | Type | Name          | Description
 ---:| ---- | ------------- | --------------------------------------------------
`00:`|char\*xN*(8)|`ppNames`| Pointer to an array of `nKinds` human-readable names for Id. Names
`08:`|u8|`nKinds`           | Number of names listed in this section
`09:`|| **END**

`ppNames[kind]` is the human-readable name for the Identifier kind `kind`,
where `kind` is part of a [Hierarchical Identifier Tuple][HIT].

[HIT]: #profiledb-hierarchical-identifier-tuple-section

Additional notes:
 - The strings pointed to `ppNames[...]` are fully contained within the
   Hierarchical Identifier Names section, including the terminating NUL.


`meta.db` Performance Metrics section
-------------------------------------
> The Performance Metrics section lists the performance metrics measured at
> application runtime, and the analysis performed by HPCToolkit to generate the
> metric values within `profile.db` and `cct.db`. In summary:
>   - Performance measurements for an application thread are first attributed to
>     contexts, listed in the [Context Tree section](#metadb-context-tree-section).
>     These are the raw metric values.
>   - Propagated metric values are generated for each context by summing values
>     attributed to children contexts, within the measurements for a single
>     application thread. Which children are included in this sum is indicated
>     by the `scope`.
>   - Summary statistic values are generated for each context from the
>     propagated metric values for each application thread, by first applying
>     `formula` to each value and then combining via `combine`. This generates a
>     single statistic value for each context.

The Performance Metrics section starts with the following header:

 Hex | Type | Name         | Description
 ---:| ---- | ------------ | --------------------------------------------------
`00:`|{MD}xN*(8)|`pMetrics`| Pointer to an array of `nMetrics` metric descriptions
`08:`|u32|`nMetrics`   | Number of performance metrics included in this section
`0c:`|u8|`szMetric`    | Size of the {MD} structure, currently 24
`0d:`|u8|`szScope`     | Size of the {PS} structure, currently 24
`0e:`|u8|`szSummary`   | Size of the {SS} structure, currently 16
`0f:`|| **END**

{MD} above refers to the following sub-structure:

 Hex | Type | Name     | Description
 ---:| ---- | -------- | ------------------------------------------------------
`00:`|char*|`pName`    | Canonical name of the raw performance metric
`08:`|u16|`nScopes`    | Number of propagation scopes used for this metric
|    |
`10:`|{PS}xN*(8)|`pScopes`| Pointer to an array of `nScopes` propagation scope descriptions
`18:`|| **END**

{PS} above refers to the following sub-structure:

 Hex | Type | Name        | Description
 ---:| ---- | ----------- | ---------------------------------------------------
`00:`|char*|`pScope`      | Canonical name of the propagation scope
`08:`|u16|`nSummaries`    | Number of summary statistics generated for this scope
`0a:`|u16|`propMetricId`  | Unique identifier for propagated metric values
|    |
`10:`|{SS}xN*(8)|`pSummaries`| Array of `nSummaries` summary statistic descriptions
`18:`|| **END**

{SS} above refers to the following sub-structure:

 Hex | Type | Name      | Description
 ---:| ---- | --------- | -----------------------------------------------------
`00:`|char*|`pFormula`  | Canonical unary function used for summary values
`08:`|u8|`combine`      | Combination n-ary function used for summary values
|    |
`0a:`|u16|`statMetricId`| Unique identifier for summary statistic values
|    |
`10:`|| **END**

> The propagation scope `scope` may be any string, however to aid writing and
> maintaining metric taxonomies (see METRICS.yaml) the names rarely change
> meanings. Each scope indicates the children that are included in the sum for
> propagating metric values, the following scopes are in active use:
>  - "point": No children are included in the sum -- no propagation happens.
>  - "function": Only children not related by a caller-calle relationship are
>    included in the sum -- propagation only happens within a function call.
>    Values for this scope are sometimes called "exclusive cost."
>  - "execution": All children are included in the sum. Values for this scope
>    are often called "inclusive cost."

The combination function `combine` is an enumeration with the following possible
values (the name after `/` is the matching name for `inputs:combine` in
METRICS.yaml):
 - `0/sum`: Sum of input values
 - `1/min`: Minimum of input values
 - `2/max`: Maximum of input values

Additional notes:
 - The arrays pointed to by `pMetrics`, `pScopes` and `pSummaries` are fully
   contained within the Performance Metrics section.
 - The strings pointed to by `pName`, `pScope` and `pFormula` are fully contained
   within the Performance Metrics section, including the terminating NUL.
 - The format and interpretation of `*pFormula` matches the `inputs:formula` key
   in METRICS.yaml, see there for details.
 - `propMetricId` is the metric identifier used in `profile.db` and `cct.db` for
   propagated metric values for the given `*pName` and `*pScope`.
 - `statMetricId` is the metric identifier used in the `profile.db` summary
   profile for summary statistic values for the given `*pName`, `*pScope`,
   `*pFormula` and `combine`.
 - The stride of `*pMetrics`, `*pScopes` and `*pSummaries` is `szMetric`,
   `szScope` and `szSummary`, respectively. For forward compatibility these
   values should be read and used whenever accessing these arrays.

`meta.db` Load Modules section
-----------------------------
> The Load Modules section lists information about the binaries used during the
> measured application execution.

The Load Modules section starts with the following header:

 Hex | Type | Name          | Description
 ---:| ---- | ------------- | -------------------------------------------------
`00:`|[LMS]xN*(8)|`pModules`| Pointer to an array of `nModules` load module specifications
`08:`|u32|`nModules` | Number of load modules listed in this section
`0c:`|u16|`szModule` | Size of a [Load Module Specification][LMS], currently 16
`0e:`|| **END**

[LMS]: #load-module-specification

Additional notes:
 - The array pointed to by `pModules` is completely within the Load Modules
   section.
 - The stride of `*pModules` is `szModule`, for forwards compatibility this
   should always be read and used as the stride when accessing `*pModules`.

### Load Module Specification ###
A Load Module Specification refers to the following structure:

 Hex | Type | Name | Description
 ---:| ---- | ---- | ----------------------------------------------------------
`00:`|u32|`flags`  | Reserved for future use
|    |
`08:`|char*|`pPath`| Full path to the associated application binary
`10:`|| **END**

Additional notes:
 - The string pointed to by `pPath` is completely within the
   [Common String Table section](#metadb-version-40), including the terminating
   NUL byte.


`meta.db` Source Files section
-----------------------------
> The Source Files section lists information about the application's source
> files, as gathered through debugging information on application binaries.

The Source Files section starts with the following header:

 Hex | Type | Name        | Description
 ---:| ---- | ----------- | ---------------------------------------------------
`00:`|[SFS]xN*(8)|`pFiles`| Pointer to an array of `nFiles` source file specifications
`08:`|u32|`nFiles` | Number of source files listed in this section
`0c:`|u16|`szFile` | Size of a [Source File Specification][SFS], currently 16
`0e:`|| **END**

[SFS]: #source-file-specification

Additional notes:
 - The array pointed to by `pFiles` is completely within the Source Files
   section.
 - The stride of `*pFiles` is `szFile`, for forwards compatibility this should
   always be read and used as the stride when accessing `*pFiles`.

### Source File Specification ###
A Source File Specification refers to the following structure:

 Hex | Type | Name   | Description
 ---:| ---- | ------ | --------------------------------------------------------
`00:`|{Flags}|`flags`| See below
|    |
`08:`|char*|`pPath`  | Path to the source file. Absolute, or relative to the root database directory.
`10:`|| **END**

{Flags} refers to an u32 bitfield with the following sub-fields (bit 0 is
least significant):
 - Bit 0: `copied`. If 1, the source file was copied into the database and
   should always be available. If 0, the source file was not copied and thus may
   need to be searched for.
 - Bits 1-31: Reserved for future use.

Additional notes:
 - The string pointed to by `pPath` is completely within the
  [Common String Table section](#metadb-version-40), including the terminating
   NUL byte.


`meta.db` Functions section
------------------------------
> The Functions section lists various named source-level constructs observed in
> the application. These are inclusively called "functions," however this also
> includes other named constructs (e.g. `<gpu kernel>`).
>
> Counter-intuitively, sometimes we know a named source-level construct should
> exist, but not its actual name. These are "anonymous functions," in this case
> it is the reader's responsibility to construct a reasonable name with what
> information is available.

The Functions section starts with the following header:

 Hex | Type | Name           | Description
 ---:| ---- | -------------- | ------------------------------------------------
`00:`|[FS]xN*(8)|`pFunctions`| Pointer to an array of `nFunctions` function specifications
`08:`|u32|`nFunctions`| Number of functions listed in this section
`0c:`|u16|`szFunction`| Size of a [Function Specification][FS], currently 40
`0e:`|| **END**

[FS]: #function-specification

Additional notes:
 - The array pointed to by `pFunctions` is completely within the Functions
   section.
 - The stride of `*pFunctions` is `szFunction`, for forwards compatibility this
   should always be read and used as the stride when accessing `*pFunctions`.

### Function Specification ###
A Function Specification refers to the following structure:

 Hex | Type | Name       | Description
 ---:| ---- | ---------- | -----------------------------------------------------
`00:`|char*|`pName`      | Human-readable name of the function, or 0
`08:`|[LMS]*(8)|`pModule`| Load module containing this function, or 0
`10:`|u64|`offset`       | Offset within `*pModule` of this function's entry point
`18:`|[SFS]*(8)|`pFile`  | Source file of the function's definition, or 0
`20:`|u32|`line`         | Source line in `*pFile` of the function's definition
`24:`|u32|`flags`        | Reserved for future use
`28:`|| **END**

[LMS]: #load-module-specification
[SFS]: #source-file-specification


Additional notes:
 - If not 0, the string pointed to by `pName` is completely within the
   [Common String Table section](#metadb-version-40), including the terminating
   NUL byte.
 - If not 0, `pModule` points within the [Load Module section](#metadb-load-module-section).
 - If not 0, `pFile` points within the [Source File section](#metadb-source-file-section).
 - At least one of `pName`, `pModule` and `pFile` will not be 0.


`meta.db` Context Tree section
-------------------------------
> The Context Tree section lists the source-level calling contexts in which
> performance data was gathered during application runtime. Each context
> ({Ctx} below) represents a source-level (lexical) context, and these can be
> nested to create paths in the tree:
>
>     function foo()
>       loop at foo.c:12
>         line foo.c:15
>           instruction libfoo.so@0x10123
>
> The relation between contexts may be enclosing lexical context (as above), or
> can be marked as a call of various types (see `relation` below):
>
>     instruction libfoo.so@0x10123
>       [normal call to] function bar()
>         line bar.c:57
>           [inlined call to] function baz()
>             instruction libbar.so@0x25120
>
> Although some patterns in the context tree are more common than others, the
> format is very flexible and will allow almost any nested structure. It is up
> to the reader to interpret the context tree as appropriate.

The Context Tree section starts with the following header:

 Hex | Type | Name        | Description
 ---:| ---- | ----------- | ---------------------------------------------------
`00:`|u64|`szRoots`       | Total size of the `roots` field, in bytes
`08:`|{Ctx}xN*(8)|`pRoots`| Pointer to an array of root context specifications
`10:`|| **END**

{Ctx} here refers to the following structure:

 Hex | Type | Name           | Description
 ---:| ---- | -------------- | ------------------------------------------------
`00:`|u64|`szChildren`       | Total size of `*pChildren`, in bytes
`08:`|{Ctx}xN*(8)|`pChildren`| Pointer to the array of child contexts
`10:`|u32|`ctxId`            | Unique identifier for this context
`14:`|{Flags}|`flags`        | See below
`15:`|u8|`relation`          | Relation this context has with its parent
`16:`|u8|`lexicalType`       | Type of lexical context represented
`17:`|u8|`nFlexWords`        | Size of `flex`, in u8x8 "words" (bytes / 8)
`18:`|u8x8xN|`flex`          | Flexible data region, see below
` *:`|| **END**

`flex` contains a dynamic sequence of sub-fields, which are sequentially
"packed" into the next unused bytes at the minimum alignment. In particular:
 - An u64 sub-field will always take the next full u8x8 "word" and never span
   two words, but
 - Two u32 sub-fields will share a single u8x8 word even if an u64 sub-field
   is between them in the packing order.

The packing order is indicated by the index on `flex`, ie. `flex[1]` is the
sub-field next in the packing order after `flex[0]`. This order still holds
even if not all fields are present for any particular instance.

{Flags} above refers to an u8 bitfield with the following sub-fields (bit 0 is
least significant):
 - Bit 0: `hasFunction`. If 1, the following sub-fields of `flex` are present:
   + `flex[0]:` [FS]* `pFunction`: Function associated with this context
 - Bit 1: `hasSrcLoc`. If 1, the following sub-fields of `flex` are present:
   + `flex[1]:` [SFS]* `pFile`: Source file associated with this context
   + `flex[2]:` u32 `line`: Associated source line in `pFile`
 - Bit 2: `hasPoint`. If 1, the following sub-fields of `flex` are present:
   + `flex[3]:` [LMS]* `pModule`: Load module associated with this context
   + `flex[4]:` u64 `offset`: Assocated byte offset in `*pModule`
 - Bits 3-7: Reserved for future use.

[FS]: #function-specification
[SFS]: #source-file-specification
[LMS]: #load-module-specification

`relation` is an enumeration with the following values:
 - `0`: This context's parent is an enclosing lexical context, eg. source line
   within a function. Specifically, no call occurred.
 - `1`: This context's parent used a typical function call to reach this
   context. The parent context is the source-level location of the call.
 - `2`: This context's parent used an inlined function call (ie. the call was
   inlined by the compiler). The parent context is the source-level location of
   the original call.

The lexical type `lexicalType` is an enumeration with the following values:
 - `0`: Function-like construct. If `hasFunction` is 1, `*pFunction` indicates
   the function represented by this context. Otherwise the function for this
   context is unknown (ie. an unknown function).
 - `1`: Loop construct. `*pFile` and `line` indicate the source line of the
   loop header.
 - `2`: Source line construct. `*pFile` and `line` indicate the source line
   represented by this context.
 - `3`: Single instruction. `*pModule` and `offset` indicate the first byte of the
   instruction represented by this context.

Additional notes:
 - The arrays pointed to by `pRoots` and `pChildren` are completely within the
   Context Tree section. The size of these arrays is given in `szRoots` or
   `szChildren`, in bytes to allow for a singular read of all root/child
   context structures.
 - `pChildren` is 0 if there are no child Contexts, `pRoots` is 0 if there are
   no Contexts in this section period. `szChildren` and `szRoots` are 0 in these
   cases respectively.
 - `pFunction` points within the [Function section](#metadb-function-section).
 - `pFile` points within the [Source File section](#metadb-source-file-section).
 - `pModule` points within the [Load Module section](#metadb-load-module-section).
 - The size of a single {Ctx} is dynamic but can be derived from `nFlexWords`.
   For forward compatibility, readers should always read and use this to read
   arrays of {Ctx} elements.


* * *
`profile.db` version 4.0
========================

The `profile.db` is a binary file containing the performance analysis results
generated by `hpcprof`, arranged by application thread and in a compact sparse
representation. Once an application thread is chosen, the analysis result for
a particular calling context can be obtained through a simple binary search.

The `profile.db` file starts with the following header:

 Hex | Type | Name     | Description
 ---:| ---- | -------- | ------------------------------------------------------
`00:`|u8x14|`magic`    | Format identifier, reads `HPCTOOLKITprof` in ASCII
`0e:`|u8|`majorVersion`| Major version number, currently 4
`0f:`|u8|`minorVersion`| Minor version number, currently 0
`10:`|u64|`szProfileInfos`   | Size of the [Profile Info section][PIsec]
`18:`|u8*(8)|`pProfileInfos` | Pointer to the [Profile Info section][PIsec]
`20:`|u64|`szIdTuples`       | Size of the [Identifier Tuple section][HITsec]
`28:`|u8*(8)|`pIdTuples`     | Pointer to the [Identifier Tuple section][HITsec]
`30:`|| **END**

[PIsec]: #profiledb-profile-info-section
[HITsec]: #profiledb-hierarchical-identifier-tuple-section

The `profile.db` file ends with an 8-byte footer, reading `_prof.db` in ASCII.


`profile.db` Profile Info section
---------------------------------
> The Profile Info section lists the CPU threads and GPU streams present in the
> application execution, and references the performance *profile* measured from
> it during application runtime. All profiles but the first contain propagated
> metric values, the first contains summary statistic values intended to aid
> analysis of the entire application execution. See the comment in the
> [`meta.db` Performance Metrics section](#metadb-performance-metrics-section)
> for more detail on how these are calculated.

The Profile Info section starts with the following header:

 Hex | Type | Name          | Description
 ---:| ---- | ------------- | -------------------------------------------------
`00:`|{PI}xN*(8)|`pProfiles`| Pointer to an array of `nProfiles` profile descriptions
`08:`|u32|`nProfiles`       | Number of profiles listed in this section
`0a:`|u8|`szProfile`        | Size of a {PI} structure, currently 16
`0b:`|| **END**

{PI} above refers to the following structure:

 Hex | Type | Name        | Description
 ---:| ---- | ------------| --------------------------------------------------
`00:`|[PSVB]|`valueBlock` | Header for the values for this application thread
`20:`|[HIT]*(8)|`pIdTuple`| Identifier tuple for this application thread
`28:`|| **END**

[HIT]: #profiledb-hierarchical-identifier-tuple-section
[PSVB]: #profile-major-sparse-value-block

Additional notes:
 - The array pointed to by `pProfiles` is fully contained within the Profile
   Info section.
 - `pIdTuple` points within the [Identifier Tuple section](#profledb-hierarchical-identifier-tuple-section).
 - Profiles are unordered within this section, except the first which is
   classified as the "summary profile."
 - The stride of `*pProfiles` is equal to `szProfile`, for forward compatibility
   this should always be read and used as the stride when accessing `*pProfiles`.

### Profile-Major Sparse Value Block ###
> All performance data in the `profile.db` and `cct.db` is arranged in
> internally-sparse "blocks," the variant present in `profile.db` uses one for
> each application thread (or equiv.) measured during application runtime.
> Conceptually, these are individual planes of a 3-dimensional tensor indexed by
> application thread (profile), context, and metric, in that order.

Each Profile-Major Sparse Value Block has the following structure:

 Hex | Type | Name             | Description
 ---:| ---- | ---------------- | ---------------------------------------------------
`00:`|u64|`nValues`            | Number of non-zero values in this block
`08:`|{Val}xN*(2)|`pValues`    | Pointer to an array of `nValues` value pairs
`10:`|u32|`nCtxs`              | Number of non-empty contexts in this block
|    |
`18:`|{Idx}xN*(4)|`pCtxIndices`| Pointer to an array of `nCtxs` context indices
`20:`|| **END**

{Val} above refers to the following structure:

 Hex | Type | Name  | Description
 ---:| ---- | ----- | ---------------------------------------------------------
`00:`|u16|`metricId`| Unique identifier of a metric listed in the [`meta.db`](#metadb-performance-metrics-section)
`02:`|f64|`value`   | Value of the metric indicated by `metricId`
`0a:`|| **END**

{Idx} above refers to the following structure:

 Hex | Type | Name    | Description
 ---:| ---- | ------- | -------------------------------------------------------
`00:`|u32|`ctxId`     | Unique identifier of a context listed in the [`meta.db`](#metadb-context-tree-section)
`04:`|u64|`startIndex`| Start index of `*pValues` attributed to the referenced context
`0c:`|| **END**

The sub-array of `*pValues` attributed to the context referenced by `ctxId`
starts at index `startIndex` and ends just before the `startIndex` of the
following {Idx} structure, if this {Idx} is the final element of `*pCtxIndices`
(index `nCtxs - 1`) then the end is the last element of `*pValues` (index
`nValues - 1`).

Additional notes:
 - `pValues` and `pCtxIndices` point outside the sections listed in the
   [`profile.db` header](#profiledb-version-40).
 - The arrays pointed to by `pValues` and `pCtxIndices` are subsequent: only
   padding is placed between them and `pValues < pCtxIndices`. This allows
   readers to read a plane of data in a single contiguous blob from `pValues`
   to `pCtxIndices + nCtxs * 0xc`.
 - `metricId` is usually a `propMetricId` listed in the
   [`meta.db` performance metrics section](#metadb-performance-metrics-section),
   unless this is the block for the "summary profile" in which case `metricId`
   is a `statMetricId` instead.
 - `ctxId` is a `ctxId` listed in the [`meta.db` context tree section](#metadb-context-tree-section).
 - `*pValues` and `*pCtxIndices` are sorted by `metricId` and `ctxId`,
   respectively. This allows the use of binary search (or some variant thereof)
   to locate the value(s) for a particular context or metric.
 - `value` and `startIndex` are not aligned, however `metricId` and `ctxId` are.
   This should in general not pose a significant performance penalty.
   See [Alignment properties](#alignment-properties) above.


`profile.db` Hierarchical Identifier Tuple section
--------------------------------------------------
> Application threads (or equiv.) are identified in a human-readable manner via
> the use of Hierarchical Identifier Tuples. Each tuple lists a series of
> identifications for an application thread, for instance compute node and
> MPI rank. The identifications within a tuple are ordered roughly by
> "hierarchy" from largest to smallest, for eg. compute node will appears before
> MPI rank if both are present.

The Hierarchical Identifier Tuple section contains multiple Identifier Tuples,
each of the following structure:

 Hex | Type | Name | Description
 ---:| ---- | ---- | ----------------------------------------------------------
`00:`|u16|`nIds`   | Number of identifications in this tuple
|    |
`08:`|{Id}xN|`ids` | Array of `nIds` identifications for an application thread
` *:`|| **END**

{Id} above refers to the following structure:

 Hex | Type | Name    | Description
 ---:| ---- | ------- | -------------------------------------------------------
`00:`|u8|`kind`       | One of the values listed in the [`meta.db` Identifier Names section][INsec].
|    |
`02:`|{Flags}|`flags` | See below.
`04:`|u32|`logicalId` | Logical identifier value, may be arbitrary but dense towards 0.
`08:`|u64|`physicalId`| Physical identifier value, eg. hostid or PCI bus index.
`10:`|| **END**

[INsec]: #metadb-hierarchical-identifier-names-section

{Flags} above refers to an u16 bitfield with the following sub-fields (bit 0 is
least significant):
 - Bit 0: `isPhysical`. If 1, the `kind` represents a physical (hardware or VM)
   construct for which `physicalId` is the identifier (and `logicalId` is
   arbitrary but distinct). If 0, `kind` represents a logical (software-only)
   construct (and `physicalId` is `logicalId` zero-extended to 64 bits).
 - Bits 1-15: Reserved for future use.

> The name associated with the `kind` in the [`meta.db`][INsec] indicates the
> meaning of `logicalId` (if `isPhysical == 0`) and/or `physicalId` (if
> `isPhysical == 1`). The following names are in current use with the given
> meanings:
>  - "NODE": Compute node, `physicalId` indicates the hostid of the node.
>  - "RANK": Rank of the process (from eg. MPI), `logicalId` indicates the rank.
>  - "CORE": Core the application thread was bound to, `physicalId` indicates
>    the index of the first hardware thread as listed in /proc/cpuinfo.
>  - "THREAD": Application CPU thread, `logicalId` indicates the index.
>  - "GPUCONTEXT": Context used to access a GPU, `logicalId` indicates the index
>    as given by the underlying programming model (eg. CUDA context index).
>  - "GPUSTREAM": Stream/queue used to push work to a GPU, `logicalId` indicates
>    the index as given by the programming model (eg. CUDA stream index).
>
> These names/meanings are not stable and may change without a version bump, it
> is highly recommended that readers refrain from any special-case handling of
> particular `kind` values where possible.

Additional notes:
 - While `physicalId` (when valid) lists a physical identification for an
   application thread, the contained value is often too obtuse for generating
   human-readable output listing many identifiers. `logicalId` is a suitable
   replacement in these cases, as these values are always dense towards 0.


* * *
`cct.db` version 4.0
====================

The `cct.db` is a binary file containing the performance analysis results
generated by `hpcprof`, arranged by calling context and in a compact sparse
representation. Once a calling context is chosen, the analysis result for
a particular metric can be obtained through a simple binary search.

The `cct.db` file starts with the following header:

 Hex | Type | Name     | Description
 ---:| ---- | -------- | ------------------------------------------------------
`00:`|u8x14|`magic`    | Format identifier, reads `HPCTOOLKITctxt` in ASCII
`0e:`|u8|`majorVersion`| Major version number, currently 4
`0f:`|u8|`minorVersion`| Minor version number, currently 0
`10:`|u64|`szCtxInfo`  | Size of the [Context Info section][CIsec]
`18:`|u8*(8)|`pCtxInfo`| Pointer to the [Context Info section][CIsec]
`20:`|| **END**

[CIsec]: #cctdb-context-info-section

The `cct.db` file ends with an 8-byte footer, reading `__ctx.db` in ASCII.


`cct.db` Context Info section
-----------------------------
> The Context Info section associates contexts with a "block" of performance
> data, similar to what the [`profile.db` Profile Info section](#metadb-profiledb-profile-info-section)
> does for application threads.

The Context Info section starts with the following header:

 Hex | Type | Name      | Description
 ---:| ---- | --------- | -----------------------------------------------------
`00:`|{CI}xN*(8)|`pCtxs`| Pointer to an array of `nCtxs` context descriptions
`08:`|u32|`nCtxs`       | Number of contexts listed in this section
`0c:`|u8|`szCtx`        | Size of a {CI} structure, currently 22
`0d:`|| **END**

{CI} above refers to the following structure:

 Hex | Type | Name       | Description
 ---:| ---- | ---------- | -----------------------------------------------
`00:`|[CSVB]|`valueBlock`| Header for the values for this Context
`20:`|| **END**

[CSVB]: #context-major-sparse-value-block

Additional notes:
 - The array pointed to by `pCtxs` is fully contained within the Context Info
   section.
 - `(*pCtxs)[ctxId]` is associated with the context with the matching `ctxId`
   as listed in the [`meta.db` context tree section](#metadb-context-tree-section).
 - The stride of `*pCtxs` is `szCtx`, for forward compatibility this should
   always be read and used as the stride when accessing `*pCtxs`.

### Context-Major Sparse Value Block ###
> The Context-Major Sparse Value Block is very similar in structure to the
> [Profile-Major Sparse Value Block](#profile-major-sparse-value-block), they
> differ mainly in the order in which their 3 dimensions are indexed.
> For Context-Major, this order is context, metric, application thread (profile).

Each Context-Major Sparse Value Block has the following structure:

 Hex | Type | Name          | Description
 ---:| ---- | ------------- | -------------------------------------------------
`00:`|u64|`nValues`         | Number of non-zero values in this block
`08:`|{Val}xN*(2)|`pValues` | Pointer to an array of `nValues` value pairs
`10:`|u16|`nMetrics`        | Number of non-empty metrics in this block
|    |
`18:`|{Idx}xN*(4)|`pMetricIndices`| Pointer to an array of `nMetrics` metric indices
`20:`|| **END**

{Val} above refers to the following structure:

 Hex | Type | Name   | Description
 ---:| ---- | ------ | ----------------------------------------------------------
`00:`|u32|`profIndex`| Index of a profile listed in the [`profile.db`](#profiledb-profile-info-section)
`04:`|f64|`value`    | Value attributed to the profile indicated by `profIndex`
`0c:`|| **END**

{Idx} above refers to the following structure:

 Hex | Type | Name    | Description
 ---:| ---- | ------- | -------------------------------------------------------
`00:`|u16|`metricId`  | Unique identifier of a metric listed in the [`meta.db`](#metadb-performance-metrics-section)
`02:`|u64|`startIndex`| Start index of `*pValues` from the associated metric
`0a:`|| **END**

The sub-array of `*pValues` from to the metric referenced by `metId` starts at
index `startIndex` and ends just before the `startIndex` of the following {Idx}
structure, if this {Idx} is the final element of `*pCtxIndices` (index
`nMetrics - 1`) then the end is the last element of `*pValues` (index
`nValues - 1`).

Additional notes:
 - `pValues` and `pMetricIndices` point outside the sections listed in the
   [`cct.db` header](#cctdb-version-40).
 - The arrays pointed to by `pValues` and `pMetricIndices` are subsequent: only
   padding is placed between them and `pValues < pMetricIndices`. This allows
   readers to read a plane of data in a single contiguous blob from `pValues`
   to `pMetricIndices + nMetrics * 0xa`.
 - `metricId` is a `propMetricId` listed in the
   [`meta.db` performance metrics section](#metadb-performance-metrics-section).
   Unlike `profile.db`, the `cct.db` does not include the "summary profile."
 - `*pValues` and `*pMetricIndices` are sorted by `profIdx` and `metricId`,
   respectively. This allows the use of binary search (or some variant thereof)
   to locate the value(s) for a particular metric or application thread.
 - `value` and `startIndex` are not aligned, however `profIdx` and `metricId`
   are. This should in general not pose a significant performance penalty.
   See [Alignment properties](#alignment-properties) above.


* * *
`trace.db` version 4.0
======================

The `trace.db` file starts with the following header:

 Hex | Type | Name     | Description
 ---:| ---- | -------- | ------------------------------------------------------
`00:`|u8x14|`magic`    | Format identifer, reads `HPCTOOLKITtrce` in ASCII
`0e:`|u8|`majorVersion`| Major version number, currently 4
`0f:`|u8|`minorVersion`| Minor version number, currently 0
`10:`|u64|`szHeaders`  | Size of the [Trace Headers section][THsec]
`18:`|u8*(8)|`pHeaders`| Pointer to the [Trace Headers section][THsec]
`20:`|| **END**

[THsec]: #tracedb-trace-headers-section


`trace.db` Trace Headers section
--------------------------------

The Trace Headers sections starts with the following structure:

 Hex | Type | Name        | Description
 ---:| ---- | ----------- | ---------------------------------------------------
`00:`|{TH}xN*(8)|`pTraces`| Pointer to an array of `nTraces` trace headers
`08:`|u32|`nTraces`       | Number of traces listed in this section
`0c:`|u8|`szTrace`        | Size of a {TH} structure, currently 22
`0d:`|| **END**

{TH} above refers to the following structure:

 Hex | Type | Name       | Description
 ---:| ---- | ---------- | ----------------------------------------------------
`00:`|u32|`profIdx`      | Index of a profile listed in the [`profile.db`](#profiledb-profile-info-section)
|    |
`08:`|{Elem}*(8)|`pStart`| Pointer to the first element of the trace line (array)
`10:`|{Elem}*(4)|`pEnd`  | Pointer to the after-end element of the trace line (array)
`18:`|| **END**

{Elem} above refers to the following structure:

 Hex | Type | Name   | Description
 ---:| ---- | ------ | --------------------------------------------------------
`00:`|u64|`timestamp`| Timestamp of the trace sample (nanoseconds since the epoch)
`08:`|u32|`ctxId`    | Unique identifier of a context listed in [`meta.db`](#metadb-context-tree-section)
`0c:`|| **END**

Additional notes:
 - The array pointed to by `pTraces` is completely within the Trace Headers
   section. The pointers `pStart` and `pEnd` point outside any of the sections
   listed in the [`trace.db` header](#tracedb-version-40).
 - The array starting at `pStart` and ending just before `pEnd` is sorted in
   order of increasing `timestamp`.
 - The stride of `*pTraces` is `szTrace`, for forward compatibility this value
   should be read and used when accessing `*pTraces`.
 - `timestamp` is only aligned for even elements in a trace line array. Where
   possible, readers are encouraged to prefer accessing even elements.
   See [Alignment properties](#alignment-properties) above.
