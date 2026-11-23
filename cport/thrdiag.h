/* PLAN-threads step 0 (2026-11-23): time accounting for the layout, read by pipeline.c (BBB_THREADS_DIAG) */
typedef struct { double setup, make, final_, pre, shells_s3, shells_smooth, shells_ork, chain_max, d2; long finds, tries, accepted, acc_hist[12], chains; } ThrDiag;
extern ThrDiag thr_diag;
double thr_ms(void);
