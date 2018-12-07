/* 
 * This file is part of the Nautilus AeroKernel developed
 * by the Hobbes and V3VEE Projects with funding from the 
 * United States National  Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  The Hobbes Project is a collaboration
 * led by Sandia National Laboratories that includes several national 
 * laboratories and universities. You can find out more at:
 * http://www.v3vee.org  and
 * http://xstack.sandia.gov/hobbes
 *
 * Copyright (c) 2018, Kyle Hale
 * Copyright (c) 2018, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Authors: Brian Richard Tauro <btauro@hawk.iit.edu>
 *          Piyush Nath <piyush.nath.2272@gmail.com>
 * 	        Kyle Hale <khale@cs.iit.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */

#ifndef __MLX3_IB_H__
#define __MLX3_IB_H__

#define MANAGEMENT_QP   0
#define SERVICE_QP      1
#define NUM_QPS 16
#define ICM_TABLE_CHUNK_SIZE 1 << 18
#define NUM_CQS  1
#define NUM_EQS  1
#define ETH_ALEN        6 
#define INFO(fmt, args...)     INFO_PRINT("MLX3: " fmt, ##args)
#define DEBUG(fmt, args...)    DEBUG_PRINT("MLX3: " fmt, ##args)
#define ERROR(fmt, args...)    ERROR_PRINT("MLX3: " fmt, ##args)
#define PAGE_ALIGN(addr)        ALIGN(addr, PAGE_SIZE_4KB)
#define CQ_DB_SIZE              8
#define QP_DB_SIZE              4
/* Note that MMIO accesses must be 4-byte aligned according to PRM */
#define READ_MEM(d, o)         (*((volatile uint32_t*)(((d)->mmio_start)+(o))))
#define WRITE_MEM(d, o, v)     ((*((volatile uint32_t*)(((d)->mmio_start)+(o))))=(v))
#define WRITE_UAR(d, o, v)     ((*((volatile uint32_t*)(((d)->uar_start)+(o))))=(v))
#define WRITE_IRQ(d, o, v)     ((*((volatile uint32_t*)(((d+(o))))))=(v))

#define LRH_LEN   8
#define BTH_LEN  12
#define DETH_LEN  8
#define RETH_LEN  16 
#define ICRC_LEN  4
#define VCRC_LEN  2


#define SQ_STRIDE_DEFAULT 64
#define RQ_STRIDE_DEFAULT 16
#define QKEY_DEFAULT        0x0000000011111111
#define PORT_DEFAULT        1 
#define DQPN_DEFAULT        64 
#define SQ_SIZE_DEFAULT     2048 
#define RQ_SIZE_DEFAULT     2048 
#define MLX_TRANSPORT 0x7
#define ENABLE_RX 1
#define ENABLE_TX 1
#define DISABLE_RX 0
#define DISABLE_TX 0


#define MLX3_FLAG2_V_IGNORE_FCS_MASK            BIT(1)
#define MLX3_FLAG2_V_USER_MTU_MASK              BIT(5)
#define MLX3_FLAG2_V_USER_MAC_MASK              BIT(6)
#define MLX3_FLAG_V_MTU_MASK                    BIT(0)
#define MLX3_FLAG_V_PPRX_MASK                   BIT(1)
#define MLX3_FLAG_V_PPTX_MASK                   BIT(2)
#define MLX3_IGNORE_FCS_MASK                    0x1
#define MLX3_EN_10GE_MASK                       BIT(3)
#define MLX3_TC_MAX_NUMBER                      BIT(8)
#define MLX3_EN_10GE                            1 << 7
#define SET_PORT_GEN_ALL_VALID  (MLX3_FLAG_V_MTU_MASK   | \
                                 MLX3_FLAG_V_PPRX_MASK  | \
                                 MLX3_FLAG_V_PPTX_MASK)
#define SET_PORT_GEN_MTU_ETH_VALID  (MLX3_FLAG_V_MTU_MASK) |  (MLX3_EN_10GE_MASK)


/* for command input and output mailboxes */
#define MLX3_GETL(base, off) bswap32(*(uint32_t*)((base) + (off)))
#define MLX3_SETL(base, off, val) *(uint32_t*)((base) + (off)) = bswap32((val))
#define DEFAULT_UAR_PAGE_SHIFT  12
#define  INIT_HCA_VERSION 2
#define DS_SIZE         sizeof(struct mlx3_wqe_data_seg)
#define MLX3_EN_BIT_DESC_OWN    0x80000000
#define CTRL_SIZE       sizeof(struct mlx3_wqe_ctrl_seg)
#define MLX3_EN_MEMTYPE_PAD     0x100

#define STAMP_SHIFT             31
#define MLX3_EN_DEF_RX_RING_SIZE        2 
#define MLX3_EN_DEF_TX_RING_SIZE        MLX3_EN_DEF_RX_RING_SIZE
#define ROUNDUP_LOG2(x)         ilog2(roundup_pow_of_two(x))

#define MLX3_SEND_DOORBELL    0x14
#define MLX3_CQ_DOORBELL      0x20

#define EQE_SIZE 32 
#define EQ_STATUS 0
#define EQ_ARMED 0x9


/* Management classes */
#define IB_MGMT_CLASS_SUBN_LID_ROUTED       0x01
/* Management methods */
#define IB_MGMT_METHOD_GET          0x01


/* Subnet management attributes */
#define IB_SMP_ATTR_NOTICE          bswap16(0x0002)
#define IB_SMP_ATTR_NODE_DESC           bswap16(0x0010)
#define IB_SMP_ATTR_NODE_INFO           bswap16(0x0011)
#define IB_SMP_ATTR_SWITCH_INFO         bswap16(0x0012)
#define IB_SMP_ATTR_GUID_INFO           bswap16(0x0014)
#define IB_SMP_ATTR_PORT_INFO           bswap16(0x0015)
#define IB_SMP_ATTR_PKEY_TABLE          bswap16(0x0016)
#define IB_SMP_ATTR_SL_TO_VL_TABLE      bswap16(0x0017)
#define IB_SMP_ATTR_VL_ARB_TABLE        bswap16(0x0018)
#define IB_SMP_ATTR_LINEAR_FORWARD_TABLE    bswap16(0x0019)
#define IB_SMP_ATTR_RANDOM_FORWARD_TABLE    bswap16(0x001A)
#define IB_SMP_ATTR_MCAST_FORWARD_TABLE     bswap16(0x001B)
#define IB_SMP_ATTR_SM_INFO         bswap16(0x0020)
#define IB_SMP_ATTR_VENDOR_DIAG         bswap16(0x0030)
#define IB_SMP_ATTR_LED_INFO            bswap16(0x0031)
#define IB_SMP_ATTR_VENDOR_MASK         bswap16(0xFF00)
#define IB_DEVICE_NODE_DESC_MAX         64


#define VLAN_HLEN   4       /* The additional bytes required by VLAN
                     * (in addition to the Ethernet header)
                     */

#define ETH_HLEN    14      /* Total octets in header.   */
#define DEFAULT_NUM_CQE 256 
#define ETH_FCS_LEN 4       /* Octets in the FCS         */
/* VLAN_HLEN is added twice,to support skb vlan tagged with multiple
 * headers. (For example: ETH_P_8021Q and ETH_P_8021AD).
 */
#define MLX3_EN_EFF_MTU(mtu)    ((mtu) + ETH_HLEN + (2 * VLAN_HLEN))


#define CQE_SIZE 32

#define IRQ_NUMBER   11

/* utility functions... the card is big-endian */

#define __bswap16(x) ( ((x) & 0xffu) << 8 | \
        (((x) >> 8) & 0xffu) )

#define __bswap32(x) ( ((x) & 0xffu) << 24 | \
		(((x) >> 8) & 0xffu)  << 16 | \
		(((x) >> 16) & 0xffu) << 8 | \
		(((x) >> 24) & 0xffu) )

#define __bswap64(x) ( ((x) & 0xffull) << 56 | \
		(((x) >> 8) & 0xffull)  << 48 | \
		(((x) >> 16) & 0xffull) << 40 | \
		(((x) >> 24) & 0xffull) << 32 | \
		(((x) >> 32) & 0xffull) << 24 | \
		(((x) >> 40) & 0xffull) << 16 | \
		(((x) >> 48) & 0xffull) << 8 | \
		(((x) >> 56) & 0xffull) )

#define MLX3_VENDORID 0x15B3
#define MLX3_DEV_ID   0x1003


/* MMIO */
#define MLX_HCR_BASE 0x80680
#define MLX_OWNER_BASE 0x8069c
#define MLX_PPF_SEL_BASE 0x8069C

/* reset registers */
#define MLX3_RESET_BASE 0xf0000
#define MLX3_RESET_VAL  __builtin_bswap32(1U)
#define MLX3_RESET_OFF  0x10
#define MLX3_SEM_OFF    0x3fc

/* HCR (command register) layout */
#define HCR_IN_PARM_OFFSET  0x00
#define HCR_IN_MOD_OFFSET   0x08
#define HCR_OUT_PARM_OFFSET 0x0C
#define HCR_TOKEN_OFFSET    0x14
#define HCR_STATUS_OFFSET   0x18

#define HCR_OPMOD_SHIFT 12

#define HCR_T_BIT  21
#define HCR_E_BIT  22
#define HCR_GO_BIT 23

#define HCR_STATUS_SHIFT 24

/* Initialization and General Commands */
#define CMD_QUERY_DEV_CAP    0x03
#define CMD_QUERY_FW         0x04
#define CMD_QUERY_ADAPTER    0x06
#define CMD_INIT_HCA         0x07
#define CMD_CLOSE_HCA        0x08
#define CMD_INIT_PORT        0x09
#define CMD_CLOSE_PORT       0x0A
#define CMD_QUERY_HCA        0x0B
#define CMD_SET_PORT         0x0C
#define CMD_SW2HW_MPT        0x0D
#define CMD_QUERY_MPT        0x0E
#define CMD_HW2SW_MPT        0x0F
#define CMD_READ_MTT         0x10
#define CMD_WRITE_MTT        0x11
#define CMD_MAP_EQ           0x12
#define CMD_SW2HW_EQ         0x13
#define CMD_HW2SW_EQ         0x14
#define CMD_QUERY_EQ         0x15
#define CMD_SW2HW_CQ         0x16
#define CMD_HW2SW_CQ         0x17
#define CMD_QUERY_CQ         0x18
#define CMD_CONF_SPECIAL_QP  0x23
#define CMD_NOP              0x31
#define CMD_QUERY_PORT       0x43
#define CMD_DUMP_ETH_STAT    0x49
#define CMD_RUN_FW           0xFF6
#define CMD_MAP_ICM          0xFFA
#define CMD_SET_ICM_SIZE     0xFFD
#define CMD_MAP_FA           0xFFF
#define CMD_MAP_ICM_AUX      0xFFC
#define CMD_MAD_IFC          0x24
#define NA 0
#define Y  1

/* "command execution time should be set to 60 sec max" */
#define CMD_TIME_CLASS_A 10000
#define CMD_POLL_TOKEN 0xffff


#define INIT_HCA_IN_SIZE                 0x200
#define INIT_HCA_VERSION_OFFSET          0x000
#define INIT_HCA_VERSION                2
#define INIT_HCA_VXLAN_OFFSET            0x0c
#define INIT_HCA_CACHELINE_SZ_OFFSET     0x0e
#define INIT_HCA_FLAGS_OFFSET            0x014
#define INIT_HCA_RECOVERABLE_ERROR_EVENT_OFFSET 0x018
#define INIT_HCA_QPC_OFFSET              0x020
#define INIT_HCA_QPC_BASE_OFFSET        (INIT_HCA_QPC_OFFSET + 0x10)
#define INIT_HCA_LOG_QP_OFFSET          (INIT_HCA_QPC_OFFSET + 0x17)
#define INIT_HCA_SRQC_BASE_OFFSET       (INIT_HCA_QPC_OFFSET + 0x28)
#define INIT_HCA_LOG_SRQ_OFFSET         (INIT_HCA_QPC_OFFSET + 0x2f)
#define INIT_HCA_CQC_BASE_OFFSET        (INIT_HCA_QPC_OFFSET + 0x30)
#define INIT_HCA_LOG_CQ_OFFSET          (INIT_HCA_QPC_OFFSET + 0x37)
#define INIT_HCA_EQE_CQE_OFFSETS        (INIT_HCA_QPC_OFFSET + 0x38)
#define INIT_HCA_EQE_CQE_STRIDE_OFFSET  (INIT_HCA_QPC_OFFSET + 0x3b)
#define INIT_HCA_ALTC_BASE_OFFSET       (INIT_HCA_QPC_OFFSET + 0x40)
#define INIT_HCA_AUXC_BASE_OFFSET       (INIT_HCA_QPC_OFFSET + 0x50)
#define INIT_HCA_EQC_BASE_OFFSET        (INIT_HCA_QPC_OFFSET + 0x60)
#define INIT_HCA_LOG_EQ_OFFSET          (INIT_HCA_QPC_OFFSET + 0x67)
#define INIT_HCA_NUM_SYS_EQS_OFFSET     (INIT_HCA_QPC_OFFSET + 0x6a)
#define INIT_HCA_RDMARC_BASE_OFFSET     (INIT_HCA_QPC_OFFSET + 0x70)
#define INIT_HCA_LOG_RD_OFFSET          (INIT_HCA_QPC_OFFSET + 0x77)
#define INIT_HCA_MCAST_OFFSET            0x0c0
#define INIT_HCA_MC_BASE_OFFSET         (INIT_HCA_MCAST_OFFSET + 0x00)
#define INIT_HCA_LOG_MC_ENTRY_SZ_OFFSET (INIT_HCA_MCAST_OFFSET + 0x12)
#define INIT_HCA_LOG_MC_HASH_SZ_OFFSET  (INIT_HCA_MCAST_OFFSET + 0x16)
#define INIT_HCA_UC_STEERING_OFFSET     (INIT_HCA_MCAST_OFFSET + 0x18)
#define INIT_HCA_LOG_MC_TABLE_SZ_OFFSET (INIT_HCA_MCAST_OFFSET + 0x1b)
#define INIT_HCA_DEVICE_MANAGED_FLOW_STEERING_EN       0x6
#define INIT_HCA_FS_PARAM_OFFSET         0x1d0
#define INIT_HCA_FS_BASE_OFFSET          (INIT_HCA_FS_PARAM_OFFSET + 0x00)
#define INIT_HCA_FS_LOG_ENTRY_SZ_OFFSET  (INIT_HCA_FS_PARAM_OFFSET + 0x12)
#define INIT_HCA_FS_A0_OFFSET            (INIT_HCA_FS_PARAM_OFFSET + 0x18)
#define INIT_HCA_FS_LOG_TABLE_SZ_OFFSET  (INIT_HCA_FS_PARAM_OFFSET + 0x1b)
#define INIT_HCA_FS_ETH_BITS_OFFSET      (INIT_HCA_FS_PARAM_OFFSET + 0x21)
#define INIT_HCA_FS_ETH_NUM_ADDRS_OFFSET (INIT_HCA_FS_PARAM_OFFSET + 0x22)
#define INIT_HCA_FS_IB_BITS_OFFSET       (INIT_HCA_FS_PARAM_OFFSET + 0x25)
#define INIT_HCA_FS_IB_NUM_ADDRS_OFFSET  (INIT_HCA_FS_PARAM_OFFSET + 0x26)
#define INIT_HCA_TPT_OFFSET              0x0f0
#define INIT_HCA_DMPT_BASE_OFFSET       (INIT_HCA_TPT_OFFSET + 0x00)
#define INIT_HCA_TPT_MW_OFFSET          (INIT_HCA_TPT_OFFSET + 0x08)
#define INIT_HCA_LOG_MPT_SZ_OFFSET      (INIT_HCA_TPT_OFFSET + 0x0b)
#define INIT_HCA_MTT_BASE_OFFSET        (INIT_HCA_TPT_OFFSET + 0x10)
#define INIT_HCA_CMPT_BASE_OFFSET       (INIT_HCA_TPT_OFFSET + 0x18)
#define INIT_HCA_UAR_OFFSET              0x120
#define INIT_HCA_LOG_UAR_SZ_OFFSET      (INIT_HCA_UAR_OFFSET + 0x0a)
#define INIT_HCA_UAR_PAGE_SZ_OFFSET     (INIT_HCA_UAR_OFFSET + 0x08)

// Error Codes
#define EPERM            1      /* Operation not permitted */
#define ENOENT           2      /* No such file or directory */
#define ESRCH            3      /* No such process */
#define EINTR            4      /* Interrupted system call */
#define EIO              5      /* I/O error */
#define ENXIO            6      /* No such device or address */
#define E2BIG            7      /* Argument list too long */
#define ENOEXEC          8      /* Exec format error */
#define EBADF            9      /* Bad file number */
#define ECHILD          10      /* No child processes */
#define EAGAIN          11      /* Try again */
#define ENOMEM          12      /* Out of memory */
#define EACCES          13      /* Permission denied */
#define EFAULT          14      /* Bad address */
#define ENOTBLK         15      /* Block device required */
#define EBUSY           16      /* Device or resource busy */
#define EEXIST          17      /* File exists */
#define EXDEV           18      /* Cross-device link */
#define ENODEV          19      /* No such device */
#define ENOTDIR         20      /* Not a directory */
#define EISDIR          21      /* Is a directory */
#define EINVAL          22      /* Invalid argument */
#define ENFILE          23      /* File table overflow */
#define EMFILE          24      /* Too many open files */
#define ENOTTY          25      /* Not a typewriter */
#define ETXTBSY         26      /* Text file busy */
#define EFBIG           27      /* File too large */
#define ENOSPC          28      /* No space left on device */
#define ESPIPE          29      /* Illegal seek */
#define EROFS           30      /* Read-only file system */
#define EMLINK          31      /* Too many links */
#define EPIPE           32      /* Broken pipe */
#define EDOM            33      /* Math argument out of domain of func */
#define ERANGE          34      /* Math result not representable */


#define ARRAY_SIZE(a)    (sizeof(a) / sizeof(a[0]))


#define LOG_TXBB_SIZE           6
#define MLX3_FW_VER_WQE_CTRL_NEC mlx4_fw_ver(2, 2, 232)


// Device capabilities register offsets
#define QUERY_DEV_CAP_OUT_SIZE                        0x100
#define QUERY_DEV_CAP_MAX_SRQ_SZ_OFFSET               0x10
#define QUERY_DEV_CAP_MAX_QP_SZ_OFFSET                0x11
#define QUERY_DEV_CAP_MAX_EQ_SZ_OFFSET                0x1c
#define QUERY_DEV_CAP_RSVD_QP_OFFSET                  0x12
#define QUERY_DEV_CAP_MAX_QP_OFFSET                   0x13
#define QUERY_DEV_CAP_RSVD_SRQ_OFFSET                 0x14
#define QUERY_DEV_CAP_MAX_SRQ_OFFSET                  0x15
#define QUERY_DEV_CAP_RSVD_EEC_OFFSET                 0x16
#define QUERY_DEV_CAP_MAX_EEC_OFFSET                  0x17
#define QUERY_DEV_CAP_NUM_RSVD_EQ_OFFSET              0x18
#define QUERY_DEV_CAP_MAX_CQ_SZ_OFFSET                0x19
#define QUERY_DEV_CAP_RSVD_CQ_OFFSET                  0x1a
#define QUERY_DEV_CAP_MAX_CQ_OFFSET                   0x1b
#define QUERY_DEV_CAP_MAX_MPT_OFFSET                  0x1d
#define QUERY_DEV_CAP_RSVD_EQ_OFFSET                  0x1e
#define QUERY_DEV_CAP_MAX_EQ_OFFSET                   0x1f
#define QUERY_DEV_CAP_RSVD_MTT_OFFSET                 0x20
#define QUERY_DEV_CAP_MAX_MRW_SZ_OFFSET               0x21
#define QUERY_DEV_CAP_RSVD_MRW_OFFSET                 0x22
#define QUERY_DEV_CAP_MAX_MTT_SEG_OFFSET              0x23
#define QUERY_DEV_CAP_NUM_SYS_EQ_OFFSET               0x26 //Not present in mlx3
#define QUERY_DEV_CAP_MAX_AV_OFFSET                   0x27
#define QUERY_DEV_CAP_MAX_REQ_QP_OFFSET               0x29
#define QUERY_DEV_CAP_MAX_RES_QP_OFFSET               0x2b
#define QUERY_DEV_CAP_MAX_GSO_OFFSET                  0x2d
#define QUERY_DEV_CAP_RSS_OFFSET                      0x2e
#define QUERY_DEV_CAP_MAX_RDMA_OFFSET                 0x2f
#define QUERY_DEV_CAP_RSZ_SRQ_OFFSET                  0x33
#define QUERY_DEV_CAP_PORT_BEACON_OFFSET              0x34
#define QUERY_DEV_CAP_ACK_DELAY_OFFSET                0x35
#define QUERY_DEV_CAP_MTU_WIDTH_OFFSET                0x36
#define QUERY_DEV_CAP_VL_PORT_OFFSET                  0x37
#define QUERY_DEV_CAP_MAX_MSG_SZ_OFFSET               0x38
#define QUERY_DEV_CAP_MAX_GID_OFFSET                  0x3b
#define QUERY_DEV_CAP_RATE_SUPPORT_OFFSET             0x3c
#define QUERY_DEV_CAP_CQ_TS_SUPPORT_OFFSET            0x3e
#define QUERY_DEV_CAP_MAX_PKEY_OFFSET                 0x3f
#define QUERY_DEV_CAP_EXT_FLAGS_OFFSET                0x40
#define QUERY_DEV_CAP_WOL_OFFSET                      0x43
#define QUERY_DEV_CAP_FLAGS_OFFSET                    0x44
#define QUERY_DEV_CAP_RSVD_UAR_OFFSET                 0x48
#define QUERY_DEV_CAP_UAR_SZ_OFFSET                   0x49
#define QUERY_DEV_CAP_PAGE_SZ_OFFSET                  0x4b
#define QUERY_DEV_CAP_BF_OFFSET                       0x4c
#define QUERY_DEV_CAP_LOG_BF_REG_SZ_OFFSET            0x4d
#define QUERY_DEV_CAP_LOG_MAX_BF_REGS_PER_PAGE_OFFSET 0x4e
#define QUERY_DEV_CAP_LOG_MAX_BF_PAGES_OFFSET         0x4f
#define QUERY_DEV_CAP_MAX_SG_SQ_OFFSET                0x51
#define QUERY_DEV_CAP_MAX_DESC_SZ_SQ_OFFSET           0x52
#define QUERY_DEV_CAP_MAX_SG_RQ_OFFSET                0x55
#define QUERY_DEV_CAP_MAX_DESC_SZ_RQ_OFFSET           0x56
#define QUERY_DEV_CAP_USER_MAC_EN_OFFSET              0x5C
#define QUERY_DEV_CAP_SVLAN_BY_QP_OFFSET              0x5D
#define QUERY_DEV_CAP_MAX_QP_MCG_OFFSET               0x61
#define QUERY_DEV_CAP_RSVD_MCG_OFFSET                 0x62
#define QUERY_DEV_CAP_MAX_MCG_OFFSET                  0x63
#define QUERY_DEV_CAP_RSVD_PD_OFFSET                  0x64
#define QUERY_DEV_CAP_MAX_PD_OFFSET                   0x65
#define QUERY_DEV_CAP_RSVD_XRC_OFFSET                 0x66
#define QUERY_DEV_CAP_MAX_XRC_OFFSET                  0x67
#define QUERY_DEV_CAP_MAX_COUNTERS_OFFSET             0x68
#define QUERY_DEV_CAP_PORT_FLOWSTATS_COUNTERS_OFFSET  0x70
#define QUERY_DEV_CAP_EXT_2_FLAGS_OFFSET              0x70
#define QUERY_DEV_CAP_FLOW_STEERING_IPOIB_OFFSET      0x74
#define QUERY_DEV_CAP_FLOW_STEERING_RANGE_EN_OFFSET   0x76
#define QUERY_DEV_CAP_FLOW_STEERING_MAX_QP_OFFSET     0x77
#define QUERY_DEV_CAP_SL2VL_EVENT_OFFSET              0x78
#define QUERY_DEV_CAP_CQ_EQ_CACHE_LINE_STRIDE         0x7a
#define QUERY_DEV_CAP_ECN_QCN_VER_OFFSET              0x7b
#define QUERY_DEV_CAP_RDMARC_ENTRY_SZ_OFFSET          0x80
#define QUERY_DEV_CAP_QPC_ENTRY_SZ_OFFSET             0x82
#define QUERY_DEV_CAP_AUX_ENTRY_SZ_OFFSET             0x84
#define QUERY_DEV_CAP_ALTC_ENTRY_SZ_OFFSET            0x86
#define QUERY_DEV_CAP_EQC_ENTRY_SZ_OFFSET             0x88
#define QUERY_DEV_CAP_CQC_ENTRY_SZ_OFFSET             0x8a
#define QUERY_DEV_CAP_SRQ_ENTRY_SZ_OFFSET             0x8c
#define QUERY_DEV_CAP_C_MPT_ENTRY_SZ_OFFSET           0x8e
#define QUERY_DEV_CAP_MTT_ENTRY_SZ_OFFSET             0x90
#define QUERY_DEV_CAP_D_MPT_ENTRY_SZ_OFFSET           0x92
#define QUERY_DEV_CAP_BMME_FLAGS_OFFSET               0x94
#define QUERY_DEV_CAP_CONFIG_DEV_OFFSET               0x94
#define QUERY_DEV_CAP_PHV_EN_OFFSET                   0x96
#define QUERY_DEV_CAP_RSVD_LKEY_OFFSET                0x98
#define QUERY_DEV_CAP_MAX_ICM_SZ_OFFSET               0xa0
#define QUERY_DEV_CAP_ETH_BACKPL_OFFSET               0x9c
#define QUERY_DEV_CAP_DIAG_RPRT_PER_PORT              0x9c
#define QUERY_DEV_CAP_FW_REASSIGN_MAC                 0x9d
#define QUERY_DEV_CAP_VXLAN                           0x9e
#define QUERY_DEV_CAP_MAD_DEMUX_OFFSET                0xb0
#define QUERY_DEV_CAP_DMFS_HIGH_RATE_QPN_BASE_OFFSET  0xa8
#define QUERY_DEV_CAP_DMFS_HIGH_RATE_QPN_RANGE_OFFSET 0xac
#define QUERY_DEV_CAP_QP_RATE_LIMIT_NUM_OFFSET        0xcc
#define QUERY_DEV_CAP_QP_RATE_LIMIT_MAX_OFFSET        0xd0
#define QUERY_DEV_CAP_QP_RATE_LIMIT_MIN_OFFSET        0xd2


// Device command return status codes
#define CMD_RET_OK            0x00
#define CMD_RET_INTERNAL_ERR  0x01
#define CMD_RET_BAD_OP        0x02
#define CMD_RET_BAD_PARAM     0x03
#define CMD_RET_BAD_SYS_STATE 0x04
#define CMD_RET_BAD_RESOURCE  0x05
#define CMD_RET_RESOURCE_BUSY 0x06
#define CMD_RET_EXCEED_LIM    0x07
#define CMD_RET_BAD_RES_STATE 0x09
#define CMD_RET_BAD_INDEX     0x0A
#define CMD_RET_BAD_NVMEM     0x0B
#define CMD_RET_ICM_ERR       0x0C
#define CMD_RET_BAD_QP_STATE  0x10
#define CMD_RET_BAD_SEG_PARAM 0x20
#define CMD_RET_REG_BOUND     0x21
#define CMD_RET_BAD_PKT       0x30
#define CMD_RET_BAD_SIZE      0x40

#define MLX3_MAX_CHUNK_LOG2 18

#define MLX3_DEFAULT_NUM_QP        (1<<17)
#define MLX3_DEFAULT_NUM_SRQ       (1<<16)
#define MLX3_DEFAULT_RDMARC_PER_QP (1<<4)
#define MLX3_DEFAULT_NUM_CQ        (1<<16)
#define MLX3_DEFAULT_NUM_MCG       (1<<13)
#define MLX3_DEFAULT_NUM_MPT       (1<<19)
#define MLX3_DEFAULT_NUM_MTT       (1<<20)  //based ON 1024 Ram Mem 1024 >> log_mtt_per_seg-1
#define MLX3_MAX_NUM_EQS           (1<<9)

#define QP_ANY (-1)

struct mlx3_query_adapter {

    /* 0x00-0xc */
    uint32_t rsvd0[0x4];

    /* 10h */
    uint32_t rsvd1:24;
    uint32_t intapin:8; /*When PCIe interrupt messages are being used, this value is used for clearing an interrupt.
                          To clear an interrupt, the driver should write the value (1<<intapin) into the clr_int regis-ter.
                          When using an MSI-X, this register is not used.*/
    /* 14-18h */
    uint32_t rsvd2[0x2];

    /* 1c */
    uint16_t vsd_vendor_id;
    uint16_t rsvd3;

    /*20 - EC*/
    uint32_t vsd[0x34];               

    /*F0-FC*/
    char psid[16];

}__attribute__((packed));


struct mlx4_rate_limit_caps {
    uint16_t     num_rates; /* Number of different rates */
    uint8_t      min_unit;
    uint16_t     min_val;
    uint8_t      max_unit;
    uint16_t     max_val;
};


enum mlx3_qp_region {
        MLX3_QP_REGION_FW = 0,
        MLX3_QP_REGION_RSS_RAW_ETH,
        MLX3_QP_REGION_BOTTOM = MLX3_QP_REGION_RSS_RAW_ETH,
        MLX3_QP_REGION_ETH_ADDR,
        MLX3_NUM_QP_REGION
};

struct mlx3_dev_cap {

    uint64_t flags;
/*
    uint64_t flags2;
    uint16_t num_sys_eqs;
    struct mlx4_rate_limit_caps rl_caps;
*/
	int min_page_sz;
	int log_num_macs;
	int log_num_vlans;
	int reserved_qps_cnt[MLX3_NUM_QP_REGION];
	int base_sqpn;
	uint64_t reserved_mtts;
	int uar_page_size;
	int num_uars;
	int eqe_size;
	uint64_t uar_size;
	    /* 0x00 */
  	uint32_t rsvd0[4];

	/* 0x10 */
	uint8_t log_max_qp ;
	uint8_t rsvd1      ;
	uint8_t log2_rsvd_qps ;
	uint8_t rsvd2      ;
	uint8_t log_max_qp_sz;
	uint8_t log_max_srq_sz;

	/* 0x14 */
	uint8_t log_max_scqs ;
	uint8_t rsvd3      ;
	uint8_t num_rsvd_scqs ;
	uint8_t rsvd4      ;
	uint8_t log_max_srqs ;
	uint8_t rsvd5      ;
	uint8_t log2_rsvd_srqs ;

	/* 0x18 */
	uint8_t log_max_cq ;
	uint8_t rsvd6 ;
	uint8_t log2_rsvd_cqs ;
	uint8_t rsvd7 ;
	uint8_t log_max_cq_sz;
	uint8_t num_rsvd_eqs;

	/* 0x1C */
	uint8_t log_max_eq ;
	uint8_t rsvd8 ;
	uint8_t log2_rsvd_eqs ;
	uint8_t rsvd9 ;
	uint8_t log_max_d_mpts ;
	uint8_t rsvd10 ;
	uint8_t log_max_eq_sz;

	/* 0x20 */
	uint8_t log_max_mtts ;
	uint8_t rsvd11;
	uint8_t log2_rsvd_mrws ;
	uint8_t rsvd12;
	uint8_t log_max_mrw_sz ;
	uint8_t rsvd13 ;
	uint8_t log2_rsvd_mtts ;

	/* 0x24 */
	uint32_t rsvd14;

	/* 0x28 */
	uint8_t log_max_ra_res_qp ;
	uint16_t rsvd15;
	uint8_t log_max_ra_req_qp ;
	uint16_t rsvd16 ;

	/* 0x2C */
	uint8_t log_max_ra_res_global ;
	uint8_t rsvd17 ;
	uint8_t log2_max_rss_tbl_sz ;
	uint8_t rss_toeplitz ;
	uint8_t rss_xor ;
	uint8_t rsvd18 ;
	uint8_t log2_max_gso_sz ;
	uint16_t rsvd19 ;

	/* 0x30 */
	uint8_t rsz_srq ;
	uint16_t rsvd20 ;
	uint8_t cmd_query_stat_cfg ;
	uint16_t rsvd21 ;

	int ports;

	/* 0x34 */
	uint8_t num_ports ;
	uint8_t rsvd22 ;
	uint8_t pci_pf_num;
	uint8_t local_ca_ack_delay ;
	uint8_t cqmep ;
	uint8_t rsvd23;

	/* 0x38 */
	uint32_t rsvd24 ;
	uint8_t log_max_msg;
	uint8_t rsvd25 ;

	/* 0x3C */
	uint16_t stat_rate_blank;
	uint16_t stat_rate_support;

	/* 0x40 */
	uint8_t eth_uc_loopback ;
	uint8_t eth_mc_loopback ;
	uint8_t rsvd26 ;
	uint8_t hs ;
	uint8_t wol_port1 ;
	uint8_t wol_port2 ;
	uint8_t thermal_warning ;
	uint8_t rss_udp ;
	uint8_t vep_uc_steering ;
	uint8_t vep_mc_steering ;
	uint8_t steering_by_vlan ;
	uint8_t steering_by_ethertype ;
	uint8_t wqe_ver1 ;
	uint8_t driver_keep_alive ;
	uint8_t ptp1588 ;
	uint8_t rsvd27 ;
	uint8_t feup ;
	uint16_t rsvd28 ;
	uint8_t eqe_64_byte ;
	uint8_t cqe_64_byte ;
	uint8_t rsvd29 ;

	/* 0x44 */
	uint8_t rc ;
	uint8_t uc ;
	uint8_t ud ;
	uint8_t xrc ; 
	uint8_t rcm ;
	uint8_t fcoib ;
	uint8_t srq ;
	uint8_t chhecksum ;
	uint8_t pkv ;
	uint8_t qkv ;
	uint8_t vmm ;
	uint8_t fcoe ;
	uint8_t dpdp ;
	uint8_t raw_ethertype ;
	uint8_t raw_ipv6 ;
	uint8_t blh ;
	uint8_t mw ;
	uint8_t apm ;
	uint8_t atm ;
	uint8_t rm ;
	uint8_t avp ;
	uint8_t udm ;
	uint8_t udm_ipv4 ;
	uint8_t dif;
	uint8_t pg ;
	uint8_t r;
	uint8_t l2mc ;
	uint8_t rsvd30 ;
	uint8_t ud_swp ;
	uint8_t ipv6_ex ;
	uint8_t lle ;
	uint8_t fcoe_tl1 ;

	/* 0x48 */
	uint8_t log_pg_sz;
	uint8_t rsvd31;
	uint8_t uar_sz ;
	uint8_t rsvd32 ;
	uint8_t num_rsvd_uars ;

	/* 0x4C */
	uint8_t log_max_bf_pages ;
	uint8_t rsvd33 ;
	uint8_t log_max_bf_regs_per_page ;
	uint8_t rsvd34 ;
	uint8_t log_bf_reg_size ;
	uint16_t rsvd35 ;
	uint8_t bf ;

	/* 0x50 */
	uint16_t max_desc_sz_sq;
	uint8_t max_sg_sq;
	uint8_t rsvd36;

	/* 0x54 */
	uint16_t max_desc_sz_rq;
	uint8_t max_sg_rq;
	uint8_t rsvd37;

	/* 0x58 */
	uint32_t rsvd38;

	/* 0x5C */
	uint32_t rsvd39;

	/* 0x60 */
	uint8_t log_max_mcg;
	uint8_t num_rsvd_mcgs ;
	uint8_t rsvd40 ;
	uint8_t log_max_qp_mcg;
	uint8_t rsvd41;

	/* 0x64 */
	uint8_t log_max_xrcd ;
	uint8_t rsvd42 ;
	uint8_t num_rsvd_xcrds ;
	uint8_t log_max_pd ;
	uint8_t rsvd43 ;
	uint8_t num_rsvd_pds ;

	/* 0x68 */
	uint32_t rsvd44;

	/* 0x6C */
	uint32_t rsvd45;

	/* 0x70 */
	uint32_t rsvd46;

	/* 0x74 -> 0x7C */
	uint32_t rsvd47[3];

	/* 0x80 */
	uint16_t qpc_entry_sz;
	uint16_t rdmardc_entry_sz;

	/* 0x84 */
	uint16_t altc_entry_size;
	uint16_t aux_entry_size;

	/* 0x88 */
	uint16_t cqc_entry_sz;
	uint16_t eqc_entry_sz;

	/* 0x8C */
	uint16_t c_mpt_entry_sz;
	uint16_t srq_entry_sz;

	/* 0x90 */
	uint16_t d_mpt_entry_sz;
	uint16_t mtt_entry_sz;

	/* 0x94 */
	uint8_t bmme ;
	uint8_t win_type ;
	uint8_t mps ;
	uint8_t bl ;
	uint8_t zb ;
	uint8_t lif ;
	uint8_t li ;
	uint8_t ri ;
	uint8_t rsvd48 ;
	uint8_t type2win ;
	uint8_t rlkey ;
	uint8_t frwr ;
	uint32_t rsvd49 ;

	/* 0x98 */
	uint32_t resd_lkey;

	/* 0x9C */
	uint32_t rsvd50;

	/*A0*/
    uint64_t max_icm_sz;


    /* 0xA8 -> 0xFC */
    uint32_t rsvd51[16];              //chamged from 22

    int num_sys_eqs;

} __attribute__((packed));




enum {
    MLX3_WQE_CTRL_NEC               = 1 << 29,
    MLX3_WQE_CTRL_IIP               = 1 << 28,
    MLX3_WQE_CTRL_ILP               = 1 << 27,
    MLX3_WQE_CTRL_FENCE             = 1 << 6,
    MLX3_WQE_CTRL_CQ_UPDATE         = 3 << 2,
    MLX3_WQE_CTRL_SOLICITED         = 1 << 1,
    MLX3_WQE_CTRL_IP_CSUM           = 1 << 4,
    MLX3_WQE_CTRL_TCP_UDP_CSUM      = 1 << 5,
    MLX3_WQE_CTRL_INS_CVLAN         = 1 << 6,
    MLX3_WQE_CTRL_INS_SVLAN         = 1 << 7,
    MLX3_WQE_CTRL_STRONG_ORDER      = 1 << 7,
    MLX3_WQE_CTRL_FORCE_LOOPBACK    = 1 << 0,
};

enum { 
    MLX3_OPCODE_NOP                 = 0x00, 
    MLX3_OPCODE_SEND_INVAL          = 0x01, 
    MLX3_OPCODE_RDMA_WRITE          = 0x08, 
    MLX3_OPCODE_RDMA_WRITE_IMM      = 0x09, 
    MLX3_OPCODE_SEND                = 0x0a, 
    MLX3_OPCODE_SEND_IMM            = 0x0b, 
    MLX3_OPCODE_LSO                 = 0x0e, 
    MLX3_OPCODE_RDMA_READ           = 0x10, 
    MLX3_OPCODE_ATOMIC_CS           = 0x11, 
    MLX3_OPCODE_ATOMIC_FA           = 0x12, 
    MLX3_OPCODE_MASKED_ATOMIC_CS    = 0x14, 
    MLX3_OPCODE_MASKED_ATOMIC_FA    = 0x15, 
    MLX3_OPCODE_BIND_MW             = 0x18, 
    MLX3_OPCODE_FMR                 = 0x19, 
    MLX3_OPCODE_LOCAL_INVAL         = 0x1b, 
    MLX3_OPCODE_CONFIG_CMD          = 0x1f, 

    MLX3_RECV_OPCODE_RDMA_WRITE_IMM = 0x00, 
    MLX3_RECV_OPCODE_SEND           = 0x01, 
    MLX3_RECV_OPCODE_SEND_IMM       = 0x02, 
    MLX3_RECV_OPCODE_SEND_INVAL     = 0x03, 

    MLX3_CQE_OPCODE_ERROR           = 0x1e, 
    MLX3_CQE_OPCODE_RESIZE          = 0x16, 
}; 

enum {
    MLX3_NUM_ASYNC_EQE      = 0x100,
    MLX3_NUM_SPARE_EQE      = 0x80,
    MLX3_EQ_ENTRY_SIZE      = 0x20
};
enum {  
    MLX3_CMPT_TYPE_QP       = 0,
    MLX3_CMPT_TYPE_SRQ      = 1,
    MLX3_CMPT_TYPE_CQ       = 2,      
    MLX3_CMPT_TYPE_EQ       = 3,
    MLX3_CMPT_NUM_TYPE
};

enum {
    /* Set port Ethernet input modifiers */
    MLX3_SET_PORT_GENERAL   = 0x0,
    MLX3_SET_PORT_RQP_CALC  = 0x1,
    MLX3_SET_PORT_MAC_TABLE = 0x2,
    MLX3_SET_PORT_VLAN_TABLE = 0x3,
    MLX3_SET_PORT_PRIO_MAP  = 0x4,
    MLX3_SET_PORT_GID_TABLE = 0x5,
    MLX3_SET_PORT_PRIO2TC   = 0x8,
    MLX3_SET_PORT_SCHEDULER = 0x9,
    MLX3_SET_PORT_VXLAN     = 0xB,
    MLX3_SET_PORT_ROCE_ADDR = 0xD
};

enum {
    MLX3_CMD_WRAPPED,
    MLX3_CMD_NATIVE
};

enum {
    /* Set port opcode modifiers */
    MLX3_SET_PORT_IB_OPCODE         = 0x0,
    MLX3_SET_PORT_ETH_OPCODE        = 0x1,
    MLX3_SET_PORT_BEACON_OPCODE     = 0x4,
};              

enum mlx3_qp_optpar {   
    MLX3_QP_OPTPAR_ALT_ADDR_PATH            = 1 << 0,
    MLX3_QP_OPTPAR_RRE                      = 1 << 1,
    MLX3_QP_OPTPAR_RAE                      = 1 << 2,
    MLX3_QP_OPTPAR_RWE                      = 1 << 3,
    MLX3_QP_OPTPAR_PKEY_INDEX               = 1 << 4,
    MLX3_QP_OPTPAR_Q_KEY                    = 1 << 5,
    MLX3_QP_OPTPAR_RNR_TIMEOUT              = 1 << 6,
    MLX3_QP_OPTPAR_PRIMARY_ADDR_PATH        = 1 << 7,
    MLX3_QP_OPTPAR_SRA_MAX                  = 1 << 8,
    MLX3_QP_OPTPAR_RRA_MAX                  = 1 << 9,
    MLX3_QP_OPTPAR_PM_STATE                 = 1 << 10,
    MLX3_QP_OPTPAR_RETRY_COUNT              = 1 << 12,
    MLX3_QP_OPTPAR_RNR_RETRY                = 1 << 13,
    MLX3_QP_OPTPAR_ACK_TIMEOUT              = 1 << 14,
    MLX3_QP_OPTPAR_SCHED_QUEUE              = 1 << 16,
    MLX3_QP_OPTPAR_COUNTER_INDEX            = 1 << 20,
    MLX3_QP_OPTPAR_VLAN_STRIPPING           = 1 << 21,
};

enum mlx3_event {
    MLX3_EVENT_TYPE_COMP               = 0x00,
    MLX3_EVENT_TYPE_PATH_MIG           = 0x01,
    MLX3_EVENT_TYPE_COMM_EST           = 0x02,
    MLX3_EVENT_TYPE_SQ_DRAINED         = 0x03,
    MLX3_EVENT_TYPE_SRQ_QP_LAST_WQE    = 0x13,
    MLX3_EVENT_TYPE_SRQ_LIMIT          = 0x14,
    MLX3_EVENT_TYPE_CQ_ERROR           = 0x04,
    MLX3_EVENT_TYPE_WQ_CATAS_ERROR     = 0x05,
    MLX3_EVENT_TYPE_EEC_CATAS_ERROR    = 0x06,
    MLX3_EVENT_TYPE_PATH_MIG_FAILED    = 0x07,
    MLX3_EVENT_TYPE_WQ_INVAL_REQ_ERROR = 0x10,
    MLX3_EVENT_TYPE_WQ_ACCESS_ERROR    = 0x11,
    MLX3_EVENT_TYPE_SRQ_CATAS_ERROR    = 0x12,
    MLX3_EVENT_TYPE_LOCAL_CATAS_ERROR  = 0x08,
    MLX3_EVENT_TYPE_PORT_CHANGE        = 0x09, 
    MLX3_EVENT_TYPE_EQ_OVERFLOW        = 0x0f,
    MLX3_EVENT_TYPE_ECC_DETECT         = 0x0e,
    MLX3_EVENT_TYPE_CMD                = 0x0a,
    MLX3_EVENT_TYPE_VEP_UPDATE         = 0x19, 
    MLX3_EVENT_TYPE_COMM_CHANNEL       = 0x18,
    MLX3_EVENT_TYPE_OP_REQUIRED        = 0x1a,
    MLX3_EVENT_TYPE_FATAL_WARNING      = 0x1b,
    MLX3_EVENT_TYPE_FLR_EVENT          = 0x1c,
    MLX3_EVENT_TYPE_PORT_MNG_CHG_EVENT = 0x1d,
    MLX3_EVENT_TYPE_RECOVERABLE_ERROR_EVENT  = 0x3e,
    MLX3_EVENT_TYPE_NONE               = 0xff,
};

enum {
    /* command completed successfully: */
    CMD_STAT_OK             = 0x00,
    /* Internal error (such as a bus error) occurred while processing command: */
    CMD_STAT_INTERNAL_ERR   = 0x01,
    /* Operation/command not supported or opcode modifier not supported: */
    CMD_STAT_BAD_OP         = 0x02,
    /* Parameter not supported or parameter out of range: */
    CMD_STAT_BAD_PARAM      = 0x03,
    /* System not enabled or bad system state: */
    CMD_STAT_BAD_SYS_STATE  = 0x04,
    /* Attempt to access reserved or unallocaterd resource: */
    CMD_STAT_BAD_RESOURCE   = 0x05,
    /* Requested resource is currently executing a command, or is otherwise busy: */
    CMD_STAT_RESOURCE_BUSY  = 0x06,
    /* Required capability exceeds device limits: */
    CMD_STAT_EXCEED_LIM     = 0x08,
    /* Resource is not in the appropriate state or ownership: */
    CMD_STAT_BAD_RES_STATE  = 0x09,
    /* Index out of range: */
    CMD_STAT_BAD_INDEX      = 0x0a,
    /* FW image corrupted: */
    CMD_STAT_BAD_NVMEM      = 0x0b,
    /* Error in ICM mapping (e.g. not enough auxiliary ICM pages to execute command): */
    CMD_STAT_ICM_ERROR      = 0x0c,
    /* Attempt to modify a QP/EE which is not in the presumed state: */
    CMD_STAT_BAD_QP_STATE   = 0x10,
    /* Bad segment parameters (Address/Size): */
    CMD_STAT_BAD_SEG_PARAM  = 0x20,
    /* Memory Region has Memory Windows bound to: */
    CMD_STAT_REG_BOUND      = 0x21,
    /* HCA local attached memory not present: */
    CMD_STAT_LAM_NOT_PRE    = 0x22,
    /* Bad management packet (silently discarded): */
    CMD_STAT_BAD_PKT        = 0x30,
    /* More outstanding CQEs in CQ than new CQ size: */
    CMD_STAT_BAD_SIZE       = 0x40,
    /* Multi Function device support required: */
    CMD_STAT_MULTI_FUNC_REQ = 0x50,
};

enum {
    MLX3_RES_QP,
    MLX3_RES_RDMARC,
    MLX3_RES_ALTC,
    MLX3_RES_AUXC,
    MLX3_RES_SRQ,
    MLX3_RES_CQ,
    MLX3_RES_EQ,
    MLX3_RES_DMPT,
    MLX3_RES_CMPT,
    MLX3_RES_MTT,
    MLX3_RES_MCG,
    MLX3_RES_NUM,
};

enum {
    MLX3_DEV_CAP_FLAG_RC            = 1LL <<  0,
    MLX3_DEV_CAP_FLAG_UC            = 1LL <<  1,
    MLX3_DEV_CAP_FLAG_UD            = 1LL <<  2,
    MLX3_DEV_CAP_FLAG_XRC           = 1LL <<  3, 
    MLX3_DEV_CAP_FLAG_SRQ           = 1LL <<  6,
    MLX3_DEV_CAP_FLAG_IPOIB_CSUM    = 1LL <<  7,
    MLX3_DEV_CAP_FLAG_BAD_PKEY_CNTR = 1LL <<  8,
    MLX3_DEV_CAP_FLAG_BAD_QKEY_CNTR = 1LL <<  9, 
    MLX3_DEV_CAP_FLAG_DPDP          = 1LL << 12,
    MLX3_DEV_CAP_FLAG_BLH           = 1LL << 15,
    MLX3_DEV_CAP_FLAG_MEM_WINDOW    = 1LL << 16,
    MLX3_DEV_CAP_FLAG_APM           = 1LL << 17, 
    MLX3_DEV_CAP_FLAG_ATOMIC        = 1LL << 18,
    MLX3_DEV_CAP_FLAG_RAW_MCAST     = 1LL << 19,
    MLX3_DEV_CAP_FLAG_UD_AV_PORT    = 1LL << 20,
    MLX3_DEV_CAP_FLAG_UD_MCAST      = 1LL << 21,
    MLX3_DEV_CAP_FLAG_IBOE          = 1LL << 30,
    MLX3_DEV_CAP_FLAG_UC_LOOPBACK   = 1LL << 32,
    MLX3_DEV_CAP_FLAG_FCS_KEEP      = 1LL << 34,
    MLX3_DEV_CAP_FLAG_WOL_PORT1     = 1LL << 37,
    MLX3_DEV_CAP_FLAG_WOL_PORT2     = 1LL << 38, 
    MLX3_DEV_CAP_FLAG_UDP_RSS       = 1LL << 40,
    MLX3_DEV_CAP_FLAG_VEP_UC_STEER  = 1LL << 41,
    MLX3_DEV_CAP_FLAG_VEP_MC_STEER  = 1LL << 42,
    MLX3_DEV_CAP_FLAG_COUNTERS      = 1LL << 48,
    MLX3_DEV_CAP_FLAG_RSS_IP_FRAG   = 1LL << 52,
    MLX3_DEV_CAP_FLAG_SET_ETH_SCHED = 1LL << 53,
    MLX3_DEV_CAP_FLAG_SENSE_SUPPORT = 1LL << 55,
    MLX3_DEV_CAP_FLAG_PORT_MNG_CHG_EV = 1LL << 59,
    MLX3_DEV_CAP_FLAG_64B_EQE       = 1LL << 61,
    MLX3_DEV_CAP_FLAG_64B_CQE       = 1LL << 62
};

enum {
    MLX3_DEV_CAP_FLAG2_RSS                  = 1LL <<  0,
    MLX3_DEV_CAP_FLAG2_RSS_TOP              = 1LL <<  1,
    MLX3_DEV_CAP_FLAG2_RSS_XOR              = 1LL <<  2,
    MLX3_DEV_CAP_FLAG2_FS_EN                = 1LL <<  3,
    MLX3_DEV_CAP_FLAG2_REASSIGN_MAC_EN      = 1LL <<  4,
    MLX3_DEV_CAP_FLAG2_TS                   = 1LL <<  5,
    MLX3_DEV_CAP_FLAG2_VLAN_CONTROL         = 1LL <<  6,
    MLX3_DEV_CAP_FLAG2_FSM                  = 1LL <<  7,
    MLX3_DEV_CAP_FLAG2_UPDATE_QP            = 1LL <<  8,
    MLX3_DEV_CAP_FLAG2_DMFS_IPOIB           = 1LL <<  9,
    MLX3_DEV_CAP_FLAG2_VXLAN_OFFLOADS       = 1LL <<  10,
    MLX3_DEV_CAP_FLAG2_MAD_DEMUX            = 1LL <<  11,
    MLX3_DEV_CAP_FLAG2_CQE_STRIDE           = 1LL <<  12,
    MLX3_DEV_CAP_FLAG2_EQE_STRIDE           = 1LL <<  13,
    MLX3_DEV_CAP_FLAG2_ETH_PROT_CTRL        = 1LL <<  14,
    MLX3_DEV_CAP_FLAG2_ETH_BACKPL_AN_REP    = 1LL <<  15,
    MLX3_DEV_CAP_FLAG2_CONFIG_DEV           = 1LL <<  16,
    MLX3_DEV_CAP_FLAG2_SYS_EQS              = 1LL <<  17,
    MLX3_DEV_CAP_FLAG2_80_VFS               = 1LL <<  18,
    MLX3_DEV_CAP_FLAG2_FS_A0                = 1LL <<  19,
    MLX3_DEV_CAP_FLAG2_RECOVERABLE_ERROR_EVENT = 1LL << 20,
    MLX3_DEV_CAP_FLAG2_PORT_REMAP           = 1LL <<  21,
    MLX3_DEV_CAP_FLAG2_QCN                  = 1LL <<  22,
    MLX3_DEV_CAP_FLAG2_QP_RATE_LIMIT        = 1LL <<  23,
    MLX3_DEV_CAP_FLAG2_FLOWSTATS_EN         = 1LL <<  24,
    MLX3_DEV_CAP_FLAG2_QOS_VPP              = 1LL <<  25,
    MLX3_DEV_CAP_FLAG2_ETS_CFG              = 1LL <<  26,
    MLX3_DEV_CAP_FLAG2_PORT_BEACON          = 1LL <<  27,
    MLX3_DEV_CAP_FLAG2_IGNORE_FCS           = 1LL <<  28, 
    MLX3_DEV_CAP_FLAG2_PHV_EN               = 1LL <<  29,
    MLX3_DEV_CAP_FLAG2_SKIP_OUTER_VLAN      = 1LL <<  30,
    MLX3_DEV_CAP_FLAG2_UPDATE_QP_SRC_CHECK_LB = 1ULL << 31,
    MLX3_DEV_CAP_FLAG2_LB_SRC_CHK           = 1ULL << 32,
    MLX3_DEV_CAP_FLAG2_ROCE_V1_V2           = 1ULL <<  33,
    MLX3_DEV_CAP_FLAG2_DMFS_UC_MC_SNIFFER   = 1ULL <<  34,
    MLX3_DEV_CAP_FLAG2_DIAG_PER_PORT        = 1ULL <<  35,
    MLX3_DEV_CAP_FLAG2_SVLAN_BY_QP          = 1ULL <<  36,
    MLX3_DEV_CAP_FLAG2_SL_TO_VL_CHANGE_EVENT = 1ULL << 37,
    MLX3_DEV_CAP_FLAG2_USER_MAC_EN          = 1ULL << 38,
};

enum {                            
    MLX3_CMPT_SHIFT         = 24,
    MLX3_NUM_CMPTS          = MLX3_CMPT_NUM_TYPE << MLX3_CMPT_SHIFT
};                                
/*
 *  * We allocate in as big chunks as we can, up to a maximum of 256 KB
 *   * per chunk.
 *    */
enum {
    MLX3_TABLE_CHUNK_SIZE      = 1 << 18,
    MLX3_TABLE_CHUNK_SIZE_EQ   = 1 << 15

};

static const char *res_name[] = {
    "QP",
    "RDMARC",
    "ALTC",
    "AUXC",
    "SRQ",
    "CQ",
    "EQ",
    "DMPT",
    "CMPT",
    "MTT",
    "MCG",
};

struct mlx3_fw_info {
    uint16_t maj;
    uint16_t min;
    uint16_t submin;
    uint16_t pages;
    uint16_t cmd_ix_rev;
    uint64_t clr_base;
    uint8_t  clr_int_bar;
    void * data;
    void * aux_icm;
};

struct mlx3_rsrc {
    uint64_t size;
    uint64_t start;
    int type;
    uint32_t num;
    uint32_t lognum;
};

struct mlx3_icm_info {
    int nothing;
};

struct mlx3_init_hca_param {
    uint64_t qpc_base;
    uint64_t rdmarc_base;
    uint64_t auxc_base;
    uint64_t altc_base;
    uint64_t srqc_base;
    uint64_t cqc_base;
    uint64_t eqc_base;
    uint64_t mc_base;
    uint64_t dmpt_base; 
    uint64_t cmpt_base;
    uint64_t mtt_base;
    uint64_t global_caps;
    int      num_cqs;
    int      num_qps;
    int  num_eqs;
    int      num_mpts;
    int num_mgms;                                                                                             
    int num_amgms; 
    int      num_srqs;
    int      num_mtts;
    int  max_qp_dest_rdma;
    uint16_t log_mc_entry_sz;
    uint16_t log_mc_hash_sz;
    uint16_t hca_core_clock; /* Internal Clock Frequency (in MHz) */
    uint8_t  log_num_qps;
    uint8_t  log_num_srqs;
    uint8_t  log_num_cqs;
    uint8_t  log_num_eqs;
    uint16_t num_sys_eqs;
    uint8_t  log_rd_per_qp;
    uint8_t  log_mc_table_sz;
    uint8_t  log_mpt_sz;
    uint8_t  log_uar_sz;
    uint8_t  mw_enabled;  /* Enable memory windows */
    uint8_t  uar_page_sz; /* log pg sz in 4k chunks */
    uint8_t  steering_mode; /* for QUERY_HCA */
    uint8_t  dmfs_high_steer_mode; /* for QUERY_HCA */
    uint64_t dev_cap_enabled;
    uint16_t cqe_size; /* For use only when CQE stride feature enabled */
    uint16_t eqe_size; /* For use only when EQE stride feature enabled */
    uint8_t rss_ip_frags;
    uint8_t phv_check_en; /* for QUERY_HCA */
};

struct mlx3_icm 
{
    void * data;
    uint32_t size;
};

struct mlx3_icm_table 
{
    uint64_t                     virt;
    int                          num_icm;
    uint32_t                     num_obj;
    int                          obj_size;
    struct mlx3_icm              ** icm;
};

struct mlx3_cq_context 
{
    uint32_t                  flags;
    uint16_t                  reserved1[3];
    uint16_t                  page_offset;
    uint32_t                  logsize_usrpage;
    uint16_t                  cq_period;
    uint16_t                  cq_max_count;
    uint8_t                   reserved2[3];
    uint8_t                   comp_eqn;
    uint8_t                   log_page_size;
    uint8_t                   reserved3[2];
    uint8_t                   mtt_base_addr_h;
    uint32_t                  mtt_base_addr_l;
    uint32_t                  last_notified_index;
    uint32_t                  solicit_producer_index;
    uint32_t                  consumer_index;
    uint32_t                  producer_index;
    uint32_t                  reserved4[2];
    uint64_t                  db_rec_addr;
};


struct mlx3_cq_table 
{
    struct mlx3_icm_table    table;
    struct mlx3_icm_table    cmpt_table;
};

struct mlx3_qp_table 
{
    struct mlx3_icm_table    qp_table;
    struct mlx3_icm_table    auxc_table;
    struct mlx3_icm_table    altc_table;
    struct mlx3_icm_table    rdmarc_table;
    struct mlx3_icm_table    cmpt_table;
    uint32_t                 rdmarc_base;
    int                      rdmarc_shift;

};

struct mlx3_srq_table 
{
    struct mlx3_icm_table    table;
    struct mlx3_icm_table    cmpt_table;
};

struct mlx3_eq_context 
{
    uint32_t                  flags;
    uint16_t                     reserved1[3];
    uint16_t                  page_offset;
    uint8_t                      log_eq_size;
    uint8_t                      reserved2[4];
    uint8_t                      eq_period;
    uint8_t                      reserved3;
    uint8_t                      eq_max_count;
    uint8_t                      reserved4[3];
    uint8_t                      intr;
    uint8_t                      log_page_size;
    uint8_t                      reserved5[2];
    uint8_t                      mtt_base_addr_h;
    uint32_t                  mtt_base_addr_l;
    uint32_t                     reserved6[2];
    uint32_t                  consumer_index;
    uint32_t                  producer_index;
    uint32_t                     reserved7[4];
}__attribute__((packed));

struct mlx3_eq
{
    int             nentry;
    int             size;
    void            *buffer;
    void            *mtt;
    uint32_t        *doorbell;
    int             eqn;
    int             cons_index;
    struct          pci_dev *dev;
    uint8_t         pci_intr;  // IRQ number on bus
    uint8_t         intr_vec;  // IRQ we will see
    uint64_t         uar_map;
    uint8_t          inta_pin;
    uint32_t        *clr_int;
    uint32_t         clr_mask;
};

struct mlx3_d_mpt
{
    int                 nentry;
    void               *buffer;
    uint64_t            mtt;
    int                 eqn;
    int                 qpn;
    int                 size;
    int                 key;
};

struct mlx3_eq_table {

    struct mlx3_icm_table        table;
    struct mlx3_icm_table        cmpt_table;
};

struct mlx3_mcg_table {
    struct mlx3_icm_table table; 
};

struct mlx3_mr_table {
    uint64_t                     mtt_base;
    uint64_t                     mpt_base;
    int                          offset;
    struct mlx3_icm_table        mtt_table;
    struct mlx3_icm_table        dmpt_table;
    struct mlx3_d_mpt           *dmpt;
};

struct hca_input
{

    /*0h */
    uint32_t rsvd0:24;
    uint8_t  version;

    /*04 - 08h */
    uint32_t rsvd1[0x2];

    /*0Ch*/
    uint16_t rsvd2:13;
    uint8_t  log2_cacheline_sz:3;
    uint16_t hca_core_clock;

    /*10h*/
    uint32_t router_qp:24;
    uint8_t rsvd3:5;
    uint8_t ipr2:1;
    uint8_t ipr1:1;
    uint8_t ibr:1;

    /*14h*/
    uint8_t udp:1;
    uint8_t he:1;
    uint8_t qos:1;
    uint8_t ce:1;
    uint8_t lle:2;
    uint8_t rsvd4:2;
    uint8_t wqe_ver:2;
    uint8_t rsvd5:6;
    uint16_t cqm_short_pkt_limit:14;
    uint8_t cqmp:2;

    /*18h*/
    uint8_t  lle_ip_protocol;
    uint8_t  lle_ip_proto_validi:1;
    uint32_t rsvd6:23;

    /*1C */
    uint32_t rsvd7;

    /*qpc_cqc_eqc_parameters*/
    /*20C -9CH*/

    /*00-0Ch*/
    uint32_t rsvd8[0x4];

    /* 10h*/
    uint32_t qpc_base_addr_h;

    /*14h*/
    uint8_t log_num_of_qp:5;

    uint32_t qpc_base_addr_l:27;

    /*18h-24h*/
    uint32_t rsvd9[0x4];

    /*28h*/
    uint32_t srqc_base_addr_h;

    /*2Ch*/
    uint8_t log_num_of_srq:5;

    uint32_t srqc_base_addr_l:27;

    /*30h*/

    uint32_t cqc_base_addr_h;

    /* 34h*/
    uint8_t log_num_of_cq:5;

    uint32_t cqc_base_addr_l:27;

    /*38 h */
    uint32_t rsvd10:29;
    uint8_t eqe_64_bytes_enable:1;
    uint8_t cqe_64_bytes_enable:1;
    uint8_t rsvd11:1;

    /* 3Ch */
    uint32_t rsvd12;

    /* 40 h */
    uint32_t altc_base_addr_h;

    /* 44 h  */
    uint32_t altc_base_addr_l;

    /*48 -4C */
    uint32_t rsvd13[0x2];

    /*50h*/
    uint32_t auxc_base_addr_h;

    /*54h*/
    uint32_t auxc_base_addr_l;


    /*58-5Ch*/
    uint32_t rsvd14[0x2];

    /*60h*/
    uint32_t eqc_base_addr_h;

    /*64h*/
    uint8_t log_num_of_eq:5;
    uint32_t eqc_base_addr_l:27;

    /*68h-6Ch*/
    uint32_t rsvd15[0x2];

    /*70h*/
    uint32_t rdmardc_base_addr_h;

    /*74h*/
    uint8_t log_num_rd:3;
    uint8_t rsvd16:2;
    uint32_t rdmardc_base_addr_l:27;

    /*78 -7C h*/
    uint32_t rsvd17[0x2];

    /*End of Qpc parameters*/

    /*A0-BC*/
    uint32_t rsvd18[0x8];

    /*C0h-DCh*/

    /*IBUD_multicast_parameters*/

    /*00h*/
    uint32_t mc_base_addr_h;

    /*04h*/
    uint32_t mc_base_addr_l;

    /*08-0c*/
    uint32_t rsvd19[0x2];

    /*10h*/
    uint8_t log2_mc_table_entry_sz:5;
    uint32_t rsvd20:27;

    /*14h*/
    uint8_t log2_mc_table_hash_sz:5;
    uint32_t rsvd21:27;

    /*18h*/
    uint8_t log2_mc_table_sz:5;
    uint32_t rsvd22:19;
    uint8_t mc_hash_fn:3;
    uint8_t uc_group_steering:1;
    uint8_t rsvd23:4;

    /*1CH*/
    uint32_t rsvd24;

    /*End of IBUD_multicast_parameters*/

    /*E0h-ECh*/
    uint32_t rsvd25[0x4];

    /*Initialize TPT parameters*/

    /*00h*/
    uint32_t dmpt_base_adr_h;

    /*04h*/
    uint32_t dmpt_base_adr_l;

    /*08h*/
    uint8_t log_dmpt_sz:6;
    uint8_t rsvd26:2;
    uint8_t pfto:5;
    uint32_t rsvd27:19;

    /*0Ch*/
    uint32_t rsvd28;

    /*10h*/
    uint32_t mtt_base_addr_h;

    /*14h*/
    uint32_t mtt_base_addr_l;

    /*18h*/
    uint32_t cmpt_base_adr_h;

    /*1Ch*/
    uint32_t cmpt_base_adr_l;

    /* End of Tpt Parameters*/

    /*110 11Ch*/
    uint32_t rsvd29[0x4];

    /**** UAR Parameters *****/
    /*00H*/
    uint32_t rsvd30;

    /*04h*/
    uint32_t rsvd31;

    /*08h*/
    uint8_t uar_page_sz;
    uint32_t rsvd32:24;

    /*0c -1c*/
    uint32_t rsvd33[0x5];

    /*** End of Uad Parameters ****/

    /*140h -1cch*/
    uint32_t rsvd34[0x24];

    /*Eth_multicast_parameters*/

    /*1D0h-1ECh*/

    /*00h*/
    uint32_t eth_mc_base_addr_h;

    /*04h*/
    uint32_t eth_mc_base_addr_l;

    /*08-0c*/
    uint32_t rsvd37[0x2];

    /*10h*/
    uint8_t eth_log2_mc_table_entry_sz:5;
    uint32_t rsvd38:27;

    /*14h*/
    uint8_t eth_log2_mc_table_hash_sz:5;
    uint32_t rsvd39:27;

    /*18h*/
    uint8_t eth_log2_mc_table_sz:5;
    uint32_t rsvd40:19;
    uint8_t eth_mc_hash_fn:3;
    uint8_t eth_uc_group_steering:1;
    uint8_t rsvd41:4;

    /*1CH*/
    uint32_t rsvd42;

    /*End of Ethernet_multicast_parameters*/

    /*1f0 -24C*/
    uint32_t rsvd43[0x18];

    /*250h*/
    uint8_t fcoe_t11:1;
    uint32_t rsvd44:31;

    /*254h*/
    uint32_t rsvd45[0x2b];

}__attribute__((packed));

struct mlx3_cmd_context {
    int                  done;
    int                          result;
    int                          next;
    uint64_t                     out_param;
    uint16_t                     token;
    uint8_t                      fw_status;
};
struct mlx3_eqe {
    uint8_t                      reserved1;
    uint8_t                      type;
    uint8_t                      reserved2;
    uint8_t                      subtype;
    union {
        uint32_t             raw[6];
        struct {
            uint32_t  cqn;
        }  __attribute__((packed))  comp;
        struct {
            uint16_t     reserved1;
            uint16_t  token;
            uint32_t     reserved2;
            uint8_t      reserved3[3];
            uint8_t      status; 
            uint64_t  out_param;
        }  __attribute__((packed))  cmd;
        struct {
            uint32_t  qpn;
        }  __attribute__((packed))  qp;
        struct {
            uint32_t  srqn;
        }  __attribute__((packed))  srq;
        struct {
            uint32_t  cqn;
            uint32_t     reserved1;
            uint8_t      reserved2[3];
            uint8_t      syndrome;
        }  __attribute__((packed))  cq_err;
        struct {
            uint32_t     reserved1[2];
            uint32_t  port;
        }  __attribute__((packed))  port_change;
        struct {
#define COMM_CHANNEL_BIT_ARRAY_SIZE     4
            uint32_t reserved;
            uint32_t bit_vec[COMM_CHANNEL_BIT_ARRAY_SIZE];
        }  __attribute__((packed))  comm_channel_arm;
        struct {
            uint8_t      port;
            uint8_t      reserved[3];
            uint64_t     mac;
        }  __attribute__((packed))  mac_update;
        struct {
            uint32_t  slave_id;
        }  __attribute__((packed))  flr_event;
        struct {
            uint16_t  current_temperature;
            uint16_t  warning_threshold;
        }  __attribute__((packed))  warming;
        struct {
            uint8_t reserved[3];
            uint8_t port;
            union {
                struct {
                    uint16_t mstr_sm_lid;
                    uint16_t port_lid;
                    uint32_t changed_attr;
                    uint8_t reserved[3];
                    uint8_t mstr_sm_sl;
                    uint64_t gid_prefix;
                } __packed port_info;
                struct {
                    uint32_t block_ptr;
                    uint32_t tbl_entries_mask;
                } __packed tbl_change_info;
                struct {
                    uint8_t sl2vl_table[8];
                } __packed sl2vl_tbl_change_info;
            } params;
        } __attribute__((packed)) port_mgmt_change;
        struct {
            uint8_t reserved[3];
            uint8_t port;
            uint32_t reserved1[5];
        }  __attribute__((packed))  bad_cable;
    }                       event;
    uint8_t                      slave_id;
    uint8_t                      reserved3[2];
    uint8_t                      owner;
} __attribute__((packed));

#define ETH_ALEN        6

struct mlx3_qp_path {
    uint8_t                      fl;
    union {
        uint8_t                      vlan_control;
        uint8_t                      control;
    };
    uint8_t                      disable_pkey_check;
    uint8_t                      pkey_index;
    uint8_t                      counter_index;
    uint8_t                      grh_mylmc;
    uint16_t                     rlid;
    uint8_t                      ackto;
    uint8_t                      mgid_index;
    uint8_t                      static_rate;
    uint8_t                      hop_limit;
    uint32_t                     tclass_flowlabel;
    uint8_t                      rgid[16];
    uint8_t                      sched_queue;
    uint8_t                      vlan_index;
    uint8_t                      feup;
    uint8_t                      fvl_rx;
    uint8_t                      reserved4[2];
    uint8_t                      dmac[ETH_ALEN];
};

struct mlx3_qp_context {
    uint32_t                  flags;
    uint32_t                  pd;
    uint8_t                   mtu_msgmax;
    uint8_t                   rq_size_stride;
    uint8_t                   sq_size_stride;
    uint8_t                   rlkey_roce_mode;
    uint32_t                  usr_page;
    uint32_t                  local_qpn;
    uint32_t                  remote_qpn;
    struct                    mlx3_qp_path pri_path;
    struct                    mlx3_qp_path alt_path;
    uint32_t                  params1;
    uint32_t                  reserved1;
    uint32_t                  next_send_psn;
    uint32_t                  cqn_send;
    uint16_t                  roce_entropy;
    uint16_t                  reserved2[3];
    uint32_t                  last_acked_psn;
    uint32_t                  ssn;    
    uint32_t                  params2;
    uint32_t                  rnr_nextrecvpsn;
    uint32_t                  xrcd;
    uint32_t                  cqn_recv;
    uint64_t                  db_rec_addr;
    uint32_t                  qkey;
    uint32_t                  srqn;
    uint32_t                  msn;
    uint16_t                  rq_wqe_counter;
    uint16_t                  sq_wqe_counter;
    uint32_t                  reserved3;
    uint16_t                  rate_limit_params;
    uint8_t                   reserved4;
    uint8_t                   qos_vport;
    uint32_t                  param3;
    uint32_t                  nummmcpeers_basemkey;
    uint8_t                   log_page_size;
    uint8_t                   reserved5[2];
    uint8_t                   mtt_base_addr_h;
    uint32_t                  mtt_base_addr_l;
    uint32_t                  reserved6[10];
};

enum
{
    /* QP/EE commands */
    MLX3_CMD_RST2INIT_QP     = 0x19,
    MLX3_CMD_INIT2RTR_QP     = 0x1a,
    MLX3_CMD_RTR2RTS_QP      = 0x1b,
    MLX3_CMD_RTS2RTS_QP      = 0x1c,
    MLX3_CMD_SQERR2RTS_QP    = 0x1d, 
    MLX3_CMD_2ERR_QP         = 0x1e,
    MLX3_CMD_RTS2SQD_QP      = 0x1f,
    MLX3_CMD_SQD2SQD_QP      = 0x38,
    MLX3_CMD_SQD2RTS_QP      = 0x20,
    MLX3_CMD_2RST_QP         = 0x21,
    MLX3_CMD_QUERY_QP        = 0x22,
    MLX3_CMD_INIT2INIT_QP    = 0x2d,
    MLX3_CMD_SUSPEND_QP      = 0x32,
    MLX3_CMD_UNSUSPEND_QP    = 0x33,
    MLX3_CMD_UPDATE_QP       = 0x61,
    /* special QP and management commands */
    MLX3_CMD_CONF_SPECIAL_QP = 0x23,
    MLX3_CMD_MAD_IFC         = 0x24,
    MLX3_CMD_MAD_DEMUX       = 0x203,

};
enum mlx3_qp_state {
    MLX3_QP_STATE_RST                       = 0,
    MLX3_QP_STATE_INIT                      = 1,
    MLX3_QP_STATE_RTR                       = 2,
    MLX3_QP_STATE_RTS                       = 3,
    MLX3_QP_STATE_SQER                      = 4,
    MLX3_QP_STATE_SQD                       = 5,
    MLX3_QP_STATE_ERR                       = 6,
    MLX3_QP_STATE_SQ_DRAINING               = 7,
    MLX3_QP_NUM_STATE
};

struct mlx3_buffer {
    void * buff;
};

struct mlx3_mtt {
    void* addr;
};

union mlx3_wqe_qpn_vlan {
    struct {
        uint16_t  vlan_tag;
        uint8_t      ins_vlan;
        uint8_t      fence_size;
    };
    uint32_t          bf_qpn;
};

struct mlx3_wq_buf {
    void * data;
    void * mtt;
};

struct mlx3_qp {

    struct mlx3_ib * mlx; // parent ptr
    struct mlx3_wq_buf * buf;
    struct mlx3_tx_ring * tx;
    struct mlx3_rx_ring * rx;
    struct mlx3_cq * cq;
    int qpn;
    int uar;
    int is_sqp;
    int tp_cx;
    uint32_t*  db_rec;
    struct mlx3_qp_context * ctx;

    enum mlx3_qp_state state;

    void (*ring_func)(struct mlx3_qp*);
};

/* these are the opcode types used by send packet */
typedef enum { 

    OP_RC_SEND       = 11,
    OP_RC_RECV       = 12,
    OP_RC_SEND_IMM   = 13,
    OP_RC_RDMA_WRITE = 22,
    OP_RC_RDMA_READ  = 23,

    OP_UC_SEND       = 14,
    OP_UC_RECV       = 15,
    OP_UC_SEND_IMM   = 16,
    OP_UC_RDMA_WRITE = 24,

    OP_UD_SEND       = 17,
    OP_UD_RECV       = 18,
    OP_UD_SEND_IMM   = 19,

    OP_RAW_SEND      = 20,
    OP_RAW_RECV      = 21,

} user_trans_op_t;

/* these are the tranpsort types supported by the card */
typedef enum { 
    MLX3_TRANS_RC  = 0,
    MLX3_TRANS_UC  = 1,
    MLX3_TRANS_UD  = 3,
    MLX3_TRANS_RAW = 7,
    MLX3_TRANS_INVALID,
} transport_type_t;

struct mlx3_qp_parms {
    int is_sqp;
    int sq_size;
    int rq_size;
    int sq_stride;
    int rq_stride;
    transport_type_t tp;
};



struct ib_context {
    user_trans_op_t user_op;
    transport_type_t transport;
    int dlid;
    int slid;
    int port;
    int src_qpn;
    int dst_qpn;
    int sq_size;
    int rq_size;
    int sq_stride;
    int rq_stride;
    uint32_t qkey; 
    uint32_t va;
    uint32_t rkey;
};

struct mlx3_av {
    uint32_t          port_pd;
    uint8_t          reserved1;
    uint8_t          g_slid;
    uint16_t          dlid;
    uint8_t          reserved2;
    uint8_t          gid_index;
    uint8_t          stat_rate;
    uint8_t          hop_limit;
    uint32_t          sl_tclass_flowlabel;
    uint32_t          dgid[4];
};

struct mlx3_wqe_datagram_seg { 
    uint32_t          av[8];
    uint32_t          dst_qpn;
    uint32_t          qkey;
    uint16_t          vlan;
    uint8_t          mac[ETH_ALEN];
}; 

struct mlx3_raddr_seg {
    uint64_t va;
    uint32_t key;
    uint32_t rsvd;
};

struct mlx3_wqe_ctrl_seg {
    uint32_t                  owner_opcode;
    uint32_t                  vlan_cv_f_ds;
    uint32_t                  flags;
    uint32_t                  flags2;
};

enum {
    MLX3_WQE_MLX_VL15       = 1 << 17,
    MLX3_WQE_MLX_SLR        = 1 << 16
};

struct mlx3_wqe_mlx_seg {
    uint8_t                      owner;
    uint8_t                      reserved1[2];
    uint8_t                      opcode;
    uint8_t                      reserved2[3];
    uint8_t                      size;
    /*
       [17]    VL15
       [16]    SLR
       [15:12] static rate
       [11:8]  SL
       [4]     ICRC
       [3:2]   C
       [0]     FL (force loopback)
       */
    uint32_t                  flags;
    uint16_t                  rlid;
    uint16_t                  reserved3;
};




enum {
    MCAST_DIRECT_ONLY   = 0,
    MCAST_DIRECT        = 1,
    MCAST_DEFAULT       = 2
};

enum mlx3_special_vlan_idx {
    MLX3_NO_VLAN_IDX        = 0,
    MLX3_VLAN_MISS_IDX,
    MLX3_VLAN_REGULAR
};

#define SET_PORT_PROMISC_SHIFT  31
#define SET_PORT_MC_PROMISC_SHIFT   30
enum {
    MLX3_STEERING_MODE_A0,
    MLX3_STEERING_MODE_B0,
    MLX3_STEERING_MODE_DEVICE_MANAGED
}; 



struct mlx3_wqe_data_seg {
    uint32_t                  byte_count;
    uint32_t                  lkey;
    uint64_t                  addr;
};

struct mlx3_wqe_inline_seg {
    uint32_t                  byte_count;
};

struct mlx3_wqe_lso_seg {
    uint32_t          mss_hdr_size;
    uint32_t          header[0];
};

struct mlx3_rx_desc {
    /* actual number of entries depends on rx ring stride */
    struct mlx3_wqe_data_seg data[0];
};

#define MLX3_EN_BIT_DESC_OWN    0x80000000
#define MLX3_EN_MEMTYPE_PAD     0x100
#define DS_SIZE         sizeof(struct mlx3_wqe_data_seg)
#define NULL_SCATTER_LKEY       0x00000100

struct mlx3_tx_desc {
    struct mlx3_wqe_ctrl_seg ctrl;
    union {
        struct mlx3_wqe_data_seg data; /* at least one data segment */
        struct mlx3_wqe_inline_seg inl;
    };
};


struct mlx3_rdma_desc {
    struct mlx3_wqe_ctrl_seg ctrl;
    struct mlx3_raddr_seg       raddr;
    union {
        struct mlx3_wqe_data_seg data; /* at least one data segment */
        struct mlx3_wqe_inline_seg inl;
    };
};

struct mlx3_uc_desc {
    struct mlx3_wqe_ctrl_seg ctrl;
    union {
        struct mlx3_wqe_data_seg data; /* at least one data segment */
        struct mlx3_wqe_inline_seg inl;
    };
};

struct mlx3_tx_ud_desc {
    struct mlx3_wqe_ctrl_seg ctrl;
    struct mlx3_wqe_datagram_seg ud_ctrl;
    union {
        struct mlx3_wqe_data_seg data; /* at least one data segment */
        struct mlx3_wqe_inline_seg inl;
    };
};

struct mlx3_tx_raw {
    struct mlx3_wqe_mlx_seg ctrl;
    union {
        struct mlx3_wqe_data_seg data; /* at least one data segment */
        struct mlx3_wqe_lso_seg lso;
        struct mlx3_wqe_inline_seg inl;
    };
};

struct mlx3_tx_ring {

    uint32_t                    cons;
    uint32_t                    prod;
    uint32_t                    doorbell_qpn;
    struct mlx3_qp             *qp;
    struct mlx3_qp_context     *context;
    uint32_t                    buf_size;
    uint32_t                    bb_size; // WQEBB size (16 * 2 ^ log2_sq_stride)
    uint32_t                    bb_count; 
    int                         qpn;
    void                       *buf;
    uint8_t                     queue_index;
    enum mlx3_qp_state          qp_state;
    uint16_t                    stride;
    uint16_t                    log_stride;
    uint16_t                    cqn; /* index of port CQ associated with this ring */
    int valid;
};

struct mlx3_rx_ring { 
    uint32_t size ;      /* number of Rx descs*/
    uint32_t size_mask;
    uint16_t stride;
    uint32_t wqe_size;
    uint16_t log_stride;
    uint16_t cqn;        /* index of port CQ associated with this ring */
    uint32_t prod;
    void     *buf;
    uint32_t cons;
    uint32_t buf_size;
    int      valid;
    uint32_t* db_rec;
};

struct mlx3_set_port_ib_context {
    uint32_t flags;
    uint32_t cap_mask;
    uint32_t sys_img_guid_h;
    uint32_t sys_img_guid_l;
    uint32_t guid0_h;
    uint32_t guid0_l;
    uint32_t node_guid_h;
    uint32_t node_guid_l;
    uint32_t egress;
    uint32_t ingress;
    uint32_t pkey_gid;
};

struct mlx3_set_port_general_context {
    uint8_t reserved1[3];
    uint8_t flags;
    uint8_t flags2;
    uint8_t reserved2;
    uint16_t mtu;
    uint8_t pptx;
    uint8_t pfctx;
    uint16_t reserved3;
    uint8_t pprx;
    uint8_t pfcrx;
    uint16_t reserved4;
};

struct mlx3_port_cap {
    uint8_t  link_state;
    uint8_t  supported_port_types;
    uint8_t  suggested_type;
    uint8_t  default_sense;
    uint8_t  log_max_macs;
    uint8_t  log_max_vlans;
    int      ib_mtu;
    int      max_port_width;
    int      max_vl;
    int      max_tc_eth;
    int      max_gids;
    int      max_pkeys;
    uint64_t def_mac;
    uint16_t eth_mtu;
    int      trans_type;
    int      vendor_oui;
    uint16_t wavelength;
    uint64_t trans_code;
    uint8_t  dmfs_optimized_state;
};

struct mlx3_cqe {
    uint32_t                  vlan_my_qpn;
    uint32_t                  immed_rss_invalid;
    uint32_t                  g_mlpath_rqpn;
    uint16_t                  sl_vid;
    union {
        struct {
            uint16_t     rlid;
            uint16_t     status;
            uint8_t      ipv6_ext_mask;
            uint8_t      badfcs_enc;
        };
        uint8_t  smac[ETH_ALEN];
    }; 
    uint32_t                  byte_cnt;
    uint16_t                  wqe_index;
    uint16_t                  checksum;
    uint8_t                   reserved[3];
    uint8_t                   owner_sr_opcode;
};

struct mlx3_cq {
    int                     uar_idx;
    void                    *uar;
    uint32_t                cons_index;
    uint16_t                irq;
    uint32_t                *set_ci_db;
    uint32_t                *arm_db;
    int                     nentry;
    void                    *buffer;
    int                     size;
    int                     arm_sn;
    int                     cqn;
    unsigned                vector;
    int                     reset_notify_added;
    uint8_t                 usage;
    struct mlx3_eq          *eq;
};

struct ethhdr {
    unsigned char   h_dest[ETH_ALEN];       /* destination eth addr */
    unsigned char   h_source[ETH_ALEN];     /* source ether addr    */
    uint16_t          h_proto;                /* packet type ID field */
} __attribute__((packed));


struct mlx3_set_port_rqp_calc_context {
    uint32_t base_qpn;
    uint8_t rererved;
    uint8_t n_mac;
    uint8_t n_vlan;
    uint8_t n_prio;
    uint8_t reserved2[3];
    uint8_t mac_miss;
    uint8_t intra_no_vlan;
    uint8_t no_vlan;
    uint8_t intra_vlan_miss;
    uint8_t vlan_miss;
    uint8_t reserved3[3];
    uint8_t no_vlan_prio;
    uint32_t promisc;
    uint32_t mcast;
};  

/*
 *  * Must be packed because mtt_seg is 64 bits but only aligned to 32 bits.
 *   */
struct mlx3_mpt_entry {
    uint32_t flags;
    uint32_t qpn;
    uint32_t key;
    uint32_t flags_pd;
    uint64_t start;
    uint64_t length;
    uint32_t lkey;
    uint32_t win_cnt;
    uint8_t  reserved1[3];
    uint8_t  mtt_rep;
    uint64_t mtt_addr;
    uint32_t mtt_sz;
    uint32_t entity_size;
    uint32_t first_byte_offset;
    uint32_t rsvd[16];
} __attribute__((packed));

struct ib_reth
{
    uint64_t va;
    uint32_t rkey;
    uint32_t dma_len;
} __attribute__((packed));
/*
 *  *Datagram Extended  Transport Header
 *   *
 *   */
struct ib_deth
{
    uint32_t qkey;                      //Authorize access to receive queue
    uint32_t rsvd_sqpn;                 //Source QP
} __attribute__((packed));

/*
 *  * Base Transport Header 12 Bytes 
 *  */
struct ib_bth
{
    uint8_t opcode;
    /*
     *      * se  : solicited event 
     *           * m   : migration 
     *                * pad : pad count(Additional bytes which may be needed to align to 4 byte boundary) 
     *                     * tver: trasnport version(IBA Transport Header)
     *                         */
    uint8_t   se_m_pad_tver;
    uint16_t  pkey;          //partion key
    uint32_t  rsvd_dqpn;
    uint32_t  ack_rsvd_psn;        //Packet Sequence number 
} __attribute__((packed));

/*
 *  *Local Routing Header 8 Bytes
 *  */
struct ib_lrh
{
    uint8_t  vl_lver;
    uint8_t  sl_rsvd_lnh;           //lnh(2 bits) - The next header followed by lrh 
    uint16_t destination_lid;
    uint16_t rsvd_packet_len;
    uint16_t source_lid;
}__attribute__((packed));

struct ib_headers
{
    struct ib_lrh  * lrh;
    struct ib_bth  * bth;
    struct ib_deth * deth;
    struct ib_reth * reth;
}__attribute__((packed));

struct ib_packet {
    struct ib_headers *header;
    void              *data;
}
__attribute__((packed));


#define IB_SMP_DATA_SIZE            64
#define IB_SMP_MAX_PATH_HOPS        64
union sl2vl_tbl_to_u64 {
    uint8_t  sl8[8];
    uint64_t sl64;
};

struct ib_smp {
    uint8_t  base_version;
    uint8_t  mgmt_class;
    uint8_t  class_version;
    uint8_t  method;
    uint16_t  status;
    uint8_t  hop_ptr;
    uint8_t  hop_cnt;
    uint64_t  tid;
    uint16_t  attr_id;
    uint16_t  resv;
    uint32_t  attr_mod;
    uint64_t  mkey;
    uint16_t  dr_slid;
    uint16_t  dr_dlid;
    uint8_t  reserved[28];
    uint8_t  data[IB_SMP_DATA_SIZE];
    uint8_t  initial_path[IB_SMP_MAX_PATH_HOPS];
    uint8_t  return_path[IB_SMP_MAX_PATH_HOPS];
} __attribute__ ((packed));

union ib_gid {
    uint8_t  raw[16];
    struct {
        uint64_t  subnet_prefix;
        uint64_t  interface_id;
    } global;
};

enum ib_wc_flags {
    IB_WC_GRH       = 1,
    IB_WC_WITH_IMM      = (1<<1),
    IB_WC_WITH_INVALIDATE   = (1<<2),
    IB_WC_IP_CSUM_OK    = (1<<3),
    IB_WC_WITH_SMAC     = (1<<4),
    IB_WC_WITH_VLAN     = (1<<5),
    IB_WC_WITH_NETWORK_HDR_TYPE = (1<<6),
};

enum ib_wc_opcode {
    IB_WC_SEND,
    IB_WC_RDMA_WRITE,
    IB_WC_RDMA_READ,
    IB_WC_COMP_SWAP,
    IB_WC_FETCH_ADD,
    IB_WC_LSO,
    IB_WC_LOCAL_INV,
    IB_WC_REG_MR,
    IB_WC_MASKED_COMP_SWAP,
    IB_WC_MASKED_FETCH_ADD,
    /*
     *  * Set value of IB_WC_RECV so consumers can test if a completion is a
     *   * receive by testing (opcode & IB_WC_RECV).
     *    */
    IB_WC_RECV          = 1 << 7,
    IB_WC_RECV_RDMA_WITH_IMM
};

enum ib_wc_status {
    IB_WC_SUCCESS,
    IB_WC_LOC_LEN_ERR,
    IB_WC_LOC_QP_OP_ERR,
    IB_WC_LOC_EEC_OP_ERR,
    IB_WC_LOC_PROT_ERR,
    IB_WC_WR_FLUSH_ERR,
    IB_WC_MW_BIND_ERR,
    IB_WC_BAD_RESP_ERR,
    IB_WC_LOC_ACCESS_ERR,
    IB_WC_REM_INV_REQ_ERR,
    IB_WC_REM_ACCESS_ERR,
    IB_WC_REM_OP_ERR,
    IB_WC_RETRY_EXC_ERR,
    IB_WC_RNR_RETRY_EXC_ERR,
    IB_WC_LOC_RDD_VIOL_ERR,
    IB_WC_REM_INV_RD_REQ_ERR,
    IB_WC_REM_ABORT_ERR,
    IB_WC_INV_EECN_ERR,
    IB_WC_INV_EEC_STATE_ERR,
    IB_WC_FATAL_ERR,
    IB_WC_RESP_TIMEOUT_ERR,
    IB_WC_GENERAL_ERR
};

struct ib_wc { 
    union {
        uint64_t     wr_id;
        struct ib_cqe   *wr_cqe;
    };
    enum ib_wc_status   status;
    enum ib_wc_opcode   opcode;
    uint32_t         vendor_err;
    uint32_t         byte_len;
    struct mlx3_qp           *qp;
    union {
        uint32_t      imm_data;
        uint32_t     invalidate_rkey;
    } ex;
    uint32_t         src_qp;
    uint32_t         slid;
    int         wc_flags;
    uint16_t         pkey_index;
    uint8_t          sl;
    uint8_t          dlid_path_bits;
    uint8_t          port_num;   /* valid only for DR SMPs on switches */
    uint8_t          smac[ETH_ALEN];
    uint16_t         vlan_id;
    uint8_t          network_hdr_type;
};

struct ib_grh {
    uint32_t      version_tclass_flow;
    uint16_t      paylen;
    uint8_t      next_hdr;
    uint8_t      hop_limit;
    union ib_gid    sgid;
    union ib_gid    dgid;
};

struct mlx3_ib {
    struct nk_net_dev           *netdev;
    struct pci_dev              *dev;
    uint8_t                     pci_intr; 
    uint8_t                     intr_vec;
    uint64_t                    mmio_start;
    uint64_t                    mmio_sz;
    uint64_t                    uar_start;
    int                         uar_ind_scq_db;
    int                         uar_ind_eq_db;
    uint64_t                    icm_size;       
    int                         nt_rsvd_cqn;
    int                         nt_rsvd_eqn;
    int                         nt_rsvd_qpn;
    int                         nt_rsvd_dmpt;
    struct mlx3_fw_info         *fw_info;
    struct mlx3_qp **           qps;
    struct mlx3_cq **           cqs;
    struct mlx3_eq **           eqs;
    struct mlx3_cq_table        cq_table;
    struct mlx3_qp_table        qp_table;
    struct mlx3_eq_table        eq_table;
    struct mlx3_srq_table       srq_table;
    struct mlx3_mr_table        mr_table;
    struct mlx3_mcg_table       mcg_table;
    struct mlx3_init_hca_param  *init_hca;
    struct mlx3_dev_cap         *caps;
    struct mlx3_query_adapter   *query_adapter;
    struct mlx3_port_cap        *port_cap;
};


typedef uint64_t dma_addr_t;

enum {
	MLX3_DEFAULT_MGM_LOG_ENTRY_SIZE = 10,
	MLX3_MIN_MGM_LOG_ENTRY_SIZE = 7,
	MLX3_MAX_MGM_LOG_ENTRY_SIZE = 12,
	MLX3_MAX_QP_PER_MGM = 4 * ((1 << MLX3_MAX_MGM_LOG_ENTRY_SIZE) / 16 - 2),
	MLX3_MTT_ENTRY_PER_SEG  = 8,
};


enum {
    MLX3_ICM_PAGE_SHIFT     = 12,
    MLX3_ICM_PAGE_SIZE      = 1 << MLX3_ICM_PAGE_SHIFT,
};

typedef struct dma_pool {
    struct list_head dma_list;
} dma_pool_t;


typedef struct mlx3_cmd_box {
    dma_addr_t buf;
} mlx3_cmd_box_t;



int mlx3_init(struct naut_info * naut);

#endif
