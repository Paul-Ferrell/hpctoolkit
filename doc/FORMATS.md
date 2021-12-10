HPCToolkit Database File Formats
================================

A full HPCToolkit database consists of the following files and directories:

    database/
    |-- FORMATS.md     This file
    |-- meta.db        Properties of the measured application execution
    |-- profile.db     Performance measurements arranged by application thread
    |-- cct.db         Performance measurements arranged by calling context
    |-- trace.db       Time-centric execution traces
    `-- src/           Relevant application source files

The following sections go into more detail for each of these files.

### Glossary ###
- **Application Thread**: General term for a serial execution of the
  application, whether it executed on the CPU or an external accelerator (GPU)
  or otherwise. Examples include standard PThreads and CUDA streams.
- **Profile**: The set of performance measurements associated with a single
  application thread.
- **Metric**: A performance-related value that is measured by sampling the
  application, enabled by passing an `-e` argument to `hpcrun`. Examples
  include `REALTIME` (wallclock time spent), `cycles` (CPU cycles spent) and
  `GPUOPS` (time spent in GPU operations).

`meta.db` version 4.0
----------------------------
The `meta.db` is a binary file listing various properties of the
application execution such as application binaries and enabled metrics.
Most importantly it lists the entire measured calling context tree.

The `meta.db` has the following overall structure:

    |--- Header -----------------------------------------------| Offset (dec hex), field size
    | Magic Identifier ("HPCTOOLKIT_expmt")                    |   0  0, 16 bytes
    | Version (major, minor. Currently 4.0)                    |  16 10,  2 bytes
    | Number of sections in this header (num_sec)              |  18 12,  2 bytes
    |                                                          |          4 bytes
    | General Properties section size (general_size)           |  24 18,  8 bytes
    | General Properties section offset (general_ptr)          |  32 20,  8 bytes
    | Performance Metric section size (metric_size)            |  40 28,  8 bytes
    | Performance Metric section offset (metric_ptr)           |  48 30
    | Context Tree section size (ctree_size)                   |  56 38,  8 bytes
    | Context Tree section offset (ctree_ptr)                  |  64 40,  8 bytes
    | Context Attribute Strings section size (str_size)        |  72 48,  8 bytes
    | Context Attribute Strings section offset (str_ptr)       |  80 50,  8 bytes
    | Application Binary section size (binaries_size)          |  88 58,  8 bytes
    | Application Binary section offset (binaries_ptr)         |  96 60,  8 bytes
    | Source File section size (files_size)                    | 104 68,  8 bytes
    | Source File section offset (files_ptr)                   | 112 70,  8 bytes
    | Function section size (funcs_size)                       | 120 78,  8 bytes
    | Function section offset (funcs_ptr)                      | 128 80,  8 bytes
    |                                                          |
    |--- General Properties section ---------------------------| (general_ptr), (general_size)
    | Null-terminated database title                           |  0  0, **
    | Hierarchical Identifier Table (see below)                | ** **, **
    |                                                          |
    |--- Performance Metric section ---------------------------| (metric_ptr), metric_size)
    | Number of Performance Metrics (num_metrics)              |  0  0, 4 bytes
    | Performance Metric Specification 0 (see below)           |  4  4, **
    | Performance Metric Specification 1                       | ** **, **
    | ...                                                      | ...
    | Performance Metric Specification (num_metrics - 1)       | ** **, **
    |                                                          |
    |--- Application Binary section ---------------------------| (binaries_ptr), (binaries_size)
    | Number of Application Binaries (num_binaries)            |  0  0, 4 bytes
    | Size of Application Binary Specification (size_binary)   |  4  4, 2 bytes
    | Application Binary Specification 0 (see below)           |  6  6, (size_binary)
    | Application Binary Specification 1                       | ** **, (size_binary)
    | ...                                                      | ...
    | Application Binary Specification (num_binaries - 1)      | ** **, (size_binary)
    |                                                          |
    |--- Source File section ----------------------------------| (files_ptr), (files_size)
    | Number of Source Files (num_files)                       |  0  0, 4 bytes
    | Size of Source File Specification (size_file)            |  4  4, 2 bytes
    | Source File Specification 0 (see below)                  |  6  6, (size_file)
    | Source File Specification 1                              | ** **, (size_file)
    | ...                                                      | ...
    | Source File Specification (num_files - 1)                | ** **, (size_file)
    |                                                          |
    |--- Function section -------------------------------------| (funcs_ptr), (funcs_size)
    | Number of Functions (num_funcs)                          |  0  0, 4 bytes
    | Size of Function Specification (size_func)               |  4  4, 2 bytes
    | Function Specification 0 (see below)                     |  6  6, (size_func)
    | Function Specification 1                                 | ** **, (size_func)
    | ...                                                      | ...
    | Function Specification (num_modules-1)                   | ** **, (size_func)
    |                                                          |
    |--- Context Attribute Strings section --------------------| (cas_ptr), (cas_size)
    | Arbitrary string data                                    |
    |                                                          |
    |--- Context Tree section ---------------------------------| (ct_ptr), (ct_size)
    | Size of root Context Tree Sibling Block (block_size)     |  0  0, 8 bytes
    | Context Tree Sibling Block for roots (see below)         |  8  8, **
    |                                                          |
    | Context Tree Node                                        |
    | ...                                                      |
    |                                                          |
    |----------------------------------------------------------|
    | Magic footer ("PROFDBmn" or "nmBDFORP")                  | ** **, 8 bytes
    |----------------------------------------------------------| EOF

### Hierarchical Identifier Table ###
The Hierarchical Identifier Table provides a human-readable interpretation of
the [Hierarchical Identifier Tuple](#hierarchical-identifier-tuple) `kind`
field.

The table has the following structure:

    |-----------------------------------------------------------|
    | Number of identifier kinds (num_kinds)                    | ** **, 2 bytes
    | Null-terminated name for identifier kind 0                | ** **, **
    | Null-terminated name for identifier kind 1                | ** **, **
    | ...                                                       | ...
    | Null-terminated name for identifier kind (num_kinds - 1)  | ** **, **
    |-----------------------------------------------------------|

### Performance Metric Specification ###
Each Performance Metric Specification describes a metric that was measured at
application runtime (by `hpcrun`) and the performance analysis applied
afterwards (by `hpcprof`) for that particular metric.

Each specification has the following structure:

    |----------------------------------------------------------------------|
    | Number of propagation scopes (num_scopes)                            |  0  0, 2 bytes
    | Null-terminated identifier for the root metric                       |  2  2, **
    | Propagation scope info 0 {                                           | ** **, **
    |   Metric id for non-summary metric values in profile.db (raw_mid)    | +  0  0, 2 bytes
    |   Number of summary formulations (num_summaries)                     | +  2  2, 2 bytes
    |   Null-terminated propagation scope identifier (scope)               | +  4  4, **
    |   Summary formulation info 0 {                                       | + ** **, **
    |     Metric id for summary metric values in profile.db (summary_mid)  |   +  0  0, 2 bytes
    |     Combination formula (combine)                                    |   +  2  2, 1 byte
    |     Null-terminated identifying formula (formula)                    |   +  3  3, **
    |   }                                                                  |
    |   Summary statistic info 1                                           | + ** **, **
    |   ...                                                                | ...
    |   Summary statistic info (num_summaries - 1)                         | + ** **, **
    | }                                                                    |
    | Propagation scope info 1                                             | ** **, **
    | ...                                                                  | ...
    | Propagation scope info (num_scopes - 1)                              | ** **, **
    |----------------------------------------------------------------------|

The propagation scope identifier `(scope)` corresponds to the `inputs:scope` key
in `METRICS.yaml`. Common values are:
 - `execution`: These metric values are the inclusive cost of the root metric,
   including costs for inner and called contexts.
 - `function`: These metric values are the exclusive cost of the root metric,
   only including costs within the attributed-to function.
 - `point`: These metric values are the unpropagated costs of the root metric.

The combination formula `(combine)` is an enumeration with the following values:
 - `0`: Sum of inputs. Corresponds to `inputs:combine: sum` in `METRICS.yaml`.
 - `1`: Minimum of inputs. Corresponds to `combine: min` in `METRICS.yaml`.
 - `2`: Maximum of inputs. Corresponds to `combine: max` in `METRICS.yaml`.

The identifying formula `(formula)` corresponds to the `inputs:formula` key in
METRICS.yaml. The format is the same as `!!str` formulas in `METRICS.yaml`,
except whitespace is removed and `$$` indicates the input (propagated) value.

### Application Binary Specification ###
Each Application Binary Specification describes a binary that was used by the
application execution.

Each specification has the following structure:

    |---------------------------------------------| (size_binary) = 12 bytes
    | Binary Flags (flags)                        |  0  0, 4 bytes
    | Null-terminated filepath offset (path_ptr)  |  4  4, 8 bytes
    |---------------------------------------------|

Binary Flags `(flags)` is a bitfield reserved for future use.

`(path_ptr)` is within the Context Attributes Strings section.

### Source File Specification ###
Each Source File Specification describes an application source file, that was
compiled (if applicable) into code that was used by the application execution.

Each specification has the following structure:

    |---------------------------------------------| (size_file) = 12 bytes
    | File Flags (flags)                          |  0  0, 4 bytes
    | Null-terminated filepath offset (path_ptr)  |  4  4, 8 bytes
    |---------------------------------------------|

File Flags `(flags)` is a bitfield composed from the following values:
 - `0x1 (is_copied)`: If `1`, this file was copied into the database and is
   available under `src/<*path_ptr>`. If `0`, this file was not found by
   `hpcprof` but may be available on the user system.

`(path_ptr)` is within the Context Attributes Strings section.

### Function Specification ###
Each Function Specification describes a function-like source code construct, or
any other code region with a name an application developer would understand.

Each specification has the following structure:

    |---------------------------------------------------------------| (size_func) = 40 bytes
    | Function Flags (flags)                                        |  0  0, 4 bytes
    | 0 or Null-terminated name offset (name_ptr)                   |  4  4, 8 bytes
    | 0 or Enclosing Application Binary Spec. offset (binary_ptr)   | 12  C, 8 bytes
    | -1 or Offset in enclosing application binary (binary_offset)  | 20 14, 8 bytes
    | 0 or Defining Source File Spec. offset (def_fileptr)          | 28 1C, 8 bytes
    | 0 or Line number of definition (def_line)                     | 36 24, 4 bytes
    |---------------------------------------------------------------|

Function Flags `(flags)` is a bitfield reserved for future use.

If `(name_ptr)` is not `0`, it points within the Context Attributes Strings
section. If `(name_ptr)` is `0`, this function is anonymous (either it has no
name or its name is unknown).

If `(binary_ptr)` is not `0`, it points to an Application Binary Specification
located in the Application Binary section. If `(binary_ptr)` is `0`,
`(binary_offset)` should be ignored. If `(binary_offset)` is
`-1 == 0xFFFFFFFFFFFFFFFF` it should be ignored.

If `(def_fileptr)` is not `0`, it points to a Source File Specification located
in the Source File section. If `(def_fileptr)` is `0`, `def_line` should be
ignored. If `(def_line)` is `0` it should be ignored.

### Context Tree Sibling Block and Context Node ###
A Context Tree Sibling Block is a dense sequence of sibling Context Tree Nodes,
arranged for quick ingestion with a single read. The children of a Node are
represented by a Sibling Block. A Context Tree Node represents a single
source-level calling context within the application execution.

Each Sibling Block has the following structure:

    |--------------------| (block_size) bytes
    | Context Tree Node  |  0  0, **
    | ...                |
    |--------------------|

Each CT Node has the following structure:

    |---------------------------------------------------| 22 + (callee_size) + (lexical_size) bytes
    | Unique identifier for *.db (ctx_id)               |  0  0, 4 bytes
    | Children Sibling Block offset (block_ptr)         |  4  4, 8 bytes
    | Children Sibling Block size (block_size)          | 12  C, 8 bytes
    | Size of the Callee Specification (callee_size)    | 20 1C, 1 byte
    | Size of the Lexical Specification (lexical_size)  | 21 1D, 1 byte
    | Callee Specification                              | 22 1E, (callee_size)
    | Lexical Specification                             | ** **, (lexical_size)
    |---------------------------------------------------|

`(block_ptr)` points to a Context Tree Sibling Block of `(block_size)` bytes.

If `(callee_size)` is `0` this context was not called by its parent CT Node.
For instance, this is true for line CT Nodes within a single function.

### Callee Specification ###
If present, the Callee Specification describes how the parent CT Node called
this CT Node. The presence of and offsets for some fields varies with
`(call_flags)`.

Each callee specification has the following structure:

    |-----------------------------------------------------| (callee_size) bytes
    | Call Type (call_type) (see below)                   |  0  0, 1 byte
    | Callee Flags (call_flags) (see below)               |  1  1, 1 byte
    | if(has_caller_srcline) {                            |
    |   Caller Source File Spec. offset (caller_fileptr)  | +  0  0, 8 bytes
    |   Caller source line number (caller_line)           | +  8  8, 4 bytes
    | }                                                   |
    |-----------------------------------------------------|

The Call Type `(call_type)` is an enumeration with the following values:
 - `0`: Normal standard function call.
 - `1`: Inlined function call (call to a function inlined by a compiler).

Callee Flags `(call_flags)` is a bitfield composed of the following values:
 - `0x1 (has_caller_srcline)`: If `1`, the source location of the call in the
   caller's context is listed in this Specification. If `0`, this line must be
   derived from a parent Lexical Specification, see there for more details.

If present, `(caller_fileptr)` points to a Source File Specification located in
the Source File section. If `(caller_line)` is `0` it should be ignored.

### Lexical Specification ###
The Lexical Specification describes the context of a CT Node. The presence of
and offsets for some fields varies with `(lex_flags)`.

Each lexical specification has the following structure:

    |-------------------------------------------------| (lexical_size) bytes
    | Lexical Type (lex_type) (see below)             |  0  0, 1 byte
    | Lexical Flags (lex_flags) (see below)           |  1  1, 2 bytes
    | if(has_function) {                              |
    |   Function Spec. offset (func_ptr)              | +  0  0, 8 bytes
    | }                                               |
    | if(has_srcline) {                               |
    |   Source File Spec. offset (src_fileptr)        | +  0  0, 8 bytes
    |   Line number in source file (src_line)         | +  8  8, 4 bytes
    | }                                               |
    | if(has_point) {                                 |
    |   Application Binary Spec. offset (binary_ptr)  | +  0  0, 8 bytes
    |   Offset in application binary (binary_offset)  | +  8  8, 8 bytes
    | }                                               |
    |-------------------------------------------------|

The Lexical Type `(lex_type)` is an enumeration with the following values:
 - `0`: Function-like construct. If `(has_function)` is `1`, `(func_ptr)`
   indicates the function in question. If not, the function for this context is
   unknown.
 - `1`: Loop construction. `(src_fileptr)` and `(src_line)` indicate the source
   line with the loop header.
 - `2`: Source line. `(src_fileptr)` and `(src_line)` indicate the source line
   in question.
 - `3`: Single "point" instruction. `(binary_ptr)` and `(binary_offset)`
   indicate the first byte of the instruction in question.

Lexical Flags `(lex_flags)` is a bitfield composed of the following values:
 - `0x1 (has_function)`: If `1`, a function is listed in this Specification.
 - `0x2 (has_srcline)`: If `1`, a source line is listed in this Specification.
 - `0x4 (has_point)`: If `1`, the offset of an instruction is listed in this
   Specification.
 - `0x8 (is_call_srcline)`: If `1`, the source line indicated by `(src_fileptr)`
   and `(src_line)` can be used in place of `(caller_fileptr)` and
   `(caller_line)` if those are not present in a descendant Callee
   Specification. This only applies to the nearest non-empty Callee
   Specification, deeper calls are not affected.

`(func_ptr)`, `(src_fileptr)` and `(binary_ptr)` point within their respective
sections.

`profile.db` version 4.0
------------------------

The `profile.db` is a binary file containing the performance analysis results
generated by `hpcprof`, arranged by application thread and in a compact sparse
representation. Once an application thread is chosen, the analysis result for
a particular calling context can be obtained through a simple binary search.

The `profile.db` has the following overall structure:

    |--- Header ----------------------------------| Offset (dec hex), field size
    | Magic identifier ("HPCPROF-profdb__")       |  0  0, 16 bytes
    | Version (major, minor. Currently 4.0)       | 16 10,  2 bytes
    | Number of profiles (num_prof)               | 18 12,  4 bytes
    | Number of sections in this header (num_sec) | 22 16,  2 bytes
    | Profile Info section size (pi_size)         | 24 18,  8 bytes
    | Profile Info section offset (pi_ptr)        | 32 20,  8 bytes
    | Identifier Tuple section size (idt_size)    | 40 28,  8 bytes
    | Identifier Tuple section offset (idt_ptr)   | 48 30,  8 bytes
    |                                             |
    |--- Profile Info section --------------------| (pi_ptr), (pi_size)
    | Profile Info index 0 (see below)            |  0  0, 52 bytes
    | Profile Info index 1                        | 52 34, 52 bytes
    | ...                                         | ...
    | Profile Info index (num_prof - 1)           | ** **, 52 bytes
    |                                             |
    |--- Identifier Tuple section ----------------| (idt_ptr), (idt_size)
    | Hierarchical Identifier Tuple (see below)   |  0  0, ** bytes
    | ...                                         | ...
    |                                             |
    |--- Sparse Metric section -------------------| ** **, **
    | Profile Sparse Value Block (see below)      | ** **, **
    | ...                                         | ...
    |                                             |
    |---------------------------------------------|
    | Magic footer ("PROFDBft" or "tfBDFORP")     | ** **, 8 bytes
    |---------------------------------------------| EOF

### Profile Info ###

Each profile info has the following structure:

    |----------------------------------------------| 52 bytes
    | Identifier Tuple pointer                     |  0  0, 8 bytes
    | Metadata pointer                             |  8  8, 8 bytes
    | Spare one pointer                            | 16 10, 8 bytes
    | Spare two pointer                            | 24 18, 8 bytes
    | Number of non-zero values (num_vals)         | 32 20, 8 bytes
    | Number of non-zero contexts (num_nzctxs)     | 40 28, 4 bytes
    | Profile Sparse Value Block offset (prof_off) | 44 2C, 8 bytes
    |----------------------------------------------|

Notes:
- `Identifier Tuple pointer` points to the Identifier Tuple of this profile, 
in Identifier Tuple section. 
- `Metadata pointer`, `Sparse one pointer`, and `Sparse two pointer` are reserved 
for future usage. They are empty (0) right now. 
- The order of profile infos in this section is random, but index 0 is always
a summary profile. We call the implicit index of a profile info `prof_info_idx`.
- To access a specific profile info: `pi_ptr + 52 * prof_info_idx`.

### Hierarchical Identifier Tuple ###

Each identifier tuple has the following structure:

    |----------------------------------------------| 2 + 18*(len) bytes
    | Number of identifier elements (len)          |  0  0, 2 bytes
    |                                              |  2  2    
    | { //each identifier element                  | 
    |   Identifier kind                            | +  0  0, 2 bytes
    |   Identifier physical_value                  | +  2  2, 8 bytes
    |   Identifier logical_value                   | + 10  A, 8 bytes
    | } ...                                        |

An element's `xxx_value` is interpreted based on its `kind`, as one of:
 - `1` "Node": The individual compute node executed on. `value` is the value
   returned by a call to `gethostid()`.
 - `2` "Rank": The MPI rank assigned to the process. `values` is the assigned
   rank index.
 - `3` "Thread": Unique CPU thread identifier within the process. `value` is a
   unique thread index.
 - `4` "GPU Device":
 - `5` "GPU Context":
 - `6` "GPU Stream":
 - `7` "Core":

### Profile Sparse Value Block ###

Each sparse value block has the following structure:

    |----------------------------------------------| num_vals * 10 + ( num_nzctxs + 1 ) * 12 bytes
    | { // (non-zero value, metric id) pair        |
    |   Non-zero value                             | +  0  0, 8 bytes
    |   Metric id                                  | +  8  8, 2 bytes
    | } ...                                        |
    |                                              |
    | { // (context id, context index) pair        | 
    |   Context id                                 | +  0  0, 4 bytes
    |   Context index                              | +  4  4, 8 bytes
    | } ...                                        |
    | End marker for the last context              | + ** **, 4 bytes
    | End context index                            | + ** **, 8 bytes
    |----------------------------------------------|

For each profile:
- `(non-zero value, metric id) pair` records a non-zero data corresponding to a specific 
metric with `metric id`. 
- `(context id, context index) pair` records a calling context that has at least one non-zero
data related to. `context id` is its assigned id. `context index` is the index of the first
`(non-zero value, metric id) pair` of this calling context, in the `(non-zero value, metric id) pair` 
section.
- End marker for the last context is a special number: `0x656E6421`, indicating the end of 
`(context id, context index) pair` section.
- To access all the metrics for a specific calling context, with the context id is `c`: 
binary search `(context id, context index) pair` section, find `(c, idx)` and the following 
`next_idx`, jump to `prof_off + idx * 10` and read till `prof_off + next_idx * 10`.

`cct.db` version 4.0
--------------------

The `cct.db` is a binary file containing the performance analysis results
generated by `hpcprof`, arranged by calling context and in a compact sparse
representation. Once a calling context is chosen, the analysis result for
a particular metric can be obtained through a simple binary search.

The `cct.db` has the following overall structure:

    |--- Header ----------------------------------| Offset (dec hex), field size
    | Magic identifier ("HPCPROF-cctdb___")       |  0  0, 16 bytes
    | Version (major, minor. Currently 4.0)       | 16 10,  2 bytes
    | Number of contexts (num_ctx)                | 18 12,  4 bytes
    | Number of sections in this header (num_sec) | 22 16,  2 bytes
    | Context Info section size (ci_size)         | 24 18,  8 bytes
    | Context Info section offset (ci_ptr)        | 32 20,  8 bytes
    |                                             |
    |--- Context Info section --------------------| (ci_ptr), (ci_size)
    | Context Info index 0 (see below)            |  0  0, 22 bytes
    | Context Info index 1                        | 22 16, 22 bytes
    | ...                                         | ...
    | Context Info index (num_ctx - 1)            | ** **, 22 bytes
    |                                             |
    |--- Sparse Metric section -------------------| ** **, **
    | Context Sparse Value Block (see below)      | ** **, **
    | ...                                         | ...
    |                                             |
    |---------------------------------------------|
    | Magic footer ("CCTDBftr" or "rtfBDTCC")     | ** **, 8 bytes
    |---------------------------------------------| EOF


### Context Info ###

Each context info has the following structure:

    |----------------------------------------------| 22 bytes
    | Context id (ctx_id)                          |  0  0, 4 bytes
    | Number of non-zero values (num_vals)         |  4  4, 8 bytes
    | Number of non-zero metrics (num_nzmids)      | 12  C, 2 bytes
    | Context Sparse Value Block offset (ctx_off)  | 14  E, 8 bytes
    |----------------------------------------------|

Notes:
- To access a specific context info: `ci_ptr + 22 * ctx_id`.

### Context Sparse Value Block ###

Each sparse value block has the following structure:

    |----------------------------------------------| num_vals * 12 + ( num_nzctxs + 1 ) * 10 bytes
    | { // (non-zero value, prof_info_idx) pair    |
    |   Non-zero value                             | +  0  0, 8 bytes
    |   prof_info_idx (check Profile Info section) | +  8  8, 4 bytes
    | } ...                                        |
    |                                              |
    | { // (metric id, metric index) pair          | 
    |   Metric id                                  | +  0  0, 2 bytes
    |   Metric index                               | +  2  2, 8 bytes
    | } ...                                        |
    | End marker for the last metric               | + ** **, 2 bytes
    | End metric index                             | + ** **, 8 bytes
    |----------------------------------------------|

For each calling context:
- `(non-zero value, prof_info_idx) pair` records a non-zero data corresponding to a specific 
profile with `prof_info_idx`. 
- `(metric id, metric index) pair` records a metric that has at least one non-zero
data related to. `metric id` is its assigned id. `metric index` is the index of the first
`(non-zero value, prof_info_idx) pair` of this metric, in the `(non-zero value, prof_info_idx) pair` 
section.
- End marker for the last metric is a special number: `0x6564`, indicating the end of 
`(metric id, metric index) pair` section.
- To access the data for all the profiles for a specific metric, with the metric id is `m`: 
binary search `(metric id, metric index) pair` section, find `(m, idx)` and the following 
`next_idx`, jump to `ctx_off + idx * 10` and read till `ctx_off + next_idx * 10`.


`trace.db` version 4.0
----------------------

    |--- Header ----------------------------------| Offset (dec hex), field size
    | Magic identifier ("HPCPROF-tracedb_")       |  0  0, 16 bytes
    | Version (major, minor. Currently 4.0)       | 16 10,  2 bytes
    | Number of trace lines (num_traces)          | 18 12,  4 bytes
    | Number of sections (num_sec)                | 22 16,  2 bytes
    | Trace Header section size (hdr_size)        | 24 18,  8 bytes
    | Trace Header section offset (hdr_ptr)       | 32 20,  8 bytes
    | Minimum timestamp                           | 40 28,  8 bytes
    | Maximum timestamp                           | 48 30,  8 bytes
    |                                             |
    |--- Trace Header section --------------------| (hdr_ptr), (hdr_size)
    | Trace Header index 0 (see below)            |  0  0, 22 bytes
    | Trace Header index 1                        | 22 16, 22 bytes
    | ...                                         | ...
    | Trace Header index (num_traces - 1)         | ** **, 22 bytes
    |                                             |
    |--- Trace Line section ----------------------| ** **, **
    | Trace Line (see below)                      | ** **, **
    | ...                                         | ...
    |---------------------------------------------| EOF
    
### Trace Header ###

    |------------------------------------------------| 22 bytes
    | prof_info_idx (in profile.db)                  |   0  0, 4 bytes
    | Trace type                                     |   4  4, 2 bytes
    | Offset of Trace Line start (line_ptr)          |   6  6, 8 bytes
    | Offset of Trace Line one-after-end (line_end)  |  14  e, 8 bytes
    |------------------------------------------------|
    
A trace type can be:
 - `0` calling context type
 - `>= 1` metric type, and the number indicates metric id

### Trace Line ###

    |--------------------------------------------------| (line_end)-(line_ptr) bytes
    | Sample {                                         |  0  0
    |   Timestamp (nanoseconds since epoch)            | + 0  0, 8 bytes
    |   Sample calling context id (in experiment.xml)  | + 8  8, 4 bytes
    | } ...                                            |
