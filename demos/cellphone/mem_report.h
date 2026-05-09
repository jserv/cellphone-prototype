/**
 * @file mem_report.h
 *
 * Public entry point for the cellphone demo's heap-breakdown
 * diagnostic.  Walks the live LVGL screen tree plus the builtin TLSF
 * pool and prints a per-class object/style census, a block-size
 * histogram, and the top-N largest live blocks.
 *
 * Build / link:
 *   - Compile mem_report.c into the test (or any other) host with
 *     -DCELLPHONE_TEST_REPORT.  Without the gate the translation unit
 *     collapses to nothing, keeping printf/qsort and the private-header
 *     coupling out of the default lvgl_demos library build.
 *   - Requires LV_USE_STDLIB_MALLOC == LV_STDLIB_BUILTIN (the report
 *     walks the builtin TLSF pool).  mem_report.c #errors otherwise.
 *
 * Usage from a custom host (after the workload has reached its
 * steady state):
 *
 *     #include "demos/cellphone/mem_report.h"
 *
 *     run_my_workload();
 *     cellphone_mem_report();   // prints to stdout
 *
 * The cellphone test driver wires this up automatically when invoked
 * via `demos/cellphone/build.sh report`.
 */

#ifndef CELLPHONE_MEM_REPORT_H
#define CELLPHONE_MEM_REPORT_H

#ifdef __cplusplus
extern "C" {
#endif

/** Print a steady-state heap breakdown to stdout.  Defined only when
 *  the translation unit is compiled with -DCELLPHONE_TEST_REPORT;
 *  callers that link against an unflagged build will fail at link
 *  time, which is the intended signal that the diagnostic was not
 *  compiled in. */
void cellphone_mem_report(void);

#ifdef __cplusplus
}
#endif

#endif /* CELLPHONE_MEM_REPORT_H */
