include(${CMAKE_CURRENT_LIST_DIR}/common.cmake)


if (NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
  message(FATAL_ERROR "This optimization is only for Linux!")
endif()

file(READ "/proc/cpuinfo" _cpuinfo)
set(_vendor_id)
set(_cpu_family)
set(_cpu_model)
set(_cpu_flags)
string(REGEX REPLACE "^.*vendor_id[ \t]*:[ \t]+([a-zA-Z0-9_-]+).*$" "\\1" _vendor_id "${_cpuinfo}")
string(REGEX REPLACE "^.*cpu family[ \t]*:[ \t]+([a-zA-Z0-9_-]+).*$" "\\1" _cpu_family "${_cpuinfo}")
string(REGEX REPLACE "^.*model[ \t]*:[ \t]+([a-zA-Z0-9_-]+).*$" "\\1" _cpu_model "${_cpuinfo}")
string(REGEX REPLACE "^.*flags[ \t]*:[ \t]+([^\n]+).*$" "\\1" _cpu_flags "${_cpuinfo}")

# Intel(R) Xeon(R) Gold 6134 CPU @ 3.20GHz
# Vendor: GenuineIntel
# Family: 6
# Model:  85
# Flags:  fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm pbe syscall nx pdpe1gb rdtscp lm constant_tsc art arch_perfmon pebs bts rep_good nopl xtopology nonstop_tsc aperfmperf eagerfpu pni pclmulqdq dtes64 monitor ds_cpl vmx smx est tm2 ssse3 sdbg fma cx16 xtpr pdcm pcid dca sse4_1 sse4_2 x2apic movbe popcnt tsc_deadline_timer aes xsave avx f16c rdrand lahf_lm abm 3dnowprefetch epb cat_l3 cdp_l3 invpcid_single intel_ppin intel_pt ssbd mba ibrs ibpb stibp tpr_shadow vnmi flexpriority ept vpid fsgsbase tsc_adjust bmi1 hle avx2 smep bmi2 erms invpcid rtm cqm mpx rdt_a avx512f avx512dq rdseed adx smap clflushopt clwb avx512cd avx512bw avx512vl xsaveopt xsavec xgetbv1 cqm_llc cqm_occup_llc cqm_mbm_total cqm_mbm_local dtherm ida arat pln pts pku ospke md_clear spec_ctrl intel_stibp flush_l1d

# AMD EPYC 7R32
# Vendor: AuthenticAMD
# Family: 23
# Model:  49
# Flags:  fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush mmx fxsr sse sse2 ht syscall nx mmxext fxsr_opt pdpe1gb rdtscp lm constant_tsc rep_good nopl nonstop_tsc cpuid extd_apicid aperfmperf tsc_known_freq pni pclmulqdq ssse3 fma cx16 sse4_1 sse4_2 movbe popcnt aes xsave avx f16c rdrand hypervisor lahf_lm cmp_legacy cr8_legacy abm sse4a misalignsse 3dnowprefetch topoext ssbd ibrs ibpb stibp vmmcall fsgsbase bmi1 avx2 smep bmi2 rdseed adx smap clflushopt clwb sha_ni xsaveopt xsavec xgetbv1 clzero xsaveerptr wbnoinvd arat npt nrip_save rdpid

# message(FATAL_ERROR "V:${_vendor_id}\n F:${_cpu_family}\n M:${_cpu_model}\n L:${_cpu_flags}")

# Flags used by the compiler during builds
SET(COMPILER_FLAGS "-O2 -DNDEBUG -DDBUG_OFF")

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  if (_vendor_id STREQUAL "GenuineIntel")
    set(COMPILER_FLAGS "-mbranches-within-32B-boundaries ${COMPILER_FLAGS}")
  elseif (_vendor_id STREQUAL "AuthenticAMD")
    set(COMPILER_FLAGS "-march=znver2 -mtune=znver2 ${COMPILER_FLAGS}")
  endif()
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  if (_vendor_id STREQUAL "GenuineIntel")
    set(COMPILER_FLAGS "-Wa,-mbranches-within-32B-boundaries ${COMPILER_FLAGS}")
  elseif (_vendor_id STREQUAL "AuthenticAMD")
    set(COMPILER_FLAGS "-Wa,-march=znver2 -Wa,-mtune=znver2 ${COMPILER_FLAGS}")
  endif()
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Intel")
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  message(FATAL_ERROR "Unwanted compiler!")
endif()

# Flags used by the linker during builds
SET(LINKER_FLAGS "-z relro -z now ${LINKER_FLAGS}")


# Default type is BOOL. Must specify type for non-yet-existent cached variables.
# This could be much better if normal variables worked like -D
setc(DISABLE_SHARED OFF)
setc(ENABLED_PROFILING OFF)
setc(ENABLE_DTRACE OFF)
setc(MAX_INDEXES 128 STRING)
setc(MUTEXTYPE futex STRING)
setc(SECURITY_HARDENED OFF)
setc(MYSQL_MAINTAINER_MODE NO STRING)
setc(CONC_WITH_DYNCOL NO)
setc(CONC_WITH_EXTERNAL_ZLIB NO)
setc(CONC_WITH_MYSQLCOMPAT NO)
setc(CONC_WITH_UNIT_TESTS NO)
setc(GSSAPI_FOUND FALSE)
setc(UPDATE_SUBMODULES OFF)
setc(USE_ARIA_FOR_TMP_TABLES OFF)
setc(WITH_DBUG_TRACE OFF)
setc(WITH_WSREP OFF)
setc(WITH_NUMA OFF STRING)
setc(WITH_SAFEMALLOC OFF)
setc(WITH_UNIT_TESTS OFF)

set(plugins_to_exclude
  PLUGIN_CONNECT
  PLUGIN_OQGRAPH
  PLUGIN_MROONGA
  PLUGIN_TOKUDB
  PLUGIN_SPIDER
  PLUGIN_SPHINX
  PLUGIN_ROCKSDB
  PLUGIN_SEMISYNC_MASTER
  PLUGIN_SEMISYNC_SLAVE
  PLUGIN_AUDIT_NULL
  PLUGIN_AUTH_0X0100
  PLUGIN_AUTH_TEST_PLUGIN
  PLUGIN_BLACKHOLE
  PLUGIN_DAEMON_EXAMPLE
  PLUGIN_DEBUG_KEY_MANAGEMENT
  PLUGIN_DIALOG_EXAMPLES
  PLUGIN_DISKS
  PLUGIN_EXAMPLE
  PLUGIN_EXAMPLE_KEY_MANAGEMENT
  PLUGIN_FEDERATED
  PLUGIN_FEDERATEDX
  PLUGIN_FEEDBACK
  PLUGIN_FTEXAMPLE
  PLUGIN_QA_AUTH_CLIENT
  PLUGIN_QA_AUTH_INTERFACE
  PLUGIN_QA_AUTH_SERVER
  PLUGIN_TEST_SQL_DISCOVERY
  PLUGIN_TEST_VERSIONING
)

set_compiler_and_linker_flags("SHOGUN" ${COMPILER_FLAGS} ${LINKER_FLAGS})
exclude_plugins("${plugins_to_exclude}")
make_plugins_static_if_shared_libs_are_disabled()