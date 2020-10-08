#ifndef PM_LFB_IOCTL
#define PM_LFB_IOCTL

#include <linux/ioctl.h>

#define USER_MODE_BIT 16
#define OS_MODE_BIT 17
#define ENABLE_COUNTERS_BIT 22

#define USER_MODE_TRUE (0x1 << USER_MODE_BIT)
#define USER_MODE_FALSE (0x0 << USER_MODE_BIT)
#define OS_MODE_TRUE (0x1 << OS_MODE_BIT)
#define OS_MODE_FALSE (0x0 << OS_MODE_BIT)
#define ENABLE_COUNTERS_TRUE (0x1 << ENABLE_COUNTERS_BIT)
#define ENABLE_COUNTERS_FALSE (0x0 << ENABLE_COUNTERS_BIT)

#define COUNTER_PER_CORE 0x8 // Coffee Lake
#define IA32_PMCx 0x0C1
#define IA32_PERFEVTSELx 0x186

#define L1D_PEND_MISS_FB_FULL ((0x02 << 8) + 0x48) 
#define LOAD_HIT_PRE_HW_PF ((0x01 << 8) + 0x4C)
#define MEM_LOAD_RETIRED_FB_HIT ((0x40 << 8) + 0xD1)
#define TX_MEM_ABORT_CONFLICT ((0x01 << 8) + 0x54)
#define TX_MEM_ABORT_CAPACITY_WRITE ((0x02 << 8) + 0x54) // Coffee Lake
#define RTM_RETIRED_START ((0x01 << 8) + 0xC9)
#define RTM_RETIRED_ABORTED ((0x04 << 8) + 0xC9)
#define RTM_RETIRED_ABORTED_MEM ((0x08 << 8) + 0xC9)

#define x_L1D_PEND_MISS_FB_FULL 0x0
#define x_LOAD_HIT_PRE_HW_PF 0x1
#define x_MEM_LOAD_RETIRED_FB_HIT 0x2
#define x_TX_MEM_ABORT_CONFLICT 0x3
#define x_TX_MEM_ABORT_CAPACITY_WRITE 0x04
#define x_RTM_RETIRED_START 0x5
#define x_RTM_RETIRED_ABORTED 0x6
#define x_RTM_RETIRED_ABORTED_MEM 0x7


#define PM_LFB_IOCTL_MAGIC            'L'
#define PM_LFB_IOCTL_ENABLE_ALL_PERFEVTS  _IO(PM_LFB_IOCTL_MAGIC, 0)
#define PM_LFB_IOCTL_DISABLE_ALL_PERFEVTS _IO(PM_LFB_IOCTL_MAGIC, 1)
#define PM_LFB_IOCTL_READ_ALL_PMCS        _IOWR(PM_LFB_IOCTL_MAGIC, 2, \
    pmc_results_arr_t)
#define PM_LFB_IOCTL_RESET_ALL_PMCS       _IO(PM_LFB_IOCTL_MAGIC, 3)

typedef unsigned long pmc_results_arr_t[COUNTER_PER_CORE];

#endif