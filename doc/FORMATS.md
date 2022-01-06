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
   * s`N`/u`N`/i`N` is a signed/unsigned/signless integer of `N` bits.
     Multi-byte integers are laid out in big-endian order.
   * f64 is an IEEE 754 double-precision floating-point number, laid out in
     big-endian order (ie. sign and exponent bytes come first).
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
boundary equivalent to their size (eg. i64 to an 8-byte boundary, i32 to 4-byte,
etc.). Structures are aligned based on the maximum required alignment among
their fields.

Pointers containing aligned values have an additional (A) suffix indicating
the alignment: i8\*(8) is aligned to an 8-byte boundary, i32*(4) is aligned to
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
`00:`|i8x14|`magic`      | Format identifier, reads `HPCTOOLKITmeta` in ASCII
`0e:`|u8|`majorVersion`  | Major version number, currently 4
`0f:`|u8|`minorVersion`  | Minor version number, currently 0
`10:`|u64|`szGeneral`    | Size of the [General Properties section][GPsec]
`18:`|i8*(8)|`pGeneral`  | Pointer to the [General Properties section][GPsec]
`20:`|u64|`szIdNames`    | Size of the [Identifier Names section][INsec]
`28:`|i8*(8)|`pIdNames`  | Pointer to the [Identifier Names section][INsec]
`30:`|u64|`szMetrics`    | Size of the [Performance Metrics section][PMsec]
`38:`|i8*(8)|`pMetrics`  | Pointer to the [Performance Metrics section][PMsec]
`40:`|u64|`szContext`    | Size of the [Context Tree section][CTsec]
`48:`|i8*(8)|`pContext`  | Pointer to the [Context Tree section][CTsec]
`50:`|u64|`szStrings`    | Size of the Common String Table section
`58:`|i8*(1)|`pStrings`  | Pointer to the Common String Table section
`60:`|u64|`szModules`    | Size of the [Load Modules section][LMsec]
`68:`|i8*(8)|`pModules`  | Pointer to the [Load Modules section][LMsec]
`70:`|u64|`szFiles`      | Size of the [Source Files section][SFsec]
`78:`|i8*(8)|`pFiles`    | Pointer to the [Source Files section][SFsec]
`80:`|u64|`szFunctions`  | Size of the [Functions section][Fnsec]
`88:`|i8*(8)|`pFunctions`| Pointer to the [Functions section][Fnsec]
`90:`|| **END**

[GPsec]: #metadb-general-properties-section
[INsec]: #metadb-hierarchical-identifier-names-section
[PMsec]: #metadb-performance-metrics-section
[CTsec]: #metadb-context-tree-section
[LMsec]: #metadb-load-modules-section
[SFsec]: #metadb-source-files-section
[Fnsec]: #metadb-functions-section

The `meta.db` file ends with an 8-byte footer, reading `__meta.db` in ASCII.

Additional notes:
 - The Common String Table section has no particular interpretation, it is used
   as a section to store strings for the [Load Modules section][LMsec],
   the [Source Files section][SFsec], and the [Functions section][Fnsec].

`meta.db` General Properties section
-----------------------------------------
The General Properties section starts with the following header:

 Hex | Type | Name       | Description
 ---:| ---- | ---------- | ----------------------------------------------------
`00:`|char*|`title`      | Title of the database. May be provided by the user.
`08:`|char*|`description`| Human-readable Markdown description of the database.
`10:`|| **END**

`description` provides information about the measured execution and subsequent
analysis that may be of interest to users. The exact layout and the information
contained may change without warning.

Additional notes:
 - The strings pointed to by `title` and `description` are fully contained
   within the General Properties section, including the terminating NUL byte.


`meta.db` Hierarchical Identifier Names section
-----------------------------------------------
> In future versions of HPCToolkit new functionality may be added that requires
> new values for the the `kind` field of a [Hierarchical Identifier Tuple](#hierarchical-identifier-tuple).
> The Hierarchical Identifier Names section provides human-readable names for
> all possible values for forward compatibility.

The Hierarchical Identifier Names section starts with the following header:

 Hex | Type | Name         | Description
 ---:| ---- | ------------ | --------------------------------------------------
`00:`|char\*xN*(8)|`pNames`| Pointer to an array of `nKinds` human-readable names for Id. Names
`08:`|u16|`nKinds`         | Number of names listed in this section
`0a:`|| **END**

`(*pNames)[kind]` is the human-readable name for the Identifier kind `kind`,
where `kind` is part of a [Hierarchical Identifier Tuple](#hierarchical-identifier-tuple).

Additional notes:
 - The strings pointed to by the elements of `names` are fully contained within
   the Hierarchical Identifier Names section, including the terminating NUL.


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
`00:`|char*|`name`     | Canonical name of the raw performance metric
`08:`|u16|`nScopes`    | Number of propagation scopes used for this metric
|    |
`10:`|{PS}xN*(8)|`pScopes`| Pointer to an array of `nScopes` propagation scope descriptions
`18:`|| **END**

{PS} above refers to the following sub-structure:

 Hex | Type | Name        | Description
 ---:| ---- | ----------- | ---------------------------------------------------
`00:`|char*|`scope`       | Canonical name of the propagation scope
`08:`|u16|`nSummaries`    | Number of summary statistics generated for this scope
`0a:`|i16|`propMetricId`  | Unique identifier for propagated metric values
|    |
`10:`|{SS}xN*(8)|`pSummaries`| Array of `nSummaries` summary statistic descriptions
`18:`|| **END**

{SS} above refers to the following sub-structure:

 Hex | Type | Name      | Description
 ---:| ---- | --------- | -----------------------------------------------------
`00:`|char*|`formula`   | Canonical unary function used for summary values
`08:`|u8|`combine`      | Combination n-ary function used for summary values
|    |
`0a:`|i16|`statMetricId`| Unique identifier for summary statistic values
|    |
`10:`|| **END**

The combination function `combine` is an enumeration with the following possible
values (the name after `/` is the matching name for `inputs:combine` in
METRICS.yaml):
 - `0/sum`: Sum of input values
 - `1/min`: Minimum of input values
 - `2/max`: Maximum of input values

Additional notes:
 - The arrays pointed to by `pMetrics`, `pScopes` and `pSummaries` are fully
   contained within the Performance Metrics section.
 - The strings pointed to by `name`, `scope` and `formula` are fully contained
   within the Performance Metrics section, including the terminating NUL.
 - The format and interpretation of `formula` matches the `inputs:formula` key
   in METRICS.yaml, see there for details.
 - `propMetricId` is the metric identifier used in profile.db and cct.db for
   propagated metric values for the given `name` and `scope`.
 - `statMetricId` is the metric identifier used in the profile.db summary
   profile for summary statistic values for the given `name`, `scope`, `formula`
   and `combine`.
 - The stride of `*pMetrics`, `*pScopes` and `pSummaries` is `szMetric`,
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
`00:`|i32|`flags`  | Reserved for future use
|    |
`08:`|char*|`path` | Full path to the associated application binary
`10:`|| **END**

Additional notes:
 - The string pointed to by `path` is completely within the
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
`08:`|char*|`path`   | Path to the source file. Absolute, or relative to the root database directory.
`10:`|| **END**

{Flags} refers to an i32 bitfield with the following sub-fields (bit 0 is
least significant):
 - Bit 0: `copied`. If 1, the source file was copied into the database and
   should always be available. If 0, the source file was not copied and thus may
   need to be searched for.
 - Bits 1-31: Reserved for future use.

Additional notes:
 - The string pointed to by `path` is completely within the
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

 Hex | Type | Name      | Description
 ---:| ---- | --------- | -----------------------------------------------------
`00:`|char*|`name`      | Human-readable name of the function, or 0
`08:`|[LMS]*(8)|`module`| Load module containing this function, or 0
`10:`|u64|`offset`      | Offset within `module` of this function's entry point
`18:`|[SFS]*(8)|`file`  | Source file of the function's definition, or 0
`20:`|u32|`line`        | Source line in `file` of the function's definition
`24:`|i32|`flags`       | Reserved for future use
`28:`|| **END**

[LMS]: #load-module-specification
[SFS]: #source-file-specification


Additional notes:
 - If not 0, the string pointed to by `name` is completely within the
   [Common String Table section](#metadb-version-40), including the terminating
   NUL byte.
 - If not 0, `module` points within the [Load Module section](#metadb-load-module-section).
 - If not 0, `file` points within the [Source File section](#metadb-source-file-section).
 - At least one of `name`, `module` and `file` will not be 0.


`meta.db` Context Tree section
-------------------------------
> The Context Tree section lists the calling contexts observed during the
> application's execution, expanded with additional source-level context to
> aid manual performance analysis.

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
`10:`|i32|`ctxId`            | Unique identifier for this context
`14:`|{Flags}|`flags`        | See below
`16:`|u8|`lexicalType`       | Type of lexical context represented here
`17:`|u8|`nFlexWords`        | Size of `flex`, in i8x8 "words" (bytes / 8)
`18:`|i8x8xN|`flex`          | Flexible data region, see below
` *:`|| **END**

`flex` contains a dynamic sequence of sub-fields, which are sequentially
"packed" into the next unused bytes at the minimum alignment. In particular:
 - An i64 sub-field will always take the next full i8x8 "word" and never span
   two words, but
 - Two i32 sub-fields will share a single i8x8 word even if an i64 sub-field
   is between them in the packing order.

The packing order is indicated by the index on `flex`, ie. `flex[1]` is the
sub-field next in the packing order after `flex[0]`. This order still holds
even if not all fields are present for any particular instance.

The lexical type `lexicalType` is an enumeration with the following values:
 - `0`: Function-like construct. If `hasFunction` is 1, `function` indicates the
   function represented by this context. Otherwise the function for this context
   is unknown (ie. an unknown function).
 - `1`: Loop construct. `srcFile` and `srcLine` indicate the source line of the
   loop header.
 - `2`: Source line construct. `file` and `line` indicate the source line
   represented by this context.
 - `3`: Single instruction. `module` and `offset` indicate the first byte of the
   instruction represented by this context.

{Flags} above refers to an i16 bitfield with the following sub-fields (bit 0 is
least significant):
 - Bit 0: `isCallee`. If 1, this context was called by its parent, and the
   following sub-field of `flex` is present:
   + `flex[0]:` i8x8xN `calleeFlex`: Flexible data sub-region dedicated to
     describing the call that took place to reach this context. Size in words is
     `nCalleeFlexWords`, see the [Call properties](#call-properties) below.
 - Bit 1: `hasFunction`. If 1, the following sub-fields of `flex` are present:
   + `flex[1]:` [FS]* `function`: Function associated with this context
 - Bit 2: `hasSrcLoc`. If 1, the following sub-fields of `flex` are present:
   + `flex[2]:` [SFS]* `file`: Source file associated with this context
   + `flex[3]:` u32 `line`: Associated source line in `file`
 - Bit 3: `hasPoint`. If 1, the following sub-fields of `flex` are present:
   + `flex[4]:` [LMS]* `module`: Load module associated with this context
   + `flex[5]:` u64 `offset`: Assocated byte-offset in `module`
 - Bit 4: `isCallSrcLine`. If 1, `hasSrcLoc == 1` and `file:line` may be used
   in place of `callFile:callLine` in children if the latter is not present.
   See below.
 - Bits 5-15: Reserved for future use.

[FS]: #function-specification
[SFS]: #source-file-specification
[LMS]: #load-module-specification

Additional notes:
 - The arrays pointed to by `pRoots` and `pChildren` are completely within the
   Context Tree section. The size of these arrays is given in `szRoots` or
   `szChildren`, in bytes to allow for a singular read of all root/child
   context structures.
 - `function` points within the [Function section](#metadb-function-section).
 - `file` points within the [Source File section](#metadb-source-file-section).
 - `module` points within the [Load Module section](#metadb-load-module-section).
 - The size of a single {Ctx} is dynamic but can be derived from `nFlexWords`.
   For forward compatibility, readers should always read and use this to read
   arrays of {Ctx} elements.
 - If `isCallee == 1`, `nCalleeFlexWords` is the first byte of `flex`. For
   forward compatibility readers should read and use this value when accessing
   sub-fields within `flex`, sub-field `flex[1]` starts at exactly
   `flex + nCalleeFlexWords * 8`.

#### Call properties ####

If `isCallee == 1`, this context was called by its parent. `calleeFlex` is
packed following the same rules as `flex`. The first sub-field `calleeFlex[0]`
is always the following 4-byte structure:

 Hex | Type | Name          | Description
 ---:| ---- | ------------- | -------------------------------------------------
`00:`|u8|`nCalleeFlexWords` | Number of words of `flex` used for callee data
`01:`|u8|`callType`         | Type of call performed by the parent
`02:`|{CFlags}|`calleeFlags`| See below.

`callType` is an enumeration with the following possible values:
 - `0`: Typical, nominal function call.
 - `1`: Inlined function call (ie. function call was inlined by a compiler).

{CFlags} above refers to an i16 bitfield with the following sub-fields (bit 0 is
least significant):
 - Bit 0: `calleeHasSrcLoc`. If 1, the following sub-fields are present:
   * `calleeFlex[1]:` [SFS]* `callFile`: Source file where the call occurred
   * `calleeFlex[2]:` u32 `callLine`: Source line where the call occurred

   If 0, the source location of the call is listed in the nearest ancestor
   context where `isCallSrcLoc == 1` (excluding self), unless `isCallee == 1`
   for some intervening context (excluding self and the ancestor in question).

 - Bits 1-15: Reserved for future use.

[SFS]: #source-file-specification

Additional notes:
 - `callFile` points within the [Source File section](#metadb-source-file-section).


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
`00:`|i8x14|`magic`    | Format identifier, reads `HPCTOOLKITprof` in ASCII
`0e:`|u8|`majorVersion`| Major version number, currently 4
`0f:`|u8|`minorVersion`| Minor version number, currently 0
`10:`|u64|`szProfileInfos`   | Size of the [Profile Info section][PIsec]
`18:`|i8*(8)|`pProfileInfos` | Pointer to the [Profile Info section][PIsec]
`20:`|u64|`szIdTuples`       | Size of the [Identifier Tuple section][HITsec]
`28:`|i8*(8)|`pIdTuples`     | Pointer to the [Identifier Tuple section][HITsec]
`30:`|| **END**

[PIsec]: #profiledb-profile-info-section

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

 Hex | Type | Name            | Description
 ---:| ---- | --------------- | --------------------------------------------------
`00:`|[PSVB]*(8)|`pValueBlock`| Pointer to the values for this application thread
`08:`|[HIT]*(8)|`pIdTuple`    | Identifier tuple for this application thread
`10:`|| **END**

[HIT]: #hierarchical-identifier-tuple
[PSVB]: #profile-major-sparse-value-block

Additional notes:
 - The array pointed to by `pProfiles` is fully contained within the Profile
   Info section.
 - `pIdTuple` points within the [Identifier Tuple section](#profledb-hierarchical-identifier-tuple-section).
 - `pValueBlock` points outside the sections listed in the
   [`profile.db` header](#profiledb-version-40).
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
`00:`|i16|`metricId`| Unique identifier of a metric listed in the [`meta.db`](#metadb-performance-metrics-section)
`02:`|f64|`value`   | Value of the metric indicated by `metricId`
`0a:`|| **END**

{Idx} above refers to the following structure:

 Hex | Type | Name    | Description
 ---:| ---- | ------- | -------------------------------------------------------
`00:`|i32|`ctxId`     | Unique identifier of a context listed in the [`meta.db`](#metadb-context-tree-section)
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
`02:`|{Id}xN|`ids` | Array of `nIds` identifications for an application thread
` *:`|| **END**

{Id} above refers to the following structure:

 Hex | Type | Name    | Description
 ---:| ---- | ------- | -------------------------------------------------------
`00:`|{Bits}|`bits`   | Interpretation of this identifier, see below
|    |
`04:`|u32|`logicalId` | Logical identifier value
`08:`|u64|`physicalId`| Physical identifier value, eg. hostid or PCI bus index
`10:`|| **END**

{Bits} above is an i16 bitfield with the following sub-fields (bit 0 is least
significant):
 - Bits 0-13: `kind`. One of the values listed in the
   [`meta.db` Identifier Names section](#metadb-hierarchical-identifier-names-section).
   Each `kind` refers to a particular hardware or virtual construct of interest
   to application developers, eg. compute node or MPI rank.
 - Bits 14-15: `interp`. Interpretation of the identifier fields `physicalId`
   and `logicalId`. Enumeration with the following possible values:
   * `0`: Both `physicalId` and `logicalId` are valid.
   * `1`,`2`: For internal use only.
   * `3`: Only `logicalId` is valid.

Additional notes:
 - While `physicalId` (when valid) lists a physical identification for an
   application thread, the contained value is often too obtuse for generating
   human-readable output listing many identifiers. `logicalId` is a suitable
   replacement in these cases, as these values are always dense towards 0.
 - The meaning of `physicalId` is based completely on the `kind`. The following
   named `kind`s have well-defined meanings for `physicalId`:
   * `NODE`: hostid of the compute node, zero-extended to 64 bits


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
`00:`|i8x14|`magic`    | Format identifier, reads `HPCTOOLKITctxt` in ASCII
`0e:`|u8|`majorVersion`| Major version number, currently 4
`0f:`|u8|`minorVersion`| Minor version number, currently 0
`10:`|u64|`szCtxInfo`  | Size of the [Context Info section][CIsec]
`18:`|i8*(8)|`pCtxInfo`| Pointer to the [Context Info section][CIsec]
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

 Hex | Type | Name            | Description
 ---:| ---- | --------------- | -----------------------------------------------
`00:`|[CSVB]*(8)|`pValueBlock`| Pointer to the values for this context
`08:`|| **END**

[CSVB]: #context-major-sparse-value-block

Additional notes:
 - The array pointed to by `pCtxs` is fully contained within the Context Info
   section.
 - `(*pCtxs)[ctxId]` is associated with the context with the matching `ctxId`
   as listed in the [`meta.db` context tree section](#metadb-context-tree-section).
 - `pValueBlock` points outside the sections listed in the [`cct.db` header](#cctdb-version-40)
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
`18:`|{Idx}xN*(4)|`pMetIdxs`| Pointer to an array of `nMetrics` metric indices
`20:`|| **END**

{Val} above refers to the following structure:

 Hex | Type | Name | Description
 ---:| ---- | ---- | ----------------------------------------------------------
`00:`|u16|`profIdx`| Index of a profile listed in the [`profile.db`](#profiledb-profile-info-section)
`02:`|f64|`value`  | Value attributed to the profile indicated by `profIdx`
`0a:`|| **END**

{Idx} above refers to the following structure:

 Hex | Type | Name    | Description
 ---:| ---- | ------- | -------------------------------------------------------
`00:`|i32|`metId`     | Unique identifier of a metric listed in the [`meta.db`](#metadb-performance-metrics-section)
`04:`|u64|`startIndex`| Start index of `*pValues` from the associated metric
`0c:`|| **END**

The sub-array of `*pValues` from to the metric referenced by `metId` starts at
index `startIndex` and ends just before the `startIndex` of the following {Idx}
structure, if this {Idx} is the final element of `*pCtxIndices` (index
`nMetrics - 1`) then the end is the last element of `*pValues` (index
`nValues - 1`).

Additional notes:
 - `pValues` and `pMetIdxs` point outside the sections listed in the
   [`cct.db` header](#cctdb-version-40).
 - `metricId` is a `propMetricId` listed in the
   [`meta.db` performance metrics section](#metadb-performance-metrics-section).
   Unlike `profile.db`, the `cct.db` does not include the "summary profile."
 - `*pValues` and `*pMetIndices` are sorted by `profIdx` and `metId`,
   respectively. This allows the use of binary search (or some variant thereof)
   to locate the value(s) for a particular metric or application thread.
 - `value` and `startIndex` are not aligned, however `profIdx` and `metId` are.
   This should in general not pose a significant performance penalty.


* * *
`trace.db` version 4.0
======================

The `trace.db` file starts with the following header:

 Hex | Type | Name     | Description
 ---:| ---- | -------- | ------------------------------------------------------
`00:`|i8x14|`magic`    | Format identifer, reads `HPCTOOLKITtrce` in ASCII
`0e:`|u8|`majorVersion`| Major version number, currently 4
`0f:`|u8|`minorVersion`| Minor version number, currently 0
`10:`|u64|`szHeaders`  | Size of the [Trace Headers section][THsec]
`18:`|i8*(8)|`pHeaders`| Pointer to the [Trace Headers section][THsec]
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
`08:`|i32|`ctxId`    | Unique identifier of a context listed in [`meta.db`](#metadb-context-tree-section)
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
