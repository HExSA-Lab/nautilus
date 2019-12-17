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
 * 	        Kyle Hale <khale@cs.iit.edu>
 *          Piyush Nath <piyush.nath.2272@gmail.com>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */

#include <nautilus/nautilus.h>
#include <nautilus/macros.h>
#include <nautilus/cpu.h>
#include <nautilus/dev.h>
#include <nautilus/netdev.h>
#include <nautilus/mtrr.h>
#include <nautilus/math.h>
#include <dev/pci.h>
#include <dev/mlx3_ib.h>
#include <nautilus/irq.h>

#ifndef NAUT_CONFIG_DEBUG_MLX3_PCI
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...)
#endif


#define __mzero_checked(ptr, size, msg, ret_expr) \
    ptr = malloc(size); \
    if (!ptr) { \
        ERROR(msg); \
        ret_expr; \
    } \
    memset(ptr, 0, size);

#define mzero_checked(ptr, size, desc) \
    __mzero_checked(ptr, size, "Could not allocate " desc "\n", return -1)

    

static inline uint16_t
bswap16 (uint16_t x)
{
    return __bswap16(x);    
}

static inline uint32_t
bswap32 (uint32_t x)
{
    return __bswap32(x);
}

static inline uint64_t
bswap64 (uint64_t x)
{
    return __bswap64(x);
}

static inline uint16_t
bswap16p (const uint16_t * p)
{
    return bswap16(*p);    
}

static inline uint32_t
bswap32p (const uint32_t * p)
{
    return bswap32(*p);
}

static inline uint64_t
bswap64p (const uint64_t * p)
{
    return bswap64(*p);
}


#define MLX3_GET(dest, source, offset)                          \
    do {                                                        \
        void *__p = (char *) (source) + (offset);               \
        switch (sizeof(dest)) {                                 \
            case 1: (dest) = *(uint8_t *) __p; break;           \
            case 2: (dest) = bswap16p(__p); break;              \
            case 4: (dest) = bswap32p(__p); break;              \
            case 8: (dest) = bswap64p(__p); break;              \
            default: ERROR("Bad use of MLXGET\n"); break;       \
        }                                                       \
    } while (0)                                                            

#define MLX3_PUT(dest, source, offset)                           \
    do {                                                         \
        void *__d = ((char *) (dest) + (offset));                \
        switch (sizeof(source)) {                                \
            case 1: *(uint8_t*) __d = (source);           break; \
            case 2: *(uint16_t *) __d =  bswap16(source); break; \
            case 4: *(uint32_t *) __d =  bswap32(source); break; \
            case 8: *(uint64_t *) __d =  bswap64(source); break; \
            default: ERROR("Bad use of MLXPUT\n"); break;        \
        }                                                        \
    } while (0)                                          


static inline void
unmarshal_area (uint32_t * dst, uint32_t * src, unsigned cnt)
{
	int i;

	for (i = 0; i < cnt; i++) {
		dst[i] = bswap32(src[i]);
		DEBUG("DW %02x: 0x%08x\n", i*4, dst[i]);
	}
}


static inline void
marshal_area (uint32_t * dst, uint32_t * src, unsigned cnt)
{
	unmarshal_area(dst, src, cnt);
}


static uint16_t
get_fw_rev_maj (struct mlx3_ib * mlx)
{
	return READ_MEM(mlx, 0) & 0xffff;

}


static uint16_t
get_fw_rev_min (struct mlx3_ib * mlx)
{
	return READ_MEM(mlx, 0) >> 16;
}


static uint16_t
get_fw_rev_submin (struct mlx3_ib * mlx)
{
	return READ_MEM(mlx, 4) & 0xffff;
}


static uint16_t
get_cmd_ix_rev (struct mlx3_ib * mlx)
{
	return READ_MEM(mlx, 4) >> 16;
}


static dma_addr_t
get_dma_page (void) 
{
	dma_addr_t a = (dma_addr_t)NULL;

	/* pray that this is not in high memory for now */
	a = (dma_addr_t)malloc(PAGE_SIZE_4KB);
	if (!a) {
		ERROR("Could not allocate DMA page\n");
	}

	memset((void*)a, 0, PAGE_SIZE_4KB);

	return a;
}


static void
free_dma_page (dma_addr_t addr)
{
	free((void*)addr);
}


static mlx3_cmd_box_t *
create_cmd_mailbox (struct mlx3_ib * m)
{
	mlx3_cmd_box_t * ret = NULL;

	ret = malloc(sizeof(mlx3_cmd_box_t));
	if (!ret) {
		ERROR("Could not create cmd mailbox\n");
		return NULL;
	}
	memset(ret, 0, sizeof(mlx3_cmd_box_t));

	ret->buf = get_dma_page();
	if (!ret->buf) {
		ERROR("Could not init cmd mailbox\n");
		goto out;
	}

	return ret;

out:
	free(ret);
	return NULL;
}


static void
destroy_cmd_mailbox (mlx3_cmd_box_t * cmd)
{
	free_dma_page(cmd->buf);
	free(cmd);
}

// TODO: this should be part of a cmd context or something...
static uint8_t exp_toggle = 0;


static int
cmd_pending (struct mlx3_ib * m)
{
	uint32_t status = READ_MEM(m, MLX_HCR_BASE + HCR_STATUS_OFFSET);

	return ((bswap32(status) >> HCR_GO_BIT) & 1) ||
		(((bswap32(status) >> HCR_T_BIT) & 1) != exp_toggle);
}


static int
mlx3_mailbox_cmd_post_in (struct mlx3_ib * m,
		uint64_t in_param,
		uint64_t out_param,
		uint32_t in_mod,
		uint8_t op_mod,
		uint16_t op,
		uint16_t token,
		int event)
{
	while (cmd_pending(m)) {
		udelay(1000);
	}

	WRITE_MEM(m, MLX_HCR_BASE,      bswap32(((uint32_t)(in_param >> 32))));
	WRITE_MEM(m, MLX_HCR_BASE + 4,  bswap32(((uint32_t)(in_param & 0xfffffffful))));
	WRITE_MEM(m, MLX_HCR_BASE + 8,  bswap32(in_mod));
	WRITE_MEM(m, MLX_HCR_BASE + 12, bswap32((((uint32_t)(out_param >> 32)))));
	WRITE_MEM(m, MLX_HCR_BASE + 16, bswap32(((uint32_t)(out_param & 0xfffffffful))));
	WRITE_MEM(m, MLX_HCR_BASE + 20, bswap32(((uint32_t)(token << 16))));

	mbarrier();

	exp_toggle ^= 1;

	WRITE_MEM(m, MLX_HCR_BASE + 24,
			bswap32((1 << HCR_GO_BIT) |
				(exp_toggle << HCR_T_BIT) |
				(event ? (1 << HCR_E_BIT) : 0) |
				(op_mod << HCR_OPMOD_SHIFT) |
				op));

	mbarrier();


	return 0;
}


static int
mlx3_mailbox_cmd_post (struct mlx3_ib * m,
		uint64_t in_param,
		uint64_t out_param,
		uint32_t in_mod,
		uint8_t op_mod,
		uint16_t op,
		uint16_t token,
		int event)
{
	while (cmd_pending(m)) {
		udelay(1000);
	}

	WRITE_MEM(m, MLX_HCR_BASE,      bswap32(((uint32_t)(in_param >> 32))));
	WRITE_MEM(m, MLX_HCR_BASE + 4,  bswap32(((uint32_t)(in_param & 0xfffffffful))));
	WRITE_MEM(m, MLX_HCR_BASE + 8,  bswap32(in_mod));
	WRITE_MEM(m, MLX_HCR_BASE + 12, bswap32((((uint32_t)(out_param >> 32)))));
	WRITE_MEM(m, MLX_HCR_BASE + 16, bswap32(((uint32_t)(out_param & 0xfffffffful))));
	WRITE_MEM(m, MLX_HCR_BASE + 20, bswap32(((uint32_t)(token << 16))));

	mbarrier();

	exp_toggle ^= 1;

	WRITE_MEM(m, MLX_HCR_BASE + 24, 
			bswap32((1 << HCR_GO_BIT) |
				(exp_toggle << HCR_T_BIT) |
				(event ? (1 << HCR_E_BIT) : 0) |
				(op_mod << HCR_OPMOD_SHIFT) |
				op));

	mbarrier();


	return 0;
}


static int
mlx3_mailbox_cmd_poll_in (struct mlx3_ib * m,
                          uint64_t * in_param,
                          uint64_t * out_param,
                          int out_is_imm,
                          uint32_t in_mod,
                          uint8_t op_mod,
                          uint16_t op,
                          ulong_t timeout)
{
	int err = 0;
	uint32_t stat;

	// TODO: actually use timeout here

	err = mlx3_mailbox_cmd_post(m, *in_param, out_param ? *out_param : 0,
			in_mod, op_mod, op, CMD_POLL_TOKEN, 0);

	if (err) {
		ERROR("Could not poll mlx3\n");
		return -1;
	}

	while (cmd_pending(m)) {
		udelay(100);
	}

	if (out_is_imm) {
		*out_param = (uint64_t)bswap32(READ_MEM(m, MLX_HCR_BASE + HCR_OUT_PARM_OFFSET)) << 32 |
			(uint64_t)bswap32(READ_MEM(m, MLX_HCR_BASE + HCR_OUT_PARM_OFFSET + 4));
	}

	stat = bswap32(READ_MEM(m, MLX_HCR_BASE + HCR_STATUS_OFFSET)) >> HCR_STATUS_SHIFT;

	if (stat) {
		ERROR("Status failed with error %d\n", stat);
		return -1;
	}

	return 0;
}


static int
mlx3_mailbox_cmd_poll_event (struct mlx3_ib * m,
	                     	 uint64_t in_param,
		                     uint64_t * out_param,
		                     int out_is_imm,
		                     uint32_t in_mod,
		                     uint8_t op_mod,
		                     uint16_t op,
		                     ulong_t timeout)
{
	int err = 0;
	uint32_t stat;

	err = mlx3_mailbox_cmd_post(m, in_param, out_param ? *out_param : 0, 
			in_mod, op_mod, op, CMD_POLL_TOKEN, 1);

	if (err) {
		ERROR("Could not poll mlx3\n");
		return -1;
	}

	return 0;
}


static int
mlx3_mailbox_cmd_poll (struct mlx3_ib * m,
		               uint64_t in_param,
		               uint64_t * out_param,
		               int out_is_imm,
		               uint32_t in_mod,
		               uint8_t op_mod,
		               uint16_t op,
		               ulong_t timeout)
{
	int err = 0;
	uint32_t stat;

	// TODO: actually use timeout here

	err = mlx3_mailbox_cmd_post(m, in_param, out_param ? *out_param : 0, 
			in_mod, op_mod, op, CMD_POLL_TOKEN, 0);

	if (err) {
		ERROR("Could not poll mlx3\n");
		return -1;
	}

	while (cmd_pending(m)) {
		udelay(100);
	}

	if (out_is_imm) {
		*out_param = (uint64_t)bswap32(READ_MEM(m, MLX_HCR_BASE + HCR_OUT_PARM_OFFSET)) << 32 |
			(uint64_t)bswap32(READ_MEM(m, MLX_HCR_BASE + HCR_OUT_PARM_OFFSET + 4));
	}

	stat = bswap32(READ_MEM(m, MLX_HCR_BASE + HCR_STATUS_OFFSET)) >> HCR_STATUS_SHIFT;

	if (stat) {
		ERROR("Status failed with error %x\n", stat);
		return -1;
	}

	return 0;
}


static int 
__mlx3_cmd (struct mlx3_ib * m, 
		uint64_t in_param, 
		uint64_t *out_param,
		int out_is_imm, 
		uint32_t in_mod, 
		uint8_t op_mod,
		uint16_t op, 
		ulong_t timeout)
{
	return mlx3_mailbox_cmd_poll(m, in_param, out_param, out_is_imm,
			in_mod, op_mod, op, timeout);
}


static int 
__mlx3_cmd_event (struct mlx3_ib * m, 
		uint64_t in_param, 
		uint64_t *out_param,
		int out_is_imm, 
		uint32_t in_mod, 
		uint8_t op_mod,
		uint16_t op, 
		ulong_t timeout)
{
	return mlx3_mailbox_cmd_poll_event(m, in_param, out_param, out_is_imm,
			in_mod, op_mod, op, timeout);
}


static int
__mlx3_cmd_in (struct mlx3_ib * m,
		uint64_t *in_param,
		uint64_t *out_param,
		int out_is_imm,
		uint32_t in_mod,
		uint8_t op_mod,
		uint16_t op,
		ulong_t timeout)
{
	return mlx3_mailbox_cmd_poll_in(m, in_param, out_param, out_is_imm,
			in_mod, op_mod, op, timeout);
}


static int 
mlx3_mailbox_cmd_event (struct mlx3_ib * m, 
		uint64_t in_param,
		uint64_t out_param,
		uint32_t in_mod,
		uint8_t op_mod,
		uint16_t op,
		ulong_t timeout)
{
	return __mlx3_cmd_event(m, in_param, &out_param, 0, in_mod, op_mod, op, timeout);
}


static int 
mlx3_mailbox_cmd (struct mlx3_ib * m, 
		uint64_t in_param,
		uint64_t out_param,
		uint32_t in_mod,
		uint8_t op_mod,
		uint16_t op,
		ulong_t timeout)
{
	return __mlx3_cmd(m, in_param, &out_param, 0, in_mod, op_mod, op, timeout);
}


static int 
mlx3_mailbox_cmd_imm (struct mlx3_ib * m, 
		uint64_t in_param,
		uint64_t *out_param,
		uint32_t in_mod,
		uint8_t op_mod,
		uint16_t op,
		ulong_t timeout)
{
	return __mlx3_cmd(m, in_param, out_param, 1, in_mod, op_mod, op, timeout);
}


static int
mlx3_mailbox_cmd_imm_in (struct mlx3_ib * m,
		uint64_t *in_param,
		uint64_t *out_param,
		uint32_t in_mod,
		uint8_t op_mod,
		uint16_t op,
		ulong_t timeout)
{
	return __mlx3_cmd_in(m, in_param, out_param, 1, in_mod, op_mod, op, timeout);
}


static int
mlx3_reset (struct mlx3_ib * m)
{
	int cnt = 1000;
	uint32_t sem = 0;

	DEBUG("Initiating card reset for ConnectX-3...\n");

	// TODO: save config space

	do {
		sem = READ_MEM(m, MLX3_RESET_BASE + MLX3_SEM_OFF);

		if (!sem) break;

		udelay(1000);

	} while (cnt--);


	if (sem) {
		ERROR("Failed to acquire HW semaphore, aborting\n");
		return -1;
	}

	WRITE_MEM(m, MLX3_RESET_BASE + MLX3_RESET_OFF, MLX3_RESET_VAL);

	// wait one second, says the hw docs
	udelay(1000000);

	cnt = 100;

	uint16_t vendor = 0;

	do {

		// wait for it to respond to PCI cycles
		if ((vendor = pci_dev_cfg_readw(m->dev, 0)) != 0xffff) 
			break;

		udelay(1000);

	} while (cnt--);

	if (vendor == 0xffff) {
		ERROR("Card failed to reset, aborting\n");
		return -1;
	}

	// TODO: restore config headers

	DEBUG("Device reset complete.\n");

	return 0;
}


static int 
get_mcg_log_entry_size (void)
{
	// TODO: how do we actually choose this correctly?
	return MLX3_DEFAULT_MGM_LOG_ENTRY_SIZE;
}


static void inline 
swap (struct mlx3_rsrc* rtab_a, struct mlx3_rsrc* rtab_b)
{
	struct mlx3_rsrc* tmp;
	tmp = rtab_a;
	rtab_a = rtab_b;
	rtab_b = tmp;
}


static int
mlx3_get_icm_size (struct mlx3_ib * mlx, 
                   uint64_t * icm_size,
                   struct mlx3_init_hca_param *init_hca)
{
	struct mlx3_rsrc * rtab;
        struct mlx3_rsrc tmp;
	int i, j;
	uint32_t nqp        = MLX3_DEFAULT_NUM_QP;
	int log_mtt_per_seg = 3;
	uint64_t total_size = 0;

	rtab = malloc(sizeof(struct mlx3_rsrc)*MLX3_RES_NUM);
	if (!rtab) {
		ERROR("Could not allocate resource table\n");
	    goto out_err;	
	}
	memset(rtab, 0, sizeof(struct mlx3_rsrc) * MLX3_RES_NUM);

	rtab[MLX3_RES_QP].size     = mlx->caps->qpc_entry_sz;
	rtab[MLX3_RES_RDMARC].size = mlx->caps->rdmardc_entry_sz;
	rtab[MLX3_RES_ALTC].size   = mlx->caps->altc_entry_size;
	rtab[MLX3_RES_AUXC].size   = mlx->caps->aux_entry_size;
	rtab[MLX3_RES_SRQ].size    = mlx->caps->srq_entry_sz;
	rtab[MLX3_RES_CQ].size     = mlx->caps->cqc_entry_sz;
	rtab[MLX3_RES_EQ].size     = mlx->caps->eqc_entry_sz;
	rtab[MLX3_RES_DMPT].size   = mlx->caps->d_mpt_entry_sz;
	rtab[MLX3_RES_CMPT].size   = mlx->caps->c_mpt_entry_sz;
	rtab[MLX3_RES_MTT].size    = mlx->caps->mtt_entry_sz;
	rtab[MLX3_RES_MCG].size    = 1 << get_mcg_log_entry_size();

	rtab[MLX3_RES_QP].num     = nqp;
	rtab[MLX3_RES_RDMARC].num = nqp * MLX3_DEFAULT_RDMARC_PER_QP;
	rtab[MLX3_RES_ALTC].num   = nqp;
	rtab[MLX3_RES_AUXC].num   = nqp;
	rtab[MLX3_RES_SRQ].num    = MLX3_DEFAULT_NUM_SRQ;
	rtab[MLX3_RES_CQ].num     = MLX3_DEFAULT_NUM_CQ;
	rtab[MLX3_RES_EQ].num     = MLX3_MAX_NUM_EQS;
	rtab[MLX3_RES_DMPT].num   = MLX3_DEFAULT_NUM_MPT;
	rtab[MLX3_RES_CMPT].num   = MLX3_NUM_CMPTS;
	rtab[MLX3_RES_MTT].num    = MLX3_DEFAULT_NUM_MTT * (1<<log_mtt_per_seg);
	rtab[MLX3_RES_MCG].num    = MLX3_DEFAULT_NUM_MCG ;

	for (i = 0; i < MLX3_RES_NUM; ++i) {
		rtab[i].type    = i;
		rtab[i].num     = roundup_pow_of_two(rtab[i].num);
		rtab[i].lognum  = ilog2(rtab[i].num);
		rtab[i].size   *= rtab[i].num;
		rtab[i].size    = max(rtab[i].size, (uint64_t)PAGE_SIZE_4KB);
	}

    /*
     * Sort the resources in decreasing order of size.  Since they
     * all have sizes that are powers of 2, we'll be able to keep
     * resources aligned to their size and pack them without gaps
     * using the sorted order.
     */
	 for (i = 0; i < MLX3_RES_NUM; ++i) {
		for (j = 1; j < MLX3_RES_NUM-i; ++j) {
            if (rtab[j].size > rtab[j - 1].size) { 
                tmp       = rtab[j];
                rtab[j]   = rtab[j-1];
                rtab[j-1] = tmp;
            }
        }
     }

	 for (i = 0; i < MLX3_RES_NUM; ++i) {

		rtab[i].start  = total_size;
		total_size    += rtab[i].size;


		uint64_t max_icm_sz = mlx->caps->max_icm_sz; 
		if (total_size > max_icm_sz) {
			ERROR("Total size (%ld) > maximum ICM size (%ld)\n", total_size, max_icm_sz);
			return 0;
		}

		if (rtab[i].size) {
			DEBUG("  resource[%2d] (%6s): 2^%02d entries @ 0x%10llx size %d KB\n",
					i, 
					res_name[rtab[i].type],
					rtab[i].lognum,
					(uint64_t)rtab[i].start,
					(uint64_t)rtab[i].size>>10);
		}
     }

     for (i = 0; i < MLX3_RES_NUM; ++i) {

         switch (rtab[i].type) {

             case MLX3_RES_CMPT:
                 init_hca->cmpt_base      = rtab[i].start;
                 break;

             case MLX3_RES_CQ:		
                 init_hca->num_cqs     = rtab[i].num;
                 init_hca->cqc_base    = rtab[i].start;
                 init_hca->log_num_cqs = rtab[i].lognum;
                 break;
             case MLX3_RES_SRQ:
                 init_hca->num_srqs     = rtab[i].num;
                 init_hca->srqc_base    = rtab[i].start;
                 init_hca->log_num_srqs = rtab[i].lognum;
                 break;

             case MLX3_RES_QP:
                 init_hca->num_qps     = rtab[i].num;
                 init_hca->qpc_base    = rtab[i].start;
                 init_hca->log_num_qps = rtab[i].lognum;
                 break;
             case MLX3_RES_ALTC:
                 init_hca->altc_base = rtab[i].start;
                 break;
             case MLX3_RES_AUXC:
                 init_hca->auxc_base = rtab[i].start;
                 break;
             case MLX3_RES_MTT:
                 init_hca->num_mtts     = rtab[i].num;
                 mlx->mr_table.mtt_base = rtab[i].start;
                 init_hca->mtt_base     = rtab[i].start;
                 break;
             case MLX3_RES_EQ:
                 init_hca->num_eqs     = MLX3_MAX_NUM_EQS;
                 init_hca->eqc_base    = rtab[i].start;
                 init_hca->log_num_eqs = ilog2(init_hca->num_eqs);
                 break;
             case MLX3_RES_RDMARC:
                 for (mlx->qp_table.rdmarc_shift = 0;
                         MLX3_DEFAULT_NUM_QP << mlx->qp_table.rdmarc_shift < rtab[i].num;
                         ++mlx->qp_table.rdmarc_shift) {
                     init_hca->max_qp_dest_rdma = 1 << mlx->qp_table.rdmarc_shift;
                     mlx->qp_table.rdmarc_base  = (uint32_t) rtab[i].start;
                     init_hca->rdmarc_base      = rtab[i].start;
                     init_hca->log_rd_per_qp    = mlx->qp_table.rdmarc_shift;
                 }
                 break;
             case MLX3_RES_DMPT:
                 init_hca->num_mpts      = rtab[i].num;
                 mlx->mr_table.mpt_base  = rtab[i].start;
                 init_hca->dmpt_base     = rtab[i].start;
                 init_hca->log_mpt_sz    = rtab[i].lognum;
                 break;
             case MLX3_RES_MCG:
                 init_hca->mc_base         = rtab[i].start;
                 init_hca->log_mc_entry_sz = ilog2(1 << get_mcg_log_entry_size());
                 init_hca->log_mc_table_sz = rtab[i].lognum;
                 init_hca->log_mc_hash_sz  = rtab[i].lognum - 1;
                 init_hca->num_mgms        = rtab[i].num >> 1;
                 init_hca->num_amgms       = rtab[i].num >> 1;
                 break;
             default:
                 break;
         }

     }

     DEBUG("Max ICM Size: %ld GB\n", (mlx->caps->max_icm_sz>>30) );
     DEBUG("ICM memory reserving %d GB\n", (int)(total_size>>30));
     DEBUG("HCA Pages Required  %d\n", (int)(total_size>>12));
     *icm_size = total_size;

     return 0;

out_err:
     free(rtab);
     return -1;
}


static int
mlx3_set_icm (struct mlx3_ib * mlx, uint64_t size, uint64_t*  output)
{
	uint64_t aux_pages;
	int err = 0;

	err = mlx3_mailbox_cmd_imm(mlx, 
                               size, 
                               &aux_pages, 
                               0, 
                               0, 
                               CMD_SET_ICM_SIZE, 
                               CMD_TIME_CLASS_A);

	if (err) {
		ERROR("Could not set ICM size\n");
		return -1;
	}

	 /*
         * Round up number of system pages needed in case
         * MLX3_ICM_PAGE_SIZE < PAGE_SIZE.
         */ 


	aux_pages = ALIGN(aux_pages, PAGE_SIZE_4KB / MLX3_ICM_PAGE_SIZE) >>
                (PAGE_SHIFT_4KB - MLX3_ICM_PAGE_SHIFT);

	DEBUG("ICM auxilliary area requires %lu 4K pages\n", aux_pages);

	*output = aux_pages;

	return 0;
}


static void 
dump_dev_cap_flags1 (uint64_t flags)
{
	int i;
	static const char * fl[32] = {
		"Unicast loopback support",
		"Multicast loopback support",
		"", "",
		"Header-data split support",
		"Wake on LAN (port 1) support",
		"Wake on LAN (port 2) support",
		"Thermal warning event",
		"UDP RSS support",
		"Unicast VEP steering support",
		"Multicast VEP steering support",
		"VLAN steering support",
		"EtherType steering support",
		"WQE v1 support",
		"",
		"PTP1588 support",
		"", "",
		"QPC Ethernet user priority support",
		"", "", "", "", "", "", "", "", "", "", 
		"64B EQE support",
		"64B CQE support",
		""
	};

	for (i = 0; i < 31; i++) {
		if (i == 2 || i == 3 || i == 14 || i == 16 || i == 17 ||
				(i > 18 && i < 29))
			continue;

		if (flags & (1<<i)) 
			INFO("    [%s]\n", fl[i]);
	}
}


static void 
dump_dev_cap_flags2 (uint32_t flags)
{
	int i;
	static const char * fl[32] = {
		"RC transport support",
		"UC transport support",
		"UD transport support", 
		"XRC transport support", 
		"Reliable Multicast support",
		"FCoB support",
		"SRQ support",
		"IPoIB checksum support",
		"PKey Violation Counter support",
		"Qkey Violation Counter support",
		"VMM support",
		"FCoE support",
		"DPDP support",
		"Raw Ethertype support",
		"Raw IPv6 support",
		"LSO header support",
		"Memory window support", 
		"Automatic Path Migration support",
		"Atomic op support",
		"Raw multicast support",
		"AVP support",
		"UD Multicast support",
		"UD IPv4 Multicast support",
		"DIF support",
		"Paging on Demand support",
		"Router mode support",
		"L2 Multicast support",
		"",
		"UD transport SW parsing support",
		"TCP checksum support for IPv6 support",
		"Low Latency Ethernet support",
		"FCoE T11 frame format support",
	};

	for (i = 0; i < 31; i++) {
		if (i == 27)
			continue;

		if (flags & (1<<i)) 
			INFO("    [%s]\n", fl[i]);
	}
}


static void
dump_psid_entry (char * inp, int cnt, const char * desc)
{
    char tmp[4] = {0,0,0,0};
	int i;

    if (cnt >= 4) return;

	for (i = 0; i < cnt; i++)
		tmp[i] = inp[i];

    DEBUG("%s: %s\n", desc, tmp);
}


static void
mlx3_dump_query_adapter (struct mlx3_query_adapter * qw_adapter)
{
	DEBUG("VSD ID %d\n", qw_adapter->vsd_vendor_id);

	dump_psid_entry(qw_adapter->psid, 3, "Vendor Symbol");
	dump_psid_entry(qw_adapter->psid + 3, 3, "Board Type Symbol");
	dump_psid_entry(qw_adapter->psid + 6, 3, "Board Version Symbol");
	dump_psid_entry(qw_adapter->psid + 9, 4, "Parameter Set Number");
}


static inline int 
mlx3_to_hw_uar_index (struct mlx3_ib * mlx, int index)
{
	return (index << (PAGE_SHIFT_4KB - DEFAULT_UAR_PAGE_SHIFT));
}


static inline int
mlx3_get_uar_eq_index (struct mlx3_ib * mlx)
{
    return mlx->uar_ind_eq_db++;
}


static inline int
mlx3_alloc_uar_scq_idx (struct mlx3_ib * mlx)
{
    return mlx->uar_ind_scq_db++;
}


static inline int
mlx3_alloc_dmpt (struct mlx3_ib * mlx)
{
    int index = mlx->nt_rsvd_dmpt;
    mlx->nt_rsvd_dmpt += 256;
    return index;
}


static inline int
mlx3_alloc_qpn (struct mlx3_ib * mlx)
{
    return mlx->nt_rsvd_qpn++;
}


static inline int
mlx3_alloc_cqn (struct mlx3_ib * mlx)
{
    return mlx->nt_rsvd_cqn++;
}


static inline int
mlx3_alloc_eqn (struct mlx3_ib * mlx)
{
    return mlx->nt_rsvd_eqn++;
}


static void
mlx3_dump_dev_cap (struct mlx3_dev_cap * cap)
{
//	if (cap->log_bf_reg_size) {
//		DEBUG("BlueFlame available (reg size %d, regs/page %d)\n",
//				1 << cap->log_bf_reg_size,
//				1 << cap->log_max_bf_regs_per_page);
//	} else {
//		DEBUG("BlueFlame unavailable\n");
//	}

//	DEBUG("Base MM extensions: flags : %08x, rsvd L_Key %08x\n",
//			*(uint32_t*)((uint64_t)cap + 0x94),
//			cap->resd_lkey);


	DEBUG("Max ICM size %lld PB\n", cap->max_icm_sz>>50);

	DEBUG("Max QPs: %d, reserved QPs: %d, QPC entry size: %d\n",
			1<<cap->log_max_qp,
			1<<cap->log2_rsvd_qps,
			cap->qpc_entry_sz);

	DEBUG("Max SRQs: %d, reserved SRQs: %d, entry size: %d\n",
			1<<cap->log_max_srqs,
			1<<cap->log2_rsvd_srqs,
			cap->srq_entry_sz);

	DEBUG("Max CQs: %d, reserved CQs: %d, CQC entry size: %d\n",
			1<<cap->log_max_cq,
			1<<cap->log2_rsvd_cqs,
			cap->cqc_entry_sz);

	DEBUG("Max EQs: %d, reserved EQs: %d, EQC entry size: %d\n", 
			1<<cap->log_max_eq,
			1<<cap->log2_rsvd_eqs,
			cap->eqc_entry_sz);

	DEBUG("reserved MPTs: %d, reserved MTTs: %d\n", 
			1<<cap->log2_rsvd_mrws,
			1<<cap->log2_rsvd_mtts);

//	DEBUG("Max PDs: %d, reserved PDs: %d, reserved UARs: %d\n",
//			1<<cap->log_max_pd,
//			cap->num_rsvd_pds,
//			cap->num_rsvd_uars);

//	DEBUG("Max QP/MCG: %d, reserved MGMs: %d\n", 
//			1<<cap->log_max_pd,
//			cap->num_rsvd_mcgs);

	DEBUG("Max CQE count: %d, max QPE count: %d, max SRQE count: %d max EQE count: %d\n",
			1<<cap->log_max_cq_sz,
			1<<cap->log_max_qp_sz,
			1<<cap->log_max_srq_sz,
			1<<cap->log_max_eq_sz);

	DEBUG("MTT Entry Size %d Reserved MTTS %d\n", cap->mtt_entry_sz, 1 << cap->log2_rsvd_mtts);

	DEBUG("cMPT Entry Size %d dMPT Entry Size %d\n", cap->c_mpt_entry_sz, cap->d_mpt_entry_sz);

	DEBUG("Reserved UAR %d  UAR Size %d Num UAR %d \n", cap->num_rsvd_uars, cap->uar_size, cap->num_uars);

	DEBUG("Network Port count %d \n", cap->num_ports);

	DEBUG("Min Page Size %d \n", cap->min_page_sz );

	DEBUG("Max SQ desc size WQE Entry Size: %d, max SQ S/G WQE Enteries: %d\n", 
			cap->max_desc_sz_sq,
			cap->max_sg_sq);

//	DEBUG("Max RQ desc size: %d, max RQ S/G: %d\n", 
//			cap->max_desc_sz_rq,
//			cap->max_sg_rq);

//	DEBUG("Max GSO size: %d\n", 1<<cap->log2_max_gso_sz);

//	DEBUG("Max RSS table size: %d\n", 
//			1<<cap->log2_max_rss_tbl_sz);

//	uint32_t * fl1 = (uint32_t*)((uint64_t)cap + 0x40);
//	uint32_t * fl2 = (uint32_t*)((uint64_t)cap + 0x44);

//	DEBUG("Supported device features:\n");

	dump_dev_cap_flags1(cap->flags >> 32);
	dump_dev_cap_flags2(cap->flags & 0xffffffffu);
}


static int 
mlx3_query_dev_cap (struct mlx3_ib * mlx)
{
	int err                   = 0;
	mlx3_cmd_box_t * cmd      = create_cmd_mailbox(mlx);
	struct mlx3_dev_cap * cap = NULL;
	uint32_t field32, flags, ext_flags;
	uint16_t size;
	uint16_t stat_rate;
	uint32_t * outbox;
	uint8_t field;

	if (!cmd) {
		ERROR("Could not create cmd mailbox\n");
		return -1;
	}

	err = mlx3_mailbox_cmd(mlx, 
			0, 
			cmd->buf, 
			0, 
			0, 
			CMD_QUERY_DEV_CAP, 
			CMD_TIME_CLASS_A);

	if (err) {
		ERROR("Could not query device capabilities\n");
		goto out_err;
	}

	cap = malloc(sizeof(struct mlx3_dev_cap));
	if (!cap) {
		ERROR("Could not allocate dev cap struct\n");
		goto out_err;
	}
	memset(cap, 0, sizeof(struct mlx3_dev_cap));

	outbox = (uint32_t*)cmd->buf;

    MLX3_GET(field, outbox, QUERY_DEV_CAP_RSVD_QP_OFFSET);
    cap->log2_rsvd_qps =  (field & 0xf);
    MLX3_GET(field, outbox, QUERY_DEV_CAP_MAX_QP_OFFSET);
    cap->log_max_qp =  (field & 0x1f);
    MLX3_GET(field, outbox, QUERY_DEV_CAP_RSVD_SRQ_OFFSET);
    cap->log2_rsvd_srqs =   (field >> 4);
    MLX3_GET(field, outbox, QUERY_DEV_CAP_MAX_SRQ_OFFSET);
    cap->log_max_srqs =  (field & 0x1f);
    MLX3_GET(field, outbox, QUERY_DEV_CAP_MAX_CQ_SZ_OFFSET);
    cap->log_max_cq_sz =  field;
    MLX3_GET(field, outbox, QUERY_DEV_CAP_RSVD_CQ_OFFSET);
    cap->log2_rsvd_cqs =  (field & 0xf);
    MLX3_GET(field, outbox, QUERY_DEV_CAP_MAX_CQ_OFFSET);
    cap->log_max_cq =  (field & 0x1f);
    MLX3_GET(field, outbox, QUERY_DEV_CAP_MAX_MPT_OFFSET);
    cap->log_max_d_mpts = (field & 0x3f);
    MLX3_GET(field, outbox, QUERY_DEV_CAP_RSVD_EQ_OFFSET);
    cap->log2_rsvd_eqs =  (field & 0xf);                                    //deprecated refer to num_resv_uar
    MLX3_GET(field, outbox, QUERY_DEV_CAP_MAX_EQ_OFFSET);
    cap->log_max_eq = (field & 0xf);
    MLX3_GET(field, outbox, QUERY_DEV_CAP_NUM_RSVD_EQ_OFFSET);
    cap->num_rsvd_eqs = (field & 0xf);                                  //num_resv_uar
    MLX3_GET(field, outbox, QUERY_DEV_CAP_RSVD_MTT_OFFSET);
    cap->log2_rsvd_mtts =  (field >> 4);
    MLX3_GET(field, outbox, QUERY_DEV_CAP_RSVD_MRW_OFFSET);
    cap->log2_rsvd_mrws =  (field & 0xf);
    MLX3_GET(field, outbox, QUERY_DEV_CAP_MAX_SRQ_SZ_OFFSET);
    cap->log_max_srq_sz = field;
    MLX3_GET(field, outbox, QUERY_DEV_CAP_MAX_QP_SZ_OFFSET);
    cap->log_max_qp_sz = field;
    MLX3_GET(field, outbox, QUERY_DEV_CAP_MAX_EQ_SZ_OFFSET);
    cap->log_max_eq_sz = field;

	MLX3_GET(size, outbox, QUERY_DEV_CAP_RDMARC_ENTRY_SZ_OFFSET);
	cap->rdmardc_entry_sz = size;
	MLX3_GET(size, outbox, QUERY_DEV_CAP_QPC_ENTRY_SZ_OFFSET);
	cap->qpc_entry_sz = size;
	MLX3_GET(size, outbox, QUERY_DEV_CAP_AUX_ENTRY_SZ_OFFSET);
	cap->aux_entry_size = size;
	MLX3_GET(size, outbox, QUERY_DEV_CAP_ALTC_ENTRY_SZ_OFFSET);
	cap->altc_entry_size = size;
	MLX3_GET(size, outbox, QUERY_DEV_CAP_EQC_ENTRY_SZ_OFFSET);
	cap->eqc_entry_sz = size;
	MLX3_GET(size, outbox, QUERY_DEV_CAP_CQC_ENTRY_SZ_OFFSET);
	cap->cqc_entry_sz = size;
	MLX3_GET(size, outbox, QUERY_DEV_CAP_SRQ_ENTRY_SZ_OFFSET);
	cap->srq_entry_sz = size;
	MLX3_GET(size, outbox, QUERY_DEV_CAP_C_MPT_ENTRY_SZ_OFFSET);
	cap->c_mpt_entry_sz = size;
	MLX3_GET(size, outbox, QUERY_DEV_CAP_MTT_ENTRY_SZ_OFFSET);
	cap->mtt_entry_sz = size;
	MLX3_GET(size, outbox, QUERY_DEV_CAP_D_MPT_ENTRY_SZ_OFFSET);
	cap->d_mpt_entry_sz = size;
    MLX3_GET(field, outbox, QUERY_DEV_CAP_RSVD_UAR_OFFSET);
    cap->num_rsvd_uars = field >> 4;
    MLX3_GET(field, outbox, QUERY_DEV_CAP_UAR_SZ_OFFSET);
    cap->uar_size = 1 << ((field & 0x3f) + 20);
    MLX3_GET(field32, outbox, QUERY_DEV_CAP_RSVD_LKEY_OFFSET);
    cap->resd_lkey = field32;
    MLX3_GET(field, outbox, QUERY_DEV_CAP_PAGE_SZ_OFFSET);
    cap->min_page_sz = 1 << field;
    MLX3_GET(field, outbox, QUERY_DEV_CAP_MAX_SG_SQ_OFFSET);
    cap->max_sg_sq = field;
    MLX3_GET(size, outbox, QUERY_DEV_CAP_MAX_DESC_SZ_SQ_OFFSET);
    cap->max_desc_sz_sq = size;
    cap->num_uars      = cap->uar_size / PAGE_SIZE_4KB;
    MLX3_GET(cap->max_icm_sz, outbox, QUERY_DEV_CAP_MAX_ICM_SZ_OFFSET);
    /*
     * Each UAR has 4 EQ doorbells; so if a UAR is reserved, then
     * we can't use any EQs whose doorbell falls on that page,
     * even if the EQ itself isn't reserved.
     */
    cap->num_rsvd_eqs = max(cap->num_rsvd_uars * 4, cap->num_rsvd_eqs);
    cap->reserved_qps_cnt[MLX3_QP_REGION_FW] = 1 << cap->log2_rsvd_qps;
    //number of ports
    MLX3_GET(field, outbox, QUERY_DEV_CAP_VL_PORT_OFFSET);
    cap->num_ports = field & 0xf;
    MLX3_GET(ext_flags, outbox, QUERY_DEV_CAP_EXT_FLAGS_OFFSET);
    MLX3_GET(flags, outbox, QUERY_DEV_CAP_FLAGS_OFFSET);
    cap->flags = flags | (uint64_t)ext_flags << 32;

    cap->reserved_qps_cnt[MLX3_QP_REGION_ETH_ADDR] =
        (1 << cap->log_num_macs) *
        (1 << cap->log_num_vlans) *
        cap->num_ports;

    mlx3_dump_dev_cap(cap);
    mlx->caps = cap;

    destroy_cmd_mailbox(cmd);

	return 0;

out_err:
	destroy_cmd_mailbox(cmd);
	return -1;
}


static int
mlx3_run_fw (struct mlx3_ib * mlx)
{
	int err = 0;

	err = mlx3_mailbox_cmd(mlx, 
			0,
			0, 
			0, 
			0, 
			CMD_RUN_FW, 
			CMD_TIME_CLASS_A);

	if (err) {
		ERROR("Could not run firmware\n");
		return -1;
	}

	DEBUG("Successfully run firmware\n");

	return 0;
}


static int 
mlx3_map_icm (struct mlx3_ib * mlx, unsigned pages, void * data, uint64_t card_va)
{
    int i, cnt, align;
    uint64_t ptr, vptr;
    int err = 0;
    uint32_t pa_h;
    uint32_t pa_l;
    uint32_t va_h;
    uint32_t va_l;

    //DEBUG("\nStart Iter\n");
    mlx3_cmd_box_t * cmd = create_cmd_mailbox(mlx);

  //  DEBUG("\nMAlloc success\n");
    ptr  = (uint64_t)data;
    vptr = card_va;

    align  = __ffs(ptr);
    /**
     *Warning !! not 100 % sure about this
     */

    if (align > ilog2(PAGE_SIZE_4KB)) {
        DEBUG("Warning !! Alignment greater than 4KB , defaulting to 4KB\n");
        align = ilog2(PAGE_SIZE_4KB);
    }

    cnt = (pages * PAGE_SIZE_4KB) / (1 << align) +
        ((pages * PAGE_SIZE_4KB) % (1 << align) == 0 ? 0 : 1);

    for (i = 0; i < cnt; i++) {

        pa_h = (uint32_t)(ptr >> 32);
        pa_l = (uint32_t)((ptr & 0xffffffffu) | (align - MLX3_ICM_PAGE_SHIFT));

        va_h = (uint32_t)(vptr >> 32);
        va_l = (uint32_t)((vptr & 0xffffffffu));

        MLX3_SETL(cmd->buf, 0x0, va_h);
        MLX3_SETL(cmd->buf, 0x4, va_l);		

        MLX3_SETL(cmd->buf, 0x8, pa_h);
        MLX3_SETL(cmd->buf, 0xC, pa_l);
       // printk("\nAddr %p pa_h %p pa_l %p flip pal %p flip pah %p\n", ptr, pa_h, bswab32(pa_l), bswab32(pa_h));
        err = mlx3_mailbox_cmd(mlx,
                cmd->buf,
                NA,
                1,
                NA,
                CMD_MAP_ICM,
                CMD_TIME_CLASS_A);

//        DEBUG("\nEnd on %d Iter\n", i);
        if (err) {
            ERROR("Could not map ICM area\n");
            goto out_err;
        }

        ptr  += 1 << align;
        vptr += 1 << align;
    }

    destroy_cmd_mailbox(cmd);

    return 0;

out_err:

    destroy_cmd_mailbox(cmd);
    return -1;
}

static int 
mlx3_init_icm_table (struct mlx3_ib * mlx, 
		struct mlx3_icm_table * table, 
		int obj_size, 
		uint32_t nobj, 
		int reserved,
		uint64_t table_size,
		uint64_t virt)
{

	int obj_per_chunk;        
	int num_icm;
	unsigned chunk_size;
	uint64_t size;
	uint64_t npages;
	int i = 0;

	table->num_obj  = nobj;
	table->obj_size = obj_size;
	size            = (uint64_t) nobj * obj_size;
	table->virt     = virt;
	obj_per_chunk   = MLX3_TABLE_CHUNK_SIZE / obj_size;

	num_icm = (nobj + obj_per_chunk - 1) / obj_per_chunk;

    mzero_checked(table->icm, num_icm * sizeof(struct mlx3_icm*), "ICM");

    for (i = 0; (i * MLX3_TABLE_CHUNK_SIZE) < (reserved * obj_size); ++i) {

        chunk_size = MLX3_TABLE_CHUNK_SIZE;

        if ((i + 1) * MLX3_TABLE_CHUNK_SIZE > size) {
            chunk_size = PAGE_ALIGN(size - i * MLX3_TABLE_CHUNK_SIZE);
        }

        table->icm[i] = malloc(sizeof(struct mlx3_icm));
        if (!table->icm[i]) {
            ERROR("Could not allocate ICM %d in %s\n", i, __func__);
            goto out_err;
        }
        memset(table->icm[i], 0, sizeof(struct mlx3_icm));

        npages = chunk_size / PAGE_SIZE_4KB;

        if(npages == 0){
            npages = 1;
            chunk_size = npages * PAGE_SIZE_4KB;
        }

        table->icm[i]->data = malloc(chunk_size);

        mlx->icm_size += chunk_size;		
        if (!table->icm[i]->data) {
            ERROR("Could not allocate ICM data for %d in %s\n", i, __func__);
            free(table->icm[i]);
            goto out_err; 
        }
        memset(table->icm[i]->data, 0, chunk_size);

        if (mlx3_map_icm(mlx,
                    npages,
                    table->icm[i]->data, 
                    table->virt + (i * MLX3_TABLE_CHUNK_SIZE)) != 0) {
            ERROR("Could not map ICM table chunk\n");
            free(table->icm[i]->data);
            free(table->icm[i]);
            goto out_err;
        }
    }

	return 0;

out_err:
    free(table->icm);
    return -1;
}


#define QUERY_ADAPTER_INTA_PIN_OFFSET      0x10
static int
mlx3_query_adapter (struct mlx3_ib* mlx)
{
    int err = 0;
    mlx3_cmd_box_t * cmd = create_cmd_mailbox(mlx);
    struct mlx3_query_adapter * qw_adapter = NULL;

    if (!cmd) {
        ERROR("Could not create cmd mailbox\n");
        return -1;
    }

    err = mlx3_mailbox_cmd(mlx,
            0,
            cmd->buf,
            0,
            0,
            CMD_QUERY_ADAPTER,
            CMD_TIME_CLASS_A);

    if (err) {
        ERROR("Could not query Adapter\n");
        goto out_err;
    }

    qw_adapter = malloc(sizeof(struct mlx3_query_adapter));
    if (!qw_adapter) {
        ERROR("Could not allocate Query Adapter  struct\n");
        goto out_err;
    }
    memset(qw_adapter, 0, sizeof(struct mlx3_query_adapter));

#ifdef NAUT_CONFIG_DEBUG_MLX3_PCI
    //	mlx3_dump_query_adapter(qw_adapter);
#endif
    uint8_t field;

    MLX3_GET(field, cmd->buf, QUERY_ADAPTER_INTA_PIN_OFFSET);

    qw_adapter->intapin = field;

    mlx->query_adapter = qw_adapter;

    destroy_cmd_mailbox(cmd);

    return 0;

out_err:
    destroy_cmd_mailbox(cmd);
    return -1;

}

static int 
mlx3_init_hca (struct mlx3_ib * mlx, struct mlx3_init_hca_param * init_hca)
{		
	uint32_t * inbox;
	int err;

	mlx3_cmd_box_t * cmd = create_cmd_mailbox(mlx);

	if (!cmd) {
		ERROR("Could not create cmd mailbox\n");
		return -1;
	}

	inbox = (uint32_t*)cmd->buf;

	*((uint8_t *) cmd->buf + INIT_HCA_VERSION_OFFSET) = INIT_HCA_VERSION;

	// Little Endian on the host
	*(inbox + INIT_HCA_FLAGS_OFFSET / 4) &= ~bswap32(1 << 1);

	// Enable Counters
	*(inbox + INIT_HCA_FLAGS_OFFSET / 4) |= bswap32(1 << 4);

    /* Check port for UD address vector: */
    *(inbox + INIT_HCA_FLAGS_OFFSET / 4) |= bswap32(1);


	// Base ICM Address
	MLX3_PUT(inbox, init_hca->qpc_base,      INIT_HCA_QPC_BASE_OFFSET);
	MLX3_PUT(inbox, init_hca->log_num_qps,   INIT_HCA_LOG_QP_OFFSET);
	MLX3_PUT(inbox, init_hca->srqc_base,     INIT_HCA_SRQC_BASE_OFFSET);
	MLX3_PUT(inbox, init_hca->log_num_srqs,  INIT_HCA_LOG_SRQ_OFFSET);
	MLX3_PUT(inbox, init_hca->cqc_base,      INIT_HCA_CQC_BASE_OFFSET);
	MLX3_PUT(inbox, init_hca->log_num_cqs,   INIT_HCA_LOG_CQ_OFFSET);
	MLX3_PUT(inbox, init_hca->altc_base,     INIT_HCA_ALTC_BASE_OFFSET);
	MLX3_PUT(inbox, init_hca->auxc_base,     INIT_HCA_AUXC_BASE_OFFSET);
	MLX3_PUT(inbox, init_hca->eqc_base,      INIT_HCA_EQC_BASE_OFFSET);
	MLX3_PUT(inbox, init_hca->log_num_eqs,   INIT_HCA_LOG_EQ_OFFSET);
	MLX3_PUT(inbox, init_hca->num_sys_eqs,   INIT_HCA_NUM_SYS_EQS_OFFSET);
	MLX3_PUT(inbox, init_hca->rdmarc_base,   INIT_HCA_RDMARC_BASE_OFFSET);
	MLX3_PUT(inbox, init_hca->log_rd_per_qp, INIT_HCA_LOG_RD_OFFSET);

	MLX3_PUT(inbox, init_hca->mc_base, INIT_HCA_MC_BASE_OFFSET);
	MLX3_PUT(inbox, init_hca->log_mc_entry_sz, INIT_HCA_LOG_MC_ENTRY_SZ_OFFSET);
	MLX3_PUT(inbox, init_hca->log_mc_hash_sz,  INIT_HCA_LOG_MC_HASH_SZ_OFFSET);
	MLX3_PUT(inbox, init_hca->log_mc_table_sz, INIT_HCA_LOG_MC_TABLE_SZ_OFFSET);

	// TPT attributes 
	MLX3_PUT(inbox, init_hca->dmpt_base,  INIT_HCA_DMPT_BASE_OFFSET);
	MLX3_PUT(inbox, init_hca->mw_enabled, INIT_HCA_TPT_MW_OFFSET);
	MLX3_PUT(inbox, init_hca->log_mpt_sz, INIT_HCA_LOG_MPT_SZ_OFFSET);
	MLX3_PUT(inbox, init_hca->mtt_base,   INIT_HCA_MTT_BASE_OFFSET);
	MLX3_PUT(inbox, init_hca->cmpt_base,  INIT_HCA_CMPT_BASE_OFFSET);
		
	// EQE Size is 32 there is 64 B support also available in CX3
	mlx->caps->eqe_size = 32;

    int s = 0;
    init_hca->uar_page_sz = DEFAULT_UAR_PAGE_SHIFT - PAGE_SHIFT_4KB;

    MLX3_PUT(inbox, init_hca->uar_page_sz, INIT_HCA_UAR_PAGE_SZ_OFFSET);

    err = mlx3_mailbox_cmd(mlx,
            cmd->buf,
            NA,
            NA,
            NA,
            CMD_INIT_HCA,
            CMD_TIME_CLASS_A);

    if (err) {
        ERROR("Could not init HCA\n");
        goto out_err;
    }

    destroy_cmd_mailbox(cmd);

    DEBUG("HCA Initialized\n");

    return 0;

out_err:
    destroy_cmd_mailbox(cmd);
    return -1;
}


static int 
mlx3_init_cmpt_table (struct mlx3_ib * mlx, 
		struct mlx3_init_hca_param * init_hca)
{
	uint64_t cmpt_base     = init_hca->cmpt_base;
	uint64_t cmpt_entry_sz = mlx->caps->c_mpt_entry_sz;
	int num_eqs            = init_hca->num_eqs;
	int err;

	err =	mlx3_init_icm_table(mlx, 
                                &(mlx->qp_table.cmpt_table),
                                cmpt_entry_sz, 
                                init_hca->num_qps,
                                1 << mlx->caps->log2_rsvd_qps, 
                                cmpt_entry_sz * init_hca->num_qps, 
                                cmpt_base);
	
	if (err) {
		ERROR("Map cMPT QP Failed\n");
		return -1;
	}

	DEBUG("Mapped QP cMPT table\n");

	err =	mlx3_init_icm_table(mlx, 
                                &(mlx->srq_table.cmpt_table),
                                cmpt_entry_sz,
                                init_hca->num_srqs, 
                                1 << mlx->caps->log2_rsvd_srqs,
                                cmpt_entry_sz * init_hca->num_srqs,
                                cmpt_base + ((uint64_t)(MLX3_CMPT_TYPE_SRQ * cmpt_entry_sz) << MLX3_CMPT_SHIFT));

	if (err) {
		ERROR("Map cMPT SRQ Failed\n");
		return -1;
	}

	DEBUG("Mapped SRQ cMPT table\n");

	err =	mlx3_init_icm_table(mlx, 
                                &(mlx->cq_table.cmpt_table),
                                cmpt_entry_sz, 
                                init_hca->num_cqs,
                                1 << mlx->caps->log2_rsvd_cqs,
                                cmpt_entry_sz * init_hca->num_cqs,
                                cmpt_base + ((uint64_t)(MLX3_CMPT_TYPE_CQ * cmpt_entry_sz) << MLX3_CMPT_SHIFT));
	
	if (err) {
		ERROR("Map CQ cMPT Failed\n");
		return -1;
	}

	DEBUG("Mapped CQ cMPT table\n");

	err =	mlx3_init_icm_table(mlx, 
                                &(mlx->eq_table.cmpt_table),
                                cmpt_entry_sz, 
                                num_eqs, 
                                num_eqs, 
                                cmpt_entry_sz * num_eqs,
                                cmpt_base + ((uint64_t)(MLX3_CMPT_TYPE_EQ * cmpt_entry_sz) << MLX3_CMPT_SHIFT));
	
	if (err) {
		ERROR("Map EQ Failed\n");
		return -1;
	}

	DEBUG("Mapped EQ cMPT table\n");

	return 0;
}


static int 
mlx3_map_icm_tables (struct mlx3_ib * mlx, 
		struct mlx3_init_hca_param * init_hca)
{
	int err;
	void * data    = NULL;
	void * eq_data = NULL;

	if (mlx3_init_cmpt_table(mlx, init_hca)) {
		ERROR("Could not init cMPT tables\n");
		return -1;
	}

	int num_eqs = init_hca->num_eqs;

	err =	mlx3_init_icm_table(mlx, 
                                &(mlx->eq_table.table),
                                mlx->caps->eqc_entry_sz,
                                num_eqs, 
                                num_eqs, 
                                mlx->caps->eqc_entry_sz * num_eqs,
                                init_hca->eqc_base);

	if (err != 0) {
		ERROR("Map EQ Failed\n");
		return -1;
	}

	/**
	 *  Assuming Cache Line is 64 Bytes.  Reserved MTT entries must be aligned up to a cacheline
	 *  boundary, since the FW will write to them, while the
	 *  driver writes to all other MTT entries. (The variable
	 *  dev->caps.mtt_entry_sz below is really the MTT segment size, not the raw entry size)
	 **/
    mlx->caps->reserved_mtts = ALIGN((1 << mlx->caps->log2_rsvd_mtts) * mlx->caps->mtt_entry_sz, 64) / mlx->caps->mtt_entry_sz;
    mlx->mr_table.offset = 0;	

	err = mlx3_init_icm_table(mlx, 
                                &(mlx->mr_table.mtt_table),
                                mlx->caps->mtt_entry_sz,
                                init_hca->num_mtts,
                                mlx->caps->reserved_mtts, //1 << mlx->caps->log2_rsvd_mtts, 
                                mlx->caps->mtt_entry_sz * init_hca->num_mtts,
                                init_hca->mtt_base);

	if (err != 0) {
		ERROR("Map mtt Failed\n");
		return -1;
	}

	err = mlx3_init_icm_table(mlx, 
                                &(mlx->mr_table.dmpt_table),
                                mlx->caps->d_mpt_entry_sz,
                                init_hca->num_mpts,
                                1 << mlx->caps->log2_rsvd_mrws, 
                                mlx->caps->d_mpt_entry_sz * init_hca->num_mpts,
                                init_hca->dmpt_base);

	if (err != 0) {
		ERROR("Map Dmpt Failed\n");
		return -1;
	}

	err = mlx3_init_icm_table(mlx, 
                                &(mlx->qp_table.qp_table),
                                mlx->caps->qpc_entry_sz,
                                init_hca->num_qps,
                                1 << mlx->caps->log2_rsvd_qps, 
                                mlx->caps->qpc_entry_sz * init_hca->num_qps, 
                                init_hca->qpc_base); 

	if (err != 0) {
		ERROR("Map QP Failed\n");
		return -1;
	}

	err = mlx3_init_icm_table(mlx, 
                                &(mlx->qp_table.auxc_table),
                                mlx->caps->aux_entry_size,
                                init_hca->num_qps,
                                1 << mlx->caps->log2_rsvd_qps,
                                mlx->caps->aux_entry_size * init_hca->num_qps, 
                                init_hca->auxc_base);

	if (err != 0) {
        ERROR("Map Aux Failed\n");
        return -1;
	}

	err = mlx3_init_icm_table(mlx, 
                                &(mlx->qp_table.altc_table),
                                mlx->caps->altc_entry_size,
                                init_hca->num_qps,
                                1 << mlx->caps->log2_rsvd_qps, 
                                mlx->caps->altc_entry_size * init_hca->num_qps, 
                                init_hca->altc_base);

	if (err != 0) {
		ERROR("Map Altc Failed\n");
		return -1;
	}

	err = mlx3_init_icm_table(mlx, 
                                &(mlx->qp_table.rdmarc_table),	
                                mlx->caps->rdmardc_entry_sz << mlx->qp_table.rdmarc_shift,
                                init_hca->num_qps,
                                1 << mlx->caps->log2_rsvd_qps,
                                (mlx->caps->rdmardc_entry_sz << mlx->qp_table.rdmarc_shift) * init_hca->num_qps, 
                                init_hca->rdmarc_base);

	if (err != 0) {
		ERROR("Map RDMA Failed\n");
		return -1;
	}

	err = mlx3_init_icm_table(mlx, 
                                &(mlx->cq_table.table),
                                mlx->caps->cqc_entry_sz,
                                init_hca->num_cqs,
                                1 << mlx->caps->log2_rsvd_cqs, 
                                mlx->caps->cqc_entry_sz * init_hca->num_cqs,
                                init_hca->cqc_base);

	if (err != 0) {
		ERROR("Map CQ Failed\n");
		return -1;
	}

	err = mlx3_init_icm_table(mlx, 
				              &(mlx->srq_table.table),
				              mlx->caps->srq_entry_sz,
			                  init_hca->num_srqs,
				              1 << mlx->caps->log2_rsvd_srqs,
                              mlx->caps->srq_entry_sz * init_hca->num_srqs,
				              init_hca->srqc_base);

        if (err != 0) {
                ERROR("Map SRQ Failed\n");
                return -1;;
        }

	err = mlx3_init_icm_table(mlx, 
                                &(mlx->mcg_table.table),
                                1 << get_mcg_log_entry_size(),
                                init_hca->num_mgms + init_hca->num_amgms,
                                init_hca->num_mgms + init_hca->num_amgms,
                                1 << get_mcg_log_entry_size() * (init_hca->num_mgms + init_hca->num_amgms),
                                init_hca->mc_base);

	if (err != 0) {
		ERROR("Map MCG Failed\n");
		return -1;
	}

	DEBUG("ICM tables mapped successfully (total size: %d MB\n", mlx->icm_size >> 20);

	return 0;
}

static int 
mlx3_map_icm_aux (struct mlx3_ib * mlx, uint64_t pages)
{
	int i, cnt, align;
	uint64_t ptr;
	int err = 0;
	uint32_t pa_h;
	uint32_t pa_l;

	mlx3_cmd_box_t * cmd = create_cmd_mailbox(mlx);

    __mzero_checked(mlx->fw_info->aux_icm, PAGE_SIZE_4KB*pages, 
            "Could not allocate pages for ICM AUX area\n", goto out_err);

	ptr = (uint64_t)mlx->fw_info->aux_icm;

	align = __ffs(ptr);

	if (align > ilog2(PAGE_SIZE_4KB)) {
		DEBUG("%s: Alignment greater than max chunk size, defaulting to 256KB\n", __func__);
		align = ilog2(PAGE_SIZE_4KB);
	}

	cnt = (pages * PAGE_SIZE_4KB) / (1 << align) +
		((pages * PAGE_SIZE_4KB) % (1 << align) == 0 ? 0 : 1);

	for (i = 0; i < cnt; i++) {

		pa_h = (uint32_t)(ptr >> 32);
		pa_l = (uint32_t)((ptr & 0xffffffffu) | (align - 12));

		MLX3_SETL(cmd->buf, 0x8, pa_h);
		MLX3_SETL(cmd->buf, 0xC, pa_l);

		err = mlx3_mailbox_cmd(mlx,
				cmd->buf,
				NA,
				1,
				NA,
				CMD_MAP_ICM_AUX,
				CMD_TIME_CLASS_A);

		if (err) {
			ERROR("Could not map ICM AUX area\n");
			goto out_err1;
		}

		ptr += 1 << align;
	}

	DEBUG("Mapped %d pages for ICM AUX area\n", pages);

	destroy_cmd_mailbox(cmd);

	return 0;

out_err1:
	free(mlx->fw_info->aux_icm);
out_err:
	destroy_cmd_mailbox(cmd);
	return -1;
}


static int
mlx3_map_fw_area (struct mlx3_ib * mlx)
{
	mlx3_cmd_box_t * cmd = create_cmd_mailbox(mlx);
	uint32_t pa_h;
	uint32_t pa_l;
	uint64_t ptr;
	int err = 0;
	int i, cnt, pages, align;

	if (!cmd) {
		ERROR("Could not create cmd mailbox\n");
		return -1;
	}

	pages = mlx->fw_info->pages;

    __mzero_checked(mlx->fw_info->data, PAGE_SIZE_4KB*pages, 
                    "Could not allocate pages for firmware area\n", goto out_err);

	ptr = (uint64_t)mlx->fw_info->data;

	align = __ffs(ptr);

	if (align > MLX3_MAX_CHUNK_LOG2) {
		DEBUG("Alignment greater than max chunk size, defaulting to 256KB\n");
		align = MLX3_MAX_CHUNK_LOG2;
	}

	cnt = (pages*PAGE_SIZE_4KB) / (1<<align) +
		((pages*PAGE_SIZE_4KB) % (1<<align) == 0 ? 0 : 1);

	/* TODO: we can batch as many vpm entries as fit in a mailbox (1 page)
	 * rather than 1 chunk per mailbox, will make bootup faster
	 */
	for (i = 0; i < cnt; i++) {

		pa_h = (uint32_t)(ptr >> 32);
		pa_l = (uint32_t)((ptr & 0xffffffffu) | (align - 12));

		MLX3_SETL(cmd->buf, 0x8, pa_h);
		MLX3_SETL(cmd->buf, 0xC, pa_l);

		err = mlx3_mailbox_cmd(mlx, 
				cmd->buf, 
				0, 
				1, 
				0, 
				CMD_MAP_FA, 
				CMD_TIME_CLASS_A);

		if (err) {
			ERROR("Could not map firmware area\n");
			goto out_err;
		}

		ptr += 1 << align;
	}

	DEBUG("Mapped %d pages for firmware area\n", pages);

	destroy_cmd_mailbox(cmd);

	return 0;

out_err:
	destroy_cmd_mailbox(cmd);
	return -1;
}


static int 
mlx3_query_fw (struct mlx3_ib * mlx)
{
	int err = 0;
	uint8_t field;
	mlx3_cmd_box_t * cmd = create_cmd_mailbox(mlx);

	if (!cmd) {
		ERROR("Could not create cmd mailbox\n");
		return -1;
	}

	err = mlx3_mailbox_cmd(mlx, 
			0, 
			cmd->buf, 
			0, 
			0, 
			CMD_QUERY_FW, 
			CMD_TIME_CLASS_A);

	if (err) {
		ERROR("Could not query firmware\n");
		goto out_err;
	}

    __mzero_checked(mlx->fw_info, sizeof(struct mlx3_fw_info), 
                    "Could not allocate FW info\n", goto out_err);

	mlx->fw_info->pages      = MLX3_GETL(cmd->buf, 0) >> 16;
	mlx->fw_info->maj        = MLX3_GETL(cmd->buf, 0) & 0xffff;
	mlx->fw_info->min        = MLX3_GETL(cmd->buf, 4) & 0xffff;
	mlx->fw_info->submin     = MLX3_GETL(cmd->buf, 4) >> 16;
	mlx->fw_info->cmd_ix_rev = MLX3_GETL(cmd->buf, 8) & 0xffff;

	MLX3_GET(mlx->fw_info->clr_base, cmd->buf, 0x20);

	MLX3_GET(mlx->fw_info->clr_int_bar, cmd->buf, 0x28);

	mlx->fw_info->clr_int_bar = (mlx->fw_info->clr_int_bar >> 6) * 2;

	DEBUG("Firmware Interrupt Bar  %d\n", mlx->fw_info->clr_int_bar);	
	DEBUG("Firmware Interrupt base %x\n", mlx->fw_info->clr_base);
	DEBUG("Firmware version detected: %d.%d.%d\n", mlx->fw_info->maj, 
			mlx->fw_info->min, 
			mlx->fw_info->submin);
	DEBUG("Firmware cmd interface revision: %d\n", mlx->fw_info->cmd_ix_rev);
	DEBUG("Firmware size: %d.%d KB\n", (mlx->fw_info->pages*PAGE_SIZE_4KB) / 1024, 
			(mlx->fw_info->pages*PAGE_SIZE_4KB) % 1024);

	destroy_cmd_mailbox(cmd);

	return 0;

out_err:
    destroy_cmd_mailbox(cmd);
    return -1;
}


static inline int
mlx3_check_ownership (struct mlx3_ib * mlx)
{
	return !READ_MEM(mlx, MLX_OWNER_BASE);
}


static int 
mlx3_status_to_errno (uint8_t status)
{
        static const int trans_table[] = {
                [CMD_STAT_INTERNAL_ERR]   = -EIO,
                [CMD_STAT_BAD_OP]         = -EPERM,
                [CMD_STAT_BAD_PARAM]      = -EINVAL,
                [CMD_STAT_BAD_SYS_STATE]  = -ENXIO,
                [CMD_STAT_BAD_RESOURCE]   = -EBADF,
                [CMD_STAT_RESOURCE_BUSY]  = -EBUSY,
                [CMD_STAT_EXCEED_LIM]     = -ENOMEM,
                [CMD_STAT_BAD_RES_STATE]  = -EBADF,
                [CMD_STAT_BAD_INDEX]      = -EBADF,
                [CMD_STAT_BAD_NVMEM]      = -EFAULT,
                [CMD_STAT_ICM_ERROR]      = -ENFILE,
                [CMD_STAT_BAD_QP_STATE]   = -EINVAL,
                [CMD_STAT_BAD_SEG_PARAM]  = -EFAULT,
                [CMD_STAT_REG_BOUND]      = -EBUSY,
                [CMD_STAT_LAM_NOT_PRE]    = -EAGAIN,
                [CMD_STAT_BAD_PKT]        = -EINVAL,
                [CMD_STAT_BAD_SIZE]       = -ENOMEM,
                [CMD_STAT_MULTI_FUNC_REQ] = -EACCES,
        };

        if (status >= ARRAY_SIZE(trans_table) ||
            (status != CMD_STAT_OK && trans_table[status] == 0))
                return -EIO;

        return trans_table[status];
}

#define MLX3_ASYNC_EVENT_MASK ((1ull << MLX3_EVENT_TYPE_PATH_MIG)           | \
		(1ull << MLX3_EVENT_TYPE_COMM_EST)           | \
		(1ull << MLX3_EVENT_TYPE_SQ_DRAINED)         | \
		(1ull << MLX3_EVENT_TYPE_CQ_ERROR)           | \
		(1ull << MLX3_EVENT_TYPE_WQ_CATAS_ERROR)     | \
		(1ull << MLX3_EVENT_TYPE_EEC_CATAS_ERROR)    | \
		(1ull << MLX3_EVENT_TYPE_PATH_MIG_FAILED)    | \
		(1ull << MLX3_EVENT_TYPE_WQ_INVAL_REQ_ERROR) | \
		(1ull << MLX3_EVENT_TYPE_WQ_ACCESS_ERROR)    | \
		(1ull << MLX3_EVENT_TYPE_PORT_CHANGE)        | \
		(1ull << MLX3_EVENT_TYPE_ECC_DETECT)         | \
		(1ull << MLX3_EVENT_TYPE_SRQ_CATAS_ERROR)    | \
		(1ull << MLX3_EVENT_TYPE_SRQ_QP_LAST_WQE)    | \
		(1ull << MLX3_EVENT_TYPE_SRQ_LIMIT)          | \
		(1ull << MLX3_EVENT_TYPE_CMD)                | \
		(1ull << MLX3_EVENT_TYPE_OP_REQUIRED)        | \
		(1ull << MLX3_EVENT_TYPE_COMM_CHANNEL)       | \
		(1ull << MLX3_EVENT_TYPE_FLR_EVENT)          | \
		(1ull << MLX3_EVENT_TYPE_FATAL_WARNING))


static void 
mlx3_cmd_event (struct mlx3_ib * mlx, 
                uint16_t token, 
                uint8_t status, 
                uint64_t out_param)
{
	struct mlx3_cmd_context * context = NULL;

    __mzero_checked(context, sizeof(struct mlx3_cmd_context), "Could not allocate command context\n", return);

	/* TODO:Need to save existing token while ssuing command and compare tokens for validation */
//	if (token != context->token)

	context->fw_status = status;
	context->result    = mlx3_status_to_errno(status);
	context->out_param = out_param;

    DEBUG("Received CMD Interface Completion Event:\n");
	DEBUG("  Token %x\n",   token);
	DEBUG("  FW STATUS: 0x%x\n", context->fw_status);
	DEBUG("  Result: 0x%x\n",    context->result);
	DEBUG("  Out Param 0x%x\n", context->out_param);
}


static inline void
writeq (uint64_t b, uint64_t *addr)
{
    *(volatile uint64_t*)addr = b;
}


static inline void 
writel (uint32_t b, uint32_t *addr)
{
        *(volatile uint32_t*)  addr = b;
}

#define __force __attribute__((force))

static inline void
__writel (unsigned int val, volatile uint32_t *addr)
{
        *(volatile unsigned int*) addr = val;
}



static void
dump_link_info (uint32_t linfo)
{
    int i;
    const char * ls[] = {"2.5Gbps", "5Gbps", "10Gbps", "14Gbps"};
    const char * els[] = {[0x0] = "10GbE (XAUI)", [0x1] = "10GbE (XFI)", [0x2] = "1GbE", [0xf] = "Other", [0x40] = "40GbE"};
    const char * ibpw[] = {"1x", "4x", "8x", "12x"};

    DEBUG("  Supported IB Link speeds:\n");
    for (i = 0; i < 4; i++) {
        if ((linfo >> 24) & (1u << i)) {
            DEBUG("  |-> %s\n", ls[i]);
        }
    }

    DEBUG("  IB Port widths supported:\n");
    for (i = 0; i < 4; i++) {
        if (((linfo >> 8) & 0xff) & (1u << i)) {
            DEBUG("  |-> %s\n", ibpw[i]);
        }
    }

    DEBUG("  Max GIDs per port (IB): %d\n", 1 << ((linfo >> 4) & 0xf));
    DEBUG("  Max PKey table size (IB): %d\n", 1 << (linfo & 0xf));

    DEBUG("  Ethernet Link speed: [%s]\n", els[(linfo >> 16) & 0xff]);
}


static int 
mlx3_query_port (struct mlx3_ib *mlx, int port, struct mlx3_port_cap *port_cap, int dump_port)
{
    mlx3_cmd_box_t  *mailbox;
    uint32_t *outbox;
    uint8_t field;
    uint32_t field32;
    int err;

    mailbox = create_cmd_mailbox(mlx);
    if (!mailbox) {
        ERROR("Could not create mailbox\n"); 
        return -1;
    }

    outbox = (uint32_t*)mailbox->buf;
    memset(port_cap, 0, sizeof(struct mlx3_port_cap));
#define QUERY_PORT_SUPPORTED_TYPE_OFFSET        0x00
#define QUERY_PORT_MTU_OFFSET                   0x01
#define QUERY_PORT_ETH_MTU_OFFSET               0x02
#define QUERY_PORT_WIDTH_OFFSET                 0x06
#define QUERY_PORT_MAX_GID_PKEY_OFFSET          0x07
#define QUERY_PORT_MAX_MACVLAN_OFFSET           0x0a
#define QUERY_PORT_MAX_VL_OFFSET                0x0b
#define QUERY_PORT_MAC_OFFSET                   0x10
#define QUERY_PORT_TRANS_VENDOR_OFFSET          0x18
#define QUERY_PORT_WAVELENGTH_OFFSET            0x1c
#define QUERY_PORT_TRANS_CODE_OFFSET            0x20

    err = mlx3_mailbox_cmd(mlx, 0, mailbox->buf, port, 0, CMD_QUERY_PORT,
            CMD_TIME_CLASS_A);

    if (err)
        goto out;

    MLX3_GET(field, outbox, QUERY_PORT_SUPPORTED_TYPE_OFFSET);
    port_cap->link_state           = (field & 0x80) >> 7;
    port_cap->supported_port_types = field & 3;
    port_cap->suggested_type       = (field >> 3) & 1;
    port_cap->default_sense        = (field >> 4) & 1;
    port_cap->dmfs_optimized_state = (field >> 5) & 1;
    MLX3_GET(field, outbox, QUERY_PORT_MTU_OFFSET);
    port_cap->ib_mtu           = field & 0xf;
    MLX3_GET(field, outbox, QUERY_PORT_WIDTH_OFFSET);
    port_cap->max_port_width = field & 0xf;
    MLX3_GET(field, outbox, QUERY_PORT_MAX_GID_PKEY_OFFSET);
    port_cap->max_gids  = 1 << (field >> 4);
    port_cap->max_pkeys = 1 << (field & 0xf);
    MLX3_GET(field, outbox, QUERY_PORT_MAX_VL_OFFSET);
    port_cap->max_vl     = field & 0xf;
    port_cap->max_tc_eth = field >> 4;
    MLX3_GET(field, outbox, QUERY_PORT_MAX_MACVLAN_OFFSET);
    port_cap->log_max_macs  = field & 0xf;
    port_cap->log_max_vlans = field >> 4;
    MLX3_GET(port_cap->eth_mtu, outbox, QUERY_PORT_ETH_MTU_OFFSET);

    uint32_t mac_hi;
    uint32_t mac_lo;
    MLX3_GET(mac_hi, outbox, QUERY_PORT_MAC_OFFSET);
    MLX3_GET(mac_lo, outbox, QUERY_PORT_MAC_OFFSET + 4);
    port_cap->def_mac = (uint64_t)mac_lo | (((uint64_t)mac_hi & 0xffff) << 32);

    MLX3_GET(field32, outbox, QUERY_PORT_TRANS_VENDOR_OFFSET);
    port_cap->trans_type = field32 >> 24;
    port_cap->vendor_oui = field32 & 0xffffff;
    MLX3_GET(port_cap->wavelength, outbox, QUERY_PORT_WAVELENGTH_OFFSET);
    MLX3_GET(port_cap->trans_code, outbox, QUERY_PORT_TRANS_CODE_OFFSET);

    if (dump_port) {
    DEBUG("Port Info:\n");

    DEBUG("  Supported Link Types:\n");
    if (port_cap->supported_port_types & 1) {
        DEBUG("  |-> IB\n");                            
    }

    if (port_cap->supported_port_types & 2) {
        DEBUG("  |-> Ethernet\n");                            
    }

    uint32_t link_info;
    MLX3_GET(link_info, outbox, 0x04);
    dump_link_info(link_info);

    DEBUG("  Link State: [%s] Link %d\n", port_cap->link_state ? "Up" : "Down", port_cap->link_state);
    DEBUG("  IB MTU %d (type=%d)\n", 1 <<  (port_cap->ib_mtu + 7), port_cap->ib_mtu);
    DEBUG("  Eth MTU %d\n", port_cap->eth_mtu); 
    DEBUG("  Port MAC %012X\n", port_cap->def_mac);
    }
out:
    destroy_cmd_mailbox(mailbox);
    return err;
}


static void 
eq_set_ci (struct mlx3_eq * eq, int req_not)
{
 	//need a barrirer

//	DEBUG("Offset %d\n", mlx->eq_table.eq->eqn / 4);
	writel((uint32_t) bswap32((eq->cons_index & 0xffffff) |req_not << 31), eq->doorbell);

//	DEBUG("Clear Interrupts \n");
}


static struct mlx3_eqe *
get_eqe (struct mlx3_eq *eq, uint32_t entry, uint8_t eqe_factor,
                                uint8_t eqe_size)
{
    /* (entry & (eq->nent - 1)) gives us a cyclic array */
    unsigned long offset = (entry & (eq->nentry - 1)) * eqe_size;
    /* CX3 is capable of extending the EQE from 32 to 64 bytes with
     * strides of 64B,128B and 256B.
     * When 64B EQE is used, the first (in the lower addresses)
     * 32 bytes in the 64 byte EQE are reserved and the next 32 bytes
     * contain the legacy EQE information.
     * In all other cases, the first 32B contains the legacy EQE info.
     */

    // TODO: should be mod size of EQ
    return eq->buffer + (offset + (eqe_factor ? MLX3_EQ_ENTRY_SIZE : 0)) % PAGE_SIZE_4KB;
}


static struct mlx3_eqe *
next_eqe_sw (struct mlx3_eq *eq, uint8_t eqe_factor, uint8_t size)
{
    struct mlx3_eqe *eqe = get_eqe(eq, eq->cons_index, eqe_factor, size);
    return !!(eqe->owner & 0x80) ^ !!(eq->cons_index & eq->nentry) ? NULL : eqe;
}


static inline int
cqe_is_sw (struct mlx3_cq * cq, struct mlx3_cqe * cqe)
{
    return (cqe->owner_sr_opcode & 0X80) == (cq->cons_index & cq->size);
}


static struct mlx3_cqe *
get_cqe (struct mlx3_cq * cq, uint32_t index)
{
    unsigned long offset = (index & (cq->nentry -1)) * CQE_SIZE;
    return (struct mlx3_cqe*)(cq->buffer + (offset ));
}


static struct mlx3_cqe *
next_cqe_sw (struct mlx3_cq * cq)
{
    struct mlx3_cqe * cqe = get_cqe(cq, cq->cons_index);
    return cqe_is_sw(cq, cqe) ? cqe : NULL;
}

enum {
    MLX3_CQ_DB_REQ_NOT_SOL = 1 << 24,
    MLX3_CQ_DB_REQ_NOT     = 2 << 24
};

#define MLX3_CQ_DOORBELL      0x20

static inline void 
mlx3_cq_arm (struct mlx3_cq * cq, uint32_t cmd, void * uar_page, int cons_index)
{
    uint32_t doorbell[2];
    uint32_t sn;
    uint32_t ci;

    sn = cq->arm_sn & 3;
    ci = cons_index & 0xffffff;

    *cq->arm_db = bswap32(sn << 28 | cmd | ci);

    doorbell[0] = bswap32(sn << 28 | cmd | cq->cqn);
    doorbell[1] = bswap32(ci);

//    writeq(*(uint64_t *)doorbell, uar_page + MLX3_CQ_DOORBELL);
    writel(doorbell[0], uar_page + MLX3_CQ_DOORBELL);
    writel(doorbell[1], uar_page + MLX3_CQ_DOORBELL + 0x4);
}

static const char * cqe_err_syndromes[] = {
    [0x01] = "Local Length Error",
    [0x02] = "Local QP Operation Error",
    [0x04] = "Local Protection Error",
    [0x05] = "Work Request Flushed Error",
    [0x06] = "Memory Window Bind Error",
    [0x10] = "Bad Response Error",
    [0x11] = "Local Access Error",
    [0x12] = "Remote Invalid Request Error",
    [0x13] = "Remote Access Error",
    [0x14] = "Remote Operation Error",
    [0x15] = "Transport Retry Counter Exceeded",
    [0x16] = "RNR Retry Counter Exceeded",
    [0x22] = "Remote Aborted Error",
};

#define SEND_COMPLETION    1
#define RECEIVE_COMPLETION 0
static char*
print_opcode(uint8_t opcode, uint8_t type)
{

    char* s;
    if (type) {
        switch (opcode) {
            case MLX3_OPCODE_SEND:
                s = "SEND";
                break;
            case MLX3_OPCODE_NOP:
                s = "NOP";
                break;
            case MLX3_OPCODE_SEND_INVAL:
                s = "SEND ITH INVAL"; 
                break;
            case MLX3_OPCODE_RDMA_WRITE:
                s = "RDMA WRITE";
                break; 
            default:
                s = "Need to support the  new opcode";
                break;
        };
    }
    else {

        switch (opcode) {
            case MLX3_RECV_OPCODE_SEND:
                s = "SEND";
                break;
            case MLX3_RECV_OPCODE_SEND_IMM:
                s = "SEND WITH IMM";
                break;
            case MLX3_RECV_OPCODE_SEND_INVAL:
                s = "SEND WITH INVAL"; 
                break; 
            default:
                s = "Need to support the new opcode";
                break;
        };
    }
    return s;
}

enum ib_qp_state {
    IB_QPS_RESET,
    IB_QPS_INIT,
    IB_QPS_RTR,
    IB_QPS_RTS,
    IB_QPS_SQD,
    IB_QPS_SQE,
    IB_QPS_ERR
};


static inline enum ib_qp_state 
to_ib_qp_state (enum mlx3_qp_state mlx3_state)
{
    switch (mlx3_state) {
        case MLX3_QP_STATE_RST:      return IB_QPS_RESET;
        case MLX3_QP_STATE_INIT:     return IB_QPS_INIT;
        case MLX3_QP_STATE_RTR:      return IB_QPS_RTR;
        case MLX3_QP_STATE_RTS:      return IB_QPS_RTS;
        case MLX3_QP_STATE_SQ_DRAINING:
        case MLX3_QP_STATE_SQD:      return IB_QPS_SQD;
        case MLX3_QP_STATE_SQER:     return IB_QPS_SQE;
        case MLX3_QP_STATE_ERR:      return IB_QPS_ERR;
        default:             return -1;
    }
}


static int
mlx3_qp_query (struct mlx3_ib * mlx, struct mlx3_qp_context * context, int qpn)
{
    mlx3_cmd_box_t * mailbox = NULL;
    int err;

    mailbox = create_cmd_mailbox(mlx);
    if (!mailbox) {
        ERROR("Could not create mailbox \n");
        return -1;
    }

    err = mlx3_mailbox_cmd(mlx, 0, mailbox->buf, qpn, 0,
            MLX3_CMD_QUERY_QP, CMD_TIME_CLASS_A);

    if (err) {
        ERROR("Could not query QP\n");
        goto out_err;
    }

    memcpy(context, (void *)(mailbox->buf + 8), sizeof(*context));

    destroy_cmd_mailbox(mailbox);
    
    return 0;

out_err:
    destroy_cmd_mailbox(mailbox);
    return -1;
}


static int
get_qp_send_counter (struct mlx3_ib * mlx, struct mlx3_qp * qp)
{
    struct mlx3_qp_context * context = NULL;
    uint16_t send_counter = 0;
    int err;

    mzero_checked(context, sizeof(struct mlx3_qp_context), "QP context skeleton");

    err = mlx3_qp_query(mlx, context, qp->qpn);

    if (err) {
        ERROR("QUERY QP failed with err %d in %s\n", err, __func__);
        goto out_err;
    }
    
    send_counter = bswap16(context->sq_wqe_counter);

    free(context);

    return send_counter;

out_err:
    free(context);
    return -1;
}
    
    
static int
mlx3_ib_query_qp (struct mlx3_ib * mlx, struct mlx3_qp * qp) {
    
    struct mlx3_qp_context * context = NULL;
    int err;
    int mlx3_state;

    mzero_checked(context, sizeof(struct mlx3_qp_context), "QP context skeleton");

    err = mlx3_qp_query(mlx, context, qp->qpn);

    if (err) {
        ERROR("Query QP Failed with err %d\n", err);
        goto out_err;
    }

    mlx3_state = bswap32(context->flags) >> 28;

    DEBUG("QP State %d\n", to_ib_qp_state(mlx3_state));
    DEBUG("MTU %d \n", context->mtu_msgmax >> 5);
    DEBUG("QKEY %x\n", bswap32(context->qkey));
    DEBUG("QP Number %d\n", bswap32(context->local_qpn));
    DEBUG("Send Counter %d\n", bswap16(context->sq_wqe_counter));
    DEBUG("Recieve Counter %d\n", bswap16(context->rq_wqe_counter));

    free(context);

    return 0;

out_err:
    free(context);
    return -1;
}


static inline void 
mlx3_update_rx_prod_db(struct mlx3_rx_ring *ring)
{
    ring->db_rec[0] = bswap32(ring->prod & 0xffff);
}


static void
dump_sq (struct mlx3_ib * mlx, struct mlx3_tx_ring * tx)
{
    // TODO
    ERROR("%s NOT IMPLEMENTED\n", __func__);
}

#define LINE_SZ 16
#define GROUP_SZ 8


// @sz - size of packet in bytes
static void
dump_packet (struct mlx3_ib * mlx, void * buf, int sz)
{
    int i = 0;

     unsigned char * cursor = (unsigned char*) buf;

    DEBUG("Packet Dump (Big Endian):\n");
    DEBUG("0x%04x: ", i);

    while (i < sz) {

        nk_vc_printf("%02X", cursor[i++]);

        if (!(i % LINE_SZ)) {
            nk_vc_printf("\n");
            DEBUG("0x%04X: ", i);
        } else if (!(i % GROUP_SZ)) {
            nk_vc_printf("  ");
        } else {
            nk_vc_printf(" ");
        }
    }

    nk_vc_printf("\n");
}


static void
dump_rx_desc (struct mlx3_ib * mlx, struct mlx3_rx_desc * rxd, int idx)
{
    uint32_t byte_count = bswap32(rxd->data[0].byte_count);
    uint64_t addr       = bswap64(rxd->data[0].addr);

    // only print out valid descriptors (that have packets)
    if (byte_count == 0) {
        return;
    }

    DEBUG("RQ RX Desc Dump (desc %d):\n", idx);
    DEBUG("Byte Count: %d\n", byte_count);
    DEBUG("LKey:       0x%08x\n",  bswap32(rxd->data[0].lkey));
    DEBUG("Packet Addr: %p\n", addr);

    dump_packet(mlx, (void*)addr, byte_count);
}


static void
dump_rq_wqe (struct mlx3_ib * mlx, struct mlx3_rx_ring * rx, int idx)
{
	struct mlx3_rx_desc * rxd = (struct mlx3_rx_desc*)(rx->buf + rx->wqe_size * idx);
	int rxds_per_wqe = rx->wqe_size / 16;
    int i;

    DEBUG("RQ WQE Dump (idx=%d):\n", idx);

	for (i = 0; i < rxds_per_wqe; i++) {
        if (rxd->data[i].byte_count != 0) {
            dump_rx_desc(mlx, (struct mlx3_rx_desc*)&(rxd->data[i]), i);
        }
    }
}


static inline void
dump_rq (struct mlx3_ib * mlx, struct mlx3_rx_ring * rx) 
{
    int i;

    for (i = 0; i < rx->size; i++) {
        dump_rq_wqe(mlx, rx, i);
    }
}


static inline void
dump_qp (struct mlx3_ib * mlx, struct mlx3_qp * qp)
{
    dump_rq(mlx, qp->rx);
 //   dump_sq(mlx, qp->tx);
}


static inline void 
cq_set_ci(struct mlx3_cq *cq)
{
    *cq->set_ci_db = bswap32(cq->cons_index & 0xffffff);
}



static int 
mlx3_SW2HW_MPT(struct mlx3_ib * mlx, uint64_t buf, int mpt_index)
{
    return mlx3_mailbox_cmd(mlx, 
                            buf, 
                            0, 
                            mpt_index,
                            0, 
                            CMD_SW2HW_MPT, 
                            CMD_TIME_CLASS_A);
}


static uint32_t 
key_to_hw_index(uint32_t key)
{
    return (key << 24) | (key >> 8);
}

/*
 * Gives the addres of the *first* entry
 * in the MTT that is not reserved by firmware,
 * *NOT* the base address of the MTT passed to the
 * card in INIT_HCA.
 *
 * NOTE: this has the side effect of incrementing
 * the next available mtt entry pointer
 */
static uint64_t
mlx3_alloc_mtt (struct mlx3_ib * mlx, int nent)
{
	uint64_t addr = (mlx->caps->reserved_mtts * mlx->caps->mtt_entry_sz) + ((mlx->mr_table.offset) * mlx->caps->mtt_entry_sz);
    mlx->mr_table.offset += nent;
	return addr;
}


#define MLX3_MTT_FLAG_PRESENT 1

static int
mlx3_WRITE_MTT (struct mlx3_ib * mlx, 
                uint64_t mtt_addr, 
                uint64_t* buf, 
                int nentry)
{
	uint64_t* inbox;
	int err = 0;
	int i;

	mlx3_cmd_box_t * cmd = create_cmd_mailbox(mlx);

	if (!cmd) {
		ERROR("Could not create cmd mailbox\n");
		return -1;
	}

	inbox = (uint64_t *)cmd->buf;

	inbox[0] = bswap64(mtt_addr);  //flag
	inbox[1] = 0;

	for (i = 0; i < nentry; i++) {
		inbox[i + 2] = bswap64(((uint64_t)buf + (i * PAGE_SIZE_4KB)) | MLX3_MTT_FLAG_PRESENT);        
	}

	err = mlx3_mailbox_cmd(
            mlx,
            cmd->buf,    	      // inparam
            NA,          	      // outparam      
            nentry,      	      // inmod   
            NA,                     // opmod                  
            CMD_WRITE_MTT ,         // op
            CMD_TIME_CLASS_A        // time  
			);

	if (err) {
		ERROR("Cannot write MTT\n");
		goto out_err;
        }

	destroy_cmd_mailbox(cmd);

	return 0;

out_err:
	destroy_cmd_mailbox(cmd);
	return -1;
}


#define MLX3_MPT_FLAG_SW_OWNS       (0xfUL << 28)
#define MLX3_MPT_FLAG_FREE      (0x3UL << 28)
#define MLX3_MPT_FLAG_REGION       (1 <<  8)
#define MLX3_MPT_FLAG_MIO          (1 << 17)
#define MLX3_MPT_FLAG_LOCAL_READ   (1 << 10) 
#define MLX3_MPT_FLAG_LOCAL_WRITE  (1 << 11) 
#define MLX3_MPT_FLAG_REMOTE_READ  (1 << 12) 
#define MLX3_MPT_FLAG_REMOTE_WRITE (1 << 13) 
#define MLX3_MPT_FLAG_RAE                   (1 << 28)
#define MLX3_DISABLE_VA            (1 << 9) 

static int
query_d_mpt (struct mlx3_ib * mlx)
{
    struct mlx3_mpt_entry * mpt_entry = NULL;
    mlx3_cmd_box_t *mailbox = NULL;
    int err;
    mailbox = create_cmd_mailbox(mlx);

    if (!mailbox) {
        ERROR("Could not create mailbox\n");
    }

    err = mlx3_mailbox_cmd(mlx, 
                           0, 
                           mailbox->buf, 
                           key_to_hw_index(mlx->mr_table.dmpt->key),
                           0, 
                           CMD_QUERY_MPT,
                           CMD_TIME_CLASS_A
                           );
    if (err)
        goto free_mailbox;

    mpt_entry = (struct mlx3_mpt_entry *)mailbox->buf;

    DEBUG("Query MPT LKEY %d Mkey %d Qpn %d \n", bswap32(key_to_hw_index(mpt_entry->lkey)), bswap32((key_to_hw_index(mpt_entry->key))), (bswap32(mpt_entry->qpn) >>8));

    return 0;

free_mailbox:
    destroy_cmd_mailbox(mailbox);
    return -1;
}


static int 
create_d_mpt_entry (struct mlx3_ib * mlx, 
                    struct mlx3_d_mpt * dmpt, 
                    struct mlx3_mpt_entry * mpt_entry, 
                    int size, 
                    void * memory_buffer)
{
    uint64_t mtt_addr;
 
//    mpt_entry->flags = bswap32( MLX3_MPT_FLAG_REGION | MLX3_MPT_FLAG_LOCAL_READ | MLX3_MPT_FLAG_LOCAL_WRITE | MLX3_MPT_FLAG_REMOTE_READ | MLX3_MPT_FLAG_REMOTE_WRITE);
    mpt_entry->flags    &= ~(bswap32(MLX3_MPT_FLAG_FREE | MLX3_MPT_FLAG_SW_OWNS));
    dmpt->key            = mlx3_alloc_dmpt(mlx);
    mpt_entry->key       = bswap32(key_to_hw_index(dmpt->key));
    mpt_entry->flags_pd  = bswap32(MLX3_MPT_FLAG_RAE);
    dmpt->buffer         = memory_buffer;
    mpt_entry->qpn       = bswap32((64 << 8) | (1 << 7));
    mtt_addr             = mlx3_alloc_mtt(mlx, size / PAGE_SIZE_4KB);

    if (mlx3_WRITE_MTT(mlx, mtt_addr, memory_buffer, size / PAGE_SIZE_4KB)) {
        ERROR("Write MTT failed\n");
        return -1;
    }

    dmpt->mtt    = mtt_addr;
    dmpt->size   = size;
    dmpt->nentry = size / PAGE_SIZE_4KB;

    if (!dmpt->nentry)
        dmpt->nentry = 1;

    mpt_entry->start       = bswap64((uint64_t) memory_buffer);
    mpt_entry->length      = bswap64(dmpt->size);
    mpt_entry->entity_size = bswap32(ilog2(size));                //map page
    mpt_entry->mtt_addr    = bswap64((uint64_t)dmpt->mtt);
    mpt_entry->mtt_sz      = bswap32(dmpt->nentry);

    mpt_entry->flags    =  bswap32(MLX3_MPT_FLAG_MIO | 
                                   MLX3_MPT_FLAG_REGION | 
                                   MLX3_MPT_FLAG_LOCAL_READ | 
                                   MLX3_MPT_FLAG_LOCAL_WRITE |  
                                   MLX3_MPT_FLAG_REMOTE_READ | 
                                   MLX3_MPT_FLAG_REMOTE_WRITE);

    DEBUG("Memory Key %x\n", dmpt->key); 

    return 0;
}


/**
 *This method 
 *1] Allocates  buffer
 *2] Configures MTT Entries
 *3] Creates MPT Region for the MTT Entries
 **/
static int
create_memory_region (struct mlx3_ib * mlx, int size, void * memory_buffer) 
{
    mlx3_cmd_box_t * mailbox          = NULL;
    struct mlx3_mpt_entry * mpt_entry = NULL;
    struct mlx3_d_mpt * dmpt          = NULL;

    if (!mlx->mr_table.dmpt) {

        mlx->mr_table.dmpt = malloc(sizeof(struct mlx3_d_mpt));

        if (!mlx->mr_table.dmpt) {
            ERROR("Could not allocate d_mpt struct\n");
            return -1;
        }
    }
    
    dmpt = mlx->mr_table.dmpt; 
    
    //TODO: Check if sufficient icm space available for allcocating dmpt entry
    mailbox = create_cmd_mailbox(mlx);
    if (!mailbox) {
        ERROR("Could not create mailbox \n");
        goto err_table;
    }
   
    if (create_d_mpt_entry(mlx, dmpt, (struct mlx3_mpt_entry *)mailbox->buf, size, memory_buffer)) {
        ERROR("DMPT Entry could not be created \n");
        goto err_cmd;
    }

    if (mlx3_SW2HW_MPT(mlx, mailbox->buf, key_to_hw_index(dmpt->key))) {
        ERROR("SW2HW_MPT failed\n");
        goto err_cmd;
    }

    DEBUG("Memory Region of size %d  with mem key %d created sucesfully\n", dmpt->size, dmpt->key);

    destroy_cmd_mailbox(mailbox);

    return 0;

err_cmd:
    destroy_cmd_mailbox(mailbox);

err_table:
    //TODO:Free ICM space
    return -1;

}


static inline void 
barrier (void)
{
    asm volatile("" : : : "memory");
}


static int 
mlx3_alloc_data (struct mlx3_ib * mlx,
                 struct mlx3_rx_ring * ring,
                 struct mlx3_rx_desc * rxd)
{
#define FRAG_SIZE PAGE_SIZE_4KB 
    void * frag = malloc(FRAG_SIZE);
    if (!frag) {
        ERROR("Could not allocate fragment buffer\n");
        return -1;
    }
    memset(frag, 0, FRAG_SIZE);
    rxd->data[0].lkey       = bswap32(mlx->caps->resd_lkey); 
    
    rxd->data[0].addr       = bswap64((uint64_t)frag);
    barrier();
    rxd->data[0].byte_count = bswap32(FRAG_SIZE);

    DEBUG("MR ADDR %x Byte Count %d LKey 0x%08x\n", (uint64_t)frag, bswap32(FRAG_SIZE), mlx->mr_table.dmpt->key);
    
    return 0;
}


static inline int 
mlx3_prepare_rx_desc (struct mlx3_ib * mlx, 
					  struct mlx3_rx_ring * ring, 
					  int idx)
{
    struct mlx3_rx_desc * rxd = ring->buf + (idx * ring->wqe_size);
    return mlx3_alloc_data(mlx, ring, rxd);
}


static void 
mlx3_refill_rx_buffers (struct mlx3_ib * mlx, struct mlx3_rx_ring * ring)
{
    mlx3_prepare_rx_desc(mlx, ring, (ring->prod & 0xffff));

    ring->prod++;

    mlx3_update_rx_prod_db(ring);
}


static inline int
mlx3_get_cqn_offset (struct mlx3_ib * mlx, int cqn)
{
    return cqn -(1 << mlx->caps->log2_rsvd_cqs);
}


static inline int
mlx3_get_qpn_offset (struct mlx3_ib * mlx, int qpn) {
    
    return qpn - (1 << mlx->caps->log2_rsvd_qps); 
}

volatile uint64_t send_start = 0;
static void 
mlx3_cq_completion (struct mlx3_ib* mlx, uint32_t cqn)
{
    
    volatile uint64_t end = 0;
    end = rdtsc();
    int cqn_offset = mlx3_get_cqn_offset(mlx, cqn);
    struct mlx3_cq * cq = mlx->cqs[cqn_offset];
    struct mlx3_cqe * cqe = NULL;
    uint8_t opcode = -1;
    int qpn = -1;
    int qpn_offset = 0;
    int byte_count = -1;
    uint16_t dlid;
    if (cq->cqn != cqn) {
        ERROR("Completion event for bogus CQ %d CQN %d CQ Enteries %d  CQ Offset %d\n", cqn, cq->cqn, cq->nentry, cqn_offset);
        return;
    }

    cqe = next_cqe_sw(cq);

    while (cqe) {

        if (!cqe) {
            DEBUG("No valid CQEs present\n");
            return;
        }

        if ((cqe->owner_sr_opcode & 0x40)) { 
            end = rdtsc();
            DEBUG("Time For Send Completion %d Send Start %d Send End %d\n", (end - send_start), send_start, end); 
        }
        opcode     = cqe->owner_sr_opcode & 0x1f;
        qpn        = (bswap32(cqe->vlan_my_qpn) & 0xffffff);
        qpn_offset = mlx3_get_qpn_offset(mlx, qpn);
        byte_count = bswap32(cqe->byte_cnt);
        dlid       = bswap16(cqe->rlid);
        
        DEBUG("Completion Event Received:\n");
        DEBUG("  on CQN # %d\n", cqn); 
        DEBUG("  From QPN #%d\n", qpn);
        DEBUG("  SLID: 0x%04x\n", dlid);
        DEBUG("  Service Level: 0x%01x\n", bswap16(cqe->sl_vid) >> 12);
        DEBUG("  Byte Count: %d\n", byte_count);
        DEBUG("  Type:  %s       \n", (cqe->owner_sr_opcode & 0x40)? "Send Completion":"Receive Completion");
        DEBUG("  Opcode Type: %s\n", print_opcode(opcode, (cqe->owner_sr_opcode & 0x40)));
        DEBUG("  Opcode: 0x%02x\n", opcode);
        DEBUG("  Origin WQE Counter: 0x%04x\n", bswap16(cqe->wqe_index));

        if ((cqe->owner_sr_opcode & 0x1f) == 0x1e) {
            uint8_t synd = bswap16(cqe->checksum) & 0xff;
            uint8_t vsynd = bswap16(cqe->checksum) >> 8;
            DEBUG("  [COMPLETION ERROR! : 0x%02x (%s)]\n", synd, synd < 0x22 ? cqe_err_syndromes[synd] : "Unknown error");
            DEBUG("  [Vendor error syndrome : 0x%02x]\n", vsynd);
        }
    
        if (!(cqe->owner_sr_opcode & 0x40)) {
            mlx3_ib_query_qp(mlx, mlx->qps[qpn_offset]);
            dump_qp(mlx, mlx->qps[qpn_offset]);
        }
        
        ++cq->cons_index;
        cq_set_ci(cq);

        // Make sure hardware sees the consumer index value
        if (!(cqe->owner_sr_opcode & 0x40)) {
             mlx3_refill_rx_buffers(mlx, mlx->qps[qpn_offset]->rx);
        }
        cqe = next_cqe_sw(cq);
    } 
    ++cq->arm_sn;
    mlx3_cq_arm(cq, MLX3_CQ_DB_REQ_NOT, (uint32_t*)cq->uar, cq->cons_index);
}


volatile int link = 0;


static int 
eq_irq_handler (excp_entry_t * et, excp_vec_t ev, void * state)
{
	struct mlx3_ib * mlx = (struct mlx3_ib*)state;
	struct mlx3_eq * eq = mlx->eqs[0];
	struct mlx3_eqe * eqe = NULL;
	int eqe_factor = 0; //If 32 set to 0
	int set_ci = 0;
	int port;
	int cqn;
    int qpn;

	writel(eq->clr_mask, eq->clr_int);
    
    while ((eqe = next_eqe_sw(eq, eqe_factor, EQE_SIZE))) {
        switch (eqe->type)
        {
            case MLX3_EVENT_TYPE_COMP:
                cqn = bswap32(eqe->event.comp.cqn) & 0xffffff;
                mlx3_cq_completion(mlx, cqn);               
                break;

            case MLX3_EVENT_TYPE_CMD:
                mlx3_cmd_event(mlx, bswap16(eqe->event.cmd.token), eqe->event.cmd.status, bswap64(eqe->event.cmd.out_param));
                break;

            case MLX3_EVENT_TYPE_PORT_MNG_CHG_EVENT:
                DEBUG("MNG Port Change Event (subtype=0x%x)\n", eqe->subtype);
                DEBUG("Port %d SLID %d Lid %d \n",eqe->event.port_mgmt_change.port, bswap16(eqe->event.port_mgmt_change.params.port_info.mstr_sm_lid), bswap16(eqe->event.port_mgmt_change.params.port_info.port_lid));
                break;

            case MLX3_EVENT_TYPE_PORT_CHANGE: 
                port = bswap32(eqe->event.port_change.port) >> 28;
                DEBUG("Port Change Event - subtype [%s] Port: %d\n", (eqe->subtype == 0x4) ? "Active" : "Port Unactive Port Link Down", port);
                link  = 1; 
                break;
            case MLX3_EVENT_TYPE_COMM_EST: 
                qpn = bswap32(eqe->event.qp.qpn); 
                DEBUG("QP %d \n", qpn);
                mlx3_ib_query_qp(mlx, mlx->qps[mlx3_get_qpn_offset(mlx, qpn)]);
                break;

            default:
                DEBUG("Unknown event type 0x%02x EQ CONS_INDEX %d Addr %p Sub Type 0x%02x \n", eqe->type, eq->cons_index, (void*)eqe, eqe->subtype);
                break;

        };

        ++set_ci;
        ++eq->cons_index;

        if(set_ci >=  MLX3_NUM_SPARE_EQE) {
            eq_set_ci(eq, 0);
            set_ci = 0;
        }
    }   

    eq_set_ci(eq, 1);

    IRQ_HANDLER_END();

    return 0;	
}


static int 
mlx3_MAP_EQ (struct mlx3_ib * mlx, 
             uint64_t event_mask, int unmap,
             int eq_num)
{
	int err;

	err= mlx3_mailbox_cmd(
			mlx, 
			event_mask,  // inparam
			NA,          //outparam      
			(unmap << 31) | eq_num, // inmod   
			NA, 			//opmod                  
			CMD_MAP_EQ,		//op
			CMD_TIME_CLASS_A	//time	
			);
	if(err)
	{

		DEBUG("\nCannot Map EQ\n");
		return -1;
	}


	return 0;
}

static int
mlx3_NOP (struct mlx3_ib * mlx)
{
	int err;
	mlx3_cmd_box_t * cmd = create_cmd_mailbox(mlx);

	if (!cmd) {
		ERROR("Could not create cmd mailbox\n");
		return -1;
	}

	err = mlx3_mailbox_cmd_event(mlx,
			NA,
			NA,
			0x1f,
			NA,
			CMD_NOP,
			CMD_TIME_CLASS_A);

	if(err) {
		DEBUG("\n Error in NOP\n");
		goto out_err;
	}

	destroy_cmd_mailbox(cmd);
	return 0;

out_err:
	destroy_cmd_mailbox(cmd);
	return -1;

}

/* A test that verifies that we can accept interrupts
 * on the given irq vector of the tested port.
 * Interrupts are checked using the NOP command.
 */
static int 
mlx3_test_interrupt (struct mlx3_ib * mlx) 
{

	DEBUG("Testing interrupts with NOP command...\n");
	int err;               

	err = mlx3_NOP(mlx);

	return 0;
}


static int 
mlx3_SW2HW_EQ (struct mlx3_ib * mlx,
               uint32_t * eq_context,
               int eqn)
{
	int err = 0;

	mlx3_cmd_box_t * cmd = NULL; 

	cmd = create_cmd_mailbox(mlx);

	if (!cmd) {
		ERROR("Could not create cmd mailbox\n");
		return -1;
	}

	memcpy((void*)cmd->buf,(void*)eq_context, sizeof(struct mlx3_eq_context));

	err = mlx3_mailbox_cmd(
			mlx,
			cmd->buf,               // inparam
			NA,                     // outparam      
			eqn,                    // inmod   
			NA,                     // opmod                  
			CMD_SW2HW_EQ,           // op
			CMD_TIME_CLASS_A        // time  
			);

	if (err) {
		DEBUG("SW2HW EQ command failed\n");
		goto out_err;
	}

	destroy_cmd_mailbox(cmd);

	return 0;

out_err:
	destroy_cmd_mailbox(cmd);
	return -1;
}

#define MLX3_EQ_STATE_ARMED        (9 <<  8)
#define MLX3_EQ_STATUS_OK          (0 << 28)
#define MLX3_EQ_EC		           (1 << 18) 
#define __force         __attribute__((force))

static inline uint32_t 
swab32 (uint32_t x)
{
    return  ((x & (uint32_t)0x000000ffUL) << 24) |
        ((x & (uint32_t)0x0000ff00UL) <<  8) |
        ((x & (uint32_t)0x00ff0000UL) >>  8) |
        ((x & (uint32_t)0xff000000UL) >> 24);
}


static struct mlx3_eq *
create_eq (struct mlx3_ib * mlx)
{
    struct mlx3_eq_context * ctx        = NULL;
    struct mlx3_eq * eq                 = NULL;

    int err, npages = 0;
    int eqe_padding = 32;
    int cnt         = 0;

    __mzero_checked(eq, sizeof(struct mlx3_eq), "Could not allocate EQ\n", return NULL);

    eq->eqn        = mlx3_alloc_eqn(mlx);
    eq->dev        = mlx->dev;
    eq->nentry     = MLX3_NUM_ASYNC_EQE + MLX3_NUM_SPARE_EQE;
    eq->cons_index = 0;

    npages = PAGE_ALIGN(eq->nentry * mlx->caps->eqe_size) / PAGE_SIZE_4KB;

    eq->size = npages * PAGE_SIZE_4KB;
    // NOTE: condition not needed if 128  EQE enteries
    if (npages == 0) {
        npages = 1;
    }

    __mzero_checked(eq->buffer, (npages * PAGE_SIZE_4KB) + eqe_padding, 
            "Could not allocate EQ buffer\n", goto out_err0);

    // Align address to 32 byte Might waste some buffer space 
    while (((uint64_t)eq->buffer & (31)) && (cnt < eqe_padding)) {
        DEBUG("Alignment Count %d Buffer %p \n", cnt, eq->buffer);
        eq->buffer++; 
        cnt++;
    }

    // NOTE: this assumes we only have *one* EQ which is not a reserved eq! each uar has 4 eq doorbells if uar is reserved even eq cannot be used
    eq->uar_map = mlx->uar_start  + ((eq->eqn / 4) << PAGE_SHIFT_4KB);

    eq->doorbell = (uint32_t *)(eq->uar_map + (0x800 + 8 * (eq->eqn % 4)));
    eq->mtt      = (void*)mlx3_alloc_mtt(mlx, npages);

    // TODO: Should use MSI-X instead of legacy INTs
    eq->intr_vec = IRQ_NUMBER;
    eq->pci_intr = mlx->dev->cfg.dev_cfg.intr_pin;

    if (mlx3_WRITE_MTT(mlx, (uint64_t)eq->mtt, eq->buffer, npages)) {
        ERROR("Could not write MTT\n");
        goto out_err1;
    }

    __mzero_checked(ctx, sizeof(struct mlx3_eq_context), "Could not allocate EQ context entry\n", goto out_err1);

    ctx->flags       = bswap32(MLX3_EQ_STATUS_OK | MLX3_EQ_STATE_ARMED);
    ctx->log_eq_size = ilog2(eq->nentry);

    ctx->log_page_size   = PAGE_SHIFT_4KB - MLX3_ICM_PAGE_SHIFT;
    ctx->mtt_base_addr_h = ((uint64_t)eq->mtt) >> 32;
    ctx->mtt_base_addr_l = bswap32(((uint64_t)eq->mtt) & 0xffffffffu);

    err = mlx3_SW2HW_EQ(mlx, (uint32_t*)ctx, eq->eqn);

    free(ctx);

    if (err) {
        ERROR("SW2HWEQ failed\n");
        goto out_err1;
    }

    return eq;

out_err1:
    free(eq->buffer);
out_err0:
    free(eq);
    return NULL;
}


static void
destroy_eq (struct mlx3_eq * eq)
{
    // TODO BRIAN: should undo any card state (e.g. MTT entries) tied to this EQ
    free(eq->buffer);
    free(eq);
}


static int 
mlx3_CONF_SPECIAL_QP (struct mlx3_ib * mlx, uint32_t base_qpn, int qp_type)
{
	mlx3_cmd_box_t * cmd = create_cmd_mailbox(mlx);
    
    uint32_t inmod = base_qpn | (qp_type << 24);

    DEBUG("In Mod %d\n", inmod);

    if (!cmd) {
        ERROR("Could not create cmd mailbox in %s\n", __func__);
        return -1;
    }

    int err = 0;

    err = mlx3_mailbox_cmd (mlx,
                            0,
                            0,
                            inmod,
                            0,
                            CMD_CONF_SPECIAL_QP,
                            CMD_TIME_CLASS_A);

    if (err) {
        ERROR("Could not configure special QP\n");
        goto out_err;
    }

    destroy_cmd_mailbox(cmd);

    return 0;

out_err:
    destroy_cmd_mailbox(cmd);
    return -1;
}


// TODO: these need to go in header
typedef enum {
    PKT_RAW = 0,
    PKT_UD  = 1,
    PKT_UC  = 2,
    PKT_RD  = 3,
    PKT_RC  = 4,
    PKT_INVALID,
} pkt_type_t;


typedef enum {
    BTH_OP_UC_SEND_ONLY  = 0x24,
    BTH_OP_UD_SEND_ONLY  = 0x64,
    BTH_OP_UC_RDMA_WRITE = 0x2a,
} bth_op_t;


static inline int 
build_bth (struct ib_bth * bth, bth_op_t opcode, uint8_t dst_qpn)
{
    memset(bth, 0, BTH_LEN);

    bth->opcode        = opcode;
    bth->rsvd_dqpn     = bswap32(dst_qpn);
    bth->pkey          = bswap16(65535);
    bth->se_m_pad_tver = (1 << 7);

    return 0;
}


static inline int 
build_deth (struct ib_deth * deth, uint8_t src_qpn, uint32_t qkey)
{
    memset(deth, 0, DETH_LEN);

    deth->qkey      = bswap32(qkey); 
    deth->rsvd_sqpn = bswap32(src_qpn);

    return 0;
}


static inline int 
build_reth (struct ib_reth * reth, uint64_t va, uint32_t rkey, uint32_t dma_len)
{
    memset(reth, 0, RETH_LEN);

    reth->va      = bswap64(va);
    reth->rkey    = bswap32(rkey);
    reth->dma_len = bswap32(dma_len); 

    return 0;
}    


static inline int 
build_lrh (struct ib_lrh * lrh, int pkt_size, int lnh, int src_lid, int dest_lid)
{
    memset(lrh, 0, LRH_LEN);

    lrh->vl_lver         = 0;
    lrh->sl_rsvd_lnh     = lnh;
    lrh->destination_lid = bswap16(dest_lid);
    lrh->rsvd_packet_len = bswap16(pkt_size);
    lrh->source_lid      = bswap16(src_lid);

    return 0;
}


static int
build_ib_header (struct ib_headers * header, 
                 int pkt_size, 
                 pkt_type_t pkt_type, 
                 int dst_qpn, 
                 int src_qpn, 
                 int qkey, 
                 uint64_t va, 
                 int rkey, 
                 int dma_len,
                 bth_op_t bth_opcode,
                 int src_lid,
                 int dest_lid)
{
    struct ib_bth  * bth  = NULL;
    struct ib_deth * deth = NULL;
    struct ib_reth * reth = NULL;
    struct ib_lrh * lrh   = NULL;
    int lnh               = 0;

    lrh = malloc(sizeof(struct ib_lrh));
    if (!lrh) {
        ERROR("Could not allocate LRH header\n");
        return -1;
    }

    header->lrh = lrh;

    // TODO: Brian needs to fix this 
    switch (pkt_type) {

        case PKT_RAW:
            return build_lrh(header->lrh, pkt_size, lnh, src_lid, dest_lid);
        case PKT_UD:
            DEBUG("UD Header Construction\n");
            lnh = 2;
            build_lrh(header->lrh, pkt_size, lnh, src_lid, dest_lid);
            bth = malloc(sizeof(struct ib_bth));
            if (!bth) {
                ERROR("Could not allocate BTH for UD packet\n");
                goto out_err;
            }
            header->bth = bth;
            build_bth(header->bth, BTH_OP_UD_SEND_ONLY, dst_qpn);
            deth = malloc(sizeof(struct ib_deth));
            if (!deth) {
                ERROR("Could not allocate DETH header for UD packet\n");
                goto out_err;
            }
            header->deth = deth;
            return build_deth(header->deth, src_qpn, qkey);
        case PKT_UC:
            DEBUG("UC Header Construction\n");
            lnh = 2;
            build_lrh(header->lrh, pkt_size, lnh, src_lid, dest_lid);
            bth = malloc(sizeof(struct ib_bth));
            if (!bth) {
                ERROR("Could not allocate BTH header for UC RDMA packet\n");
                goto out_err;
            }
            header->bth = bth;
            switch (bth_opcode) {
                case BTH_OP_UC_RDMA_WRITE:
                    build_bth(header->bth, BTH_OP_UC_RDMA_WRITE, dst_qpn);
                    reth = malloc(sizeof(struct ib_reth));
                    if (!reth) {
                        ERROR("Could not allocate RETH header for UC RDMA packet\n");
                        goto out_err;
                    }
                    header->reth = reth;
                    return build_reth(header->reth, va, rkey, dma_len);
                case BTH_OP_UC_SEND_ONLY:
                    return build_bth(header->bth, BTH_OP_UC_SEND_ONLY, dst_qpn);

                default:
                    ERROR("Invalid BTH Opcode\n");
                    goto out_err;
            }
            break; 

        default:
            ERROR("Invalid packet type (%d)\n", pkt_type);
            goto out_err;
    }

    return 0;

out_err:
    free(lrh);

    return -1;
}


static int
build_ib_packet (struct ib_packet * packet, 
                 pkt_type_t pkt_type, 
                 int dst_qpn, 
                 int src_qpn, 
                 uint32_t qkey, 
                 int pkt_size, 
                 uint64_t va, 
                 uint32_t rkey, 
                 uint32_t dma_len,
                 bth_op_t bth_opcode,
                 int src_lid,
                 int dest_lid)
{
    int err;
    int pkt_len; 
    int payload;    
    struct ib_headers * header = packet->header;

    pkt_len = (pkt_size)/ 4; 

    DEBUG("IB packet length: %d\n", pkt_len);  

    if (!header) {
        header = malloc(sizeof(struct ib_headers));
        if (!header) {
            ERROR("Could not allocate IB header\n");
            return -1;
        }
    }  
    
    packet->header = header;

    if (build_ib_header(packet->header, pkt_len, pkt_type, dst_qpn, src_qpn, qkey, va, rkey, dma_len, bth_opcode, src_lid, dest_lid) != 0) {
        ERROR("Could not build IB header\n");
        goto out_err;
    }

    payload = pkt_size;

    __mzero_checked(packet->data, payload, "Could not allocate payload\n", goto out_err);

    memset(packet->data, 0x65, payload);

    return 0;

out_err:
    free(header);
    return -1;
}


static int
build_mlx_ud_wqe (struct mlx3_ib * mlx, 
                  struct mlx3_tx_ud_desc * tx_desc, 
                  int port, 
                  void * pkt, 
                  int dst_qpn, 
                  int qkey, 
                  int pkt_size, 
                  int slid, 
                  int dlid)
{
    /*Addres Vector Layout in conjunction with UD*/ 
    struct mlx3_av *av = (struct mlx3_av *)tx_desc->ud_ctrl.av;
    av->port_pd = bswap32(port << 24);
    av->dlid    = bswap16(dlid);
    av->g_slid  = slid & 0x7f;
    /* Ctrl segment */
    tx_desc->ctrl.vlan_cv_f_ds  = bswap32(sizeof(struct  mlx3_tx_ud_desc) / 16) ; // size in 16 bytes
    tx_desc->ctrl.flags         = bswap32((0x3 << 2)); // generate a CQE on a good send completion
    tx_desc->ud_ctrl.dst_qpn       = bswap32(dst_qpn);
    tx_desc->ud_ctrl.qkey       = bswap32(qkey); 
    /* UD Segment */
    tx_desc->data.lkey          = bswap32(mlx->caps->resd_lkey);
//    tx_desc->data.lkey = bswap32(mlx->mr_table.dmpt->key);
    tx_desc->data.addr          = bswap64((uint64_t)pkt);
    barrier();
    tx_desc->data.byte_count    = bswap32(pkt_size);
    return 0;
}


static int
build_mlx_rdma_wqe (struct mlx3_ib * mlx, struct mlx3_rdma_desc * tx_desc, void* pkt, int pkt_size, uint64_t va, uint32_t rkey)
{
    /* Ctrl segment */
    tx_desc->ctrl.vlan_cv_f_ds  = bswap32(sizeof(struct  mlx3_rdma_desc) / 16) ; // size in 16 bytes
    tx_desc->ctrl.flags         = bswap32((0x3 << 2)); // generate a CQE on a good send completion
    /* Raddt Segemnt */
    tx_desc->raddr.key          = bswap32(rkey);
    tx_desc->raddr.va           = bswap64(va); 
    /* RDMA Segment */
//    tx_desc->data.lkey          = bswap32(mlx->mr_table.dmpt->key);

    tx_desc->data.lkey          = bswap32(mlx->caps->resd_lkey);
    tx_desc->data.addr          = bswap64((uint64_t)pkt);
    barrier();
    tx_desc->data.byte_count    = bswap32(pkt_size);
    return 0;
}


static int
build_mlx_uc_wqe (struct mlx3_ib * mlx, struct mlx3_uc_desc * tx_desc, void* pkt, int pkt_size)
{
    /* Ctrl segment */
    tx_desc->ctrl.vlan_cv_f_ds  = bswap32(sizeof(struct  mlx3_uc_desc) / 16) ; // size in 16 bytes
    tx_desc->ctrl.flags         = bswap32((0x3 << 2)); // generate a CQE on a good send completion
//    tx_desc->data.lkey          = bswap32(mlx->mr_table.dmpt->key);
    tx_desc->data.lkey          = bswap32(mlx->caps->resd_lkey);
    tx_desc->data.addr          = bswap64((uint64_t)pkt);
    barrier();
    tx_desc->data.byte_count    = bswap32(pkt_size);
    return 0;
}


static int
build_mlx_ud_nops_wqe (struct mlx3_ib * mlx, struct mlx3_tx_ud_desc * tx_desc, int port)
{
    /* Ctrl segment */
    tx_desc->ctrl.flags |= bswap32(0x3 << 2); // generate a CQE on a good send completion
    tx_desc->ctrl.vlan_cv_f_ds = sizeof(*tx_desc) >> 4; // size in 16-byte chunks

    struct mlx3_av *av = (struct mlx3_av *)tx_desc->ud_ctrl.av;
    memset(&tx_desc->ud_ctrl, 0, sizeof (tx_desc->ud_ctrl));
    av->port_pd = bswap32(port << 24);

    DEBUG("DS SIZE %d\n", sizeof(*tx_desc) >> 4);
    return 0;
}


static int
build_mlx_raw_wqe (struct mlx3_ib * mlx, struct mlx3_tx_raw * rx_desc, void * pkt)
{
    /* Ctrl segment */
    rx_desc->ctrl.flags |= bswap32(7 << 12); // 40 Gbps trans rate when scheduling packet
    rx_desc->ctrl.flags |= bswap32(0x3 << 2); // generate a CQE on a good send completion
    rx_desc->ctrl.size = sizeof(*rx_desc) >> 4; // size in 16-byte chunks
    DEBUG("DS SIZE %d\n", sizeof(*rx_desc) >> 4);
    rx_desc->ctrl.rlid = 0xffff;
    rx_desc->data.lkey       = bswap32(mlx->caps->resd_lkey);
    rx_desc->data.addr       = bswap64((uint64_t)pkt);

    DEBUG("IB Pkt Address %p\n", pkt);

    rx_desc->data.byte_count =  bswap32(PAGE_SIZE_4KB);
    return 0;
}


static int
build_mlx_raw_swqe (struct mlx3_ib * mlx, 
                    struct mlx3_tx_raw * tx_desc, 
                    void * pkt, 
                    uint16_t dest_rlid,
                    int pkt_size)
{
    /* Ctrl segment */
    //tx_desc->ctrl.flags |= bswap32(7 << 12); // 40 Gbps trans rate when scheduling packet
    tx_desc->ctrl.flags    = bswap32((0 << 4) | (0x3 << 2)); // no ICRC, it's a raw packet
    //tx_desc->ctrl.flags |= bswap32(1 << 17); // packet will not undergo SL2VL mapping
    //tx_desc->ctrl.flags |= bswap32(0 << 16); // SLID MSBits are taken from packet (??)
    //    tx_desc->ctrl.flags |= bswap32(0x3 << 2); // generate a CQE on a good send completion
    //    tx_desc->ctrl.flags |= bswap32(1 << 0);   //loopback
    tx_desc->ctrl.size = sizeof(*tx_desc) >> 4; // size in 16-byte chunks
    DEBUG("DS SIZE %d\n", sizeof(*tx_desc) >> 4);
    tx_desc->ctrl.rlid = bswap16(dest_rlid);
    DEBUG("Writing lkey: 0x%08x\n", mlx->caps->resd_lkey);
    tx_desc->data.lkey       = bswap32(mlx->caps->resd_lkey);
    //      tx_desc->data.lkey       = bswap32(key_to_hw_index(mlx->mr_table.dmpt->key));
    tx_desc->data.addr       = bswap64((uint64_t)pkt);
    DEBUG("IB Pkt Address %p\n", pkt);
    barrier();
    tx_desc->data.byte_count =  bswap32(pkt_size);
    return 0;
}


// See Mellanox PRM pp. 137
static int
mlx3_attach_sq (struct mlx3_ib * mlx, struct mlx3_qp * qp, int size, int stride, void * buf_start) 
{
	struct mlx3_tx_ring * swq = NULL;
    int i, j;

    // TODO: buffer size should really be set 
    // according to number of WQEBBs (provided in params),
    // (constrained by log_max_qp_sz in dev cap)

	swq = malloc(sizeof(struct mlx3_tx_ring));
    if (!swq) {
        ERROR("Could not allocate TX ring in %s\n", __func__);
        return -1;
    }
	memset(swq, 0, sizeof(struct mlx3_tx_ring));

	swq->prod       = 0;
	swq->cons       = 0xffffffff;
	swq->cqn        = qp->cq->cqn;
	swq->stride     = stride;
	swq->log_stride = ilog2(stride);
    swq->bb_size    = 16 * swq->stride; // See QPN Context Entry Format: PRM pp. 184
    swq->bb_count   = size / swq->bb_size;
    swq->buf_size   = size;
    swq->buf        = buf_start;
    swq->valid      = 0; // used for owner bit


    // All WQEBBs should initially be set such that every 64-byte block
    // within them is set to invalid
    for (i = 0; i < swq->buf_size / 64; i++) {
        uint32_t * eqe_hdr = (uint32_t*)(swq->buf + (i * 64));
        eqe_hdr[0] = bswap32(0x7fffffff | ((!swq->valid) << 31));	 
    }

    qp->tx = swq;

    return 0;
}


// RQs are comprised of WQEs, which in turn are comprised of 16-byte chunks
// Bit 31 must be clear, R WQEs cannot contain inline data
// RWQEs do not contain ctrl segments, only data segs with 
// scatter lists (mem pointers)
static int
mlx3_fill_rx_buffers (struct mlx3_ib * mlx, struct mlx3_rx_ring * rwq)
{
    int buf_ind;

    for (buf_ind = 0; buf_ind < rwq->size; buf_ind++) {
        if (mlx3_prepare_rx_desc(mlx, rwq, buf_ind) != 0) {
            ERROR("Could not prepare RX descriptor\n");
            return -1;
        }

        rwq->prod++;
    }

    return 0;
}


static inline void
mlx3_clear_rwqe (struct mlx3_rx_ring * ring, int idx)
{
	struct mlx3_rx_desc * rxd = (struct mlx3_rx_desc*)(ring->buf + ring->wqe_size * idx);
	int rxds_per_wqe = ring->wqe_size / 16;
	int i;

	for (i = 0; i < rxds_per_wqe; i++) {
        rxd->data[i].byte_count = 0;
        rxd->data[i].lkey       = bswap32(NULL_SCATTER_LKEY);
        rxd->data[i].addr       = 0;
	}
}


static inline void 
mlx3_init_rwq (struct mlx3_ib * mlx, struct mlx3_rx_ring * ring)
{
    int i;

    for (i = 0; i < ring->size; i++) {
		mlx3_clear_rwqe(ring, i);
    }
}

/* 
 * Note: assumes a SQ already exists in this QP
 */
static int
mlx3_attach_rq (struct mlx3_ib * mlx, struct mlx3_qp * qp, int size, int stride, void * buf_start, void* db_rec)
{
	struct mlx3_rx_ring * ring = NULL;
    int i;

	ring = malloc(sizeof(struct mlx3_rx_ring)); 
    if (!ring) {
        ERROR("Could not allocate receive ring in %s\n", __func__);
        return -1;
    }
	memset(ring, 0, sizeof(struct mlx3_rx_ring));

	// TODO: this needs to come from somewhere else (caller)
	//int stride = roundup_pow_of_two(sizeof(struct mlx3_rx_desc) + DS_SIZE);

	/* The size (stride) of a Receive WQE equals 16*(2log_rq_stride). The minimum
	 * size is 16 bytes and the value 7 is reserved. Reserved if the srq or rss bits
	 * are set or if the st field indicates an XRC.  
	 */

	ring->prod        = 0;
	ring->cons        = 0;
	ring->cqn         = qp->cq->cqn;
	ring->stride      = stride;
    ring->wqe_size    = 16   * stride;
	ring->size        = size / ring->wqe_size; // size in WQEs
	ring->log_stride  = ilog2(stride);
	ring->buf         = buf_start; //SQE =2048 RQE Remaining 2048
	ring->buf_size    = size; // same as actual_siz
    ring->db_rec      = qp->db_rec;
	memset(ring->buf, 0, size);
    
	mlx3_init_rwq(mlx, ring);

	mlx3_fill_rx_buffers(mlx, ring);
   
	qp->rx = ring;

    mlx3_update_rx_prod_db(ring); 
	return 0;
}

enum {
        MLX3_RAW_QP_MTU         = 5, // 4K MTU
        //MLX3_RAW_QP_MSGMAX      = 12,
        MLX3_RAW_QP_MSGMAX      = 30, // 2MB Max message size (hardware can break up into MTUs)
};

enum {
    /* params1 */
    MLX3_QP_BIT_SRE             = 1 << 15,
    MLX3_QP_BIT_SWE             = 1 << 14,
    MLX3_QP_BIT_SAE             = 1 << 13,
    /* params2 */
    MLX3_QP_BIT_RRE             = 1 << 15,
    MLX3_QP_BIT_RWE             = 1 << 14,
    MLX3_QP_BIT_RAE             = 1 << 13,
    MLX3_QP_BIT_FPP             = 1 <<  3,
    MLX3_QP_BIT_RIC             = 1 <<  4,
};


static int 
mlx3_fill_qp_ctx (struct mlx3_ib * mlx, struct mlx3_qp * qp)
{
    int port = 1;
    qp->ctx = malloc(sizeof(struct mlx3_qp_context));

	if (!qp->ctx) {
		ERROR("Could not allocate QP context\n");
        return -1;
	}
	memset(qp->ctx, 0, sizeof(struct mlx3_qp_context));	

	qp->ctx->flags      = bswap32(qp->tp_cx << 16 | (0x3 << 11) | 0x1); // Transport type and set 3 if Alternate path migration not supported
	// TODO: MTU should depend on transport type
	qp->ctx->mtu_msgmax = 0xb4;
    if (qp->rx != NULL) {
		qp->ctx->rq_size_stride = ilog2(qp->rx->size) << 3     | (qp->rx->log_stride) ;
    }
	
	if (qp->tx != NULL) {
		qp->ctx->sq_size_stride = ilog2(qp->tx->bb_count) << 3 | (qp->tx->log_stride) | (1 << 7);	    
    } 
    qp->ctx->params2        |= bswap32(MLX3_QP_BIT_FPP);    
    qp->ctx->cqn_send       = bswap32(qp->cq->cqn);
	qp->ctx->cqn_recv       = bswap32(qp->cq->cqn);

	//0 -127 EQ UAR Dorbells next followed index can be used  
	qp->ctx->usr_page        = bswap32(mlx3_to_hw_uar_index(mlx, qp->uar));
	qp->ctx->local_qpn       = bswap32(qp->qpn);
	qp->ctx->db_rec_addr     = bswap64(((uint64_t)qp->db_rec << 2));
    DEBUG("DB Rec %p  BSWAP 64 %p Db rec %p\n", qp->ctx->db_rec_addr, bswap64((uint64_t)qp->db_rec), qp->db_rec);
    qp->ctx->mtt_base_addr_h = (uint64_t)qp->buf->mtt >> 32;
    qp->ctx->mtt_base_addr_l = bswap32((uint64_t)qp->buf->mtt & 0xffffffff);
    qp->ctx->log_page_size   = PAGE_SHIFT_4KB - MLX3_ICM_PAGE_SHIFT;
    qp->ctx->pri_path.sched_queue        = 0x83 | ((port - 1) << 6);
    if (qp->is_sqp) {
        qp->ctx->qkey = bswap32(0x80010000);
    }

	return 0;
}

static int 
mlx3_qp_modify(struct mlx3_ib *mlx, 
               uint64_t mtt_addr,
               enum mlx3_qp_state cur_state, 
               enum mlx3_qp_state new_state,
               struct mlx3_qp_context *context,
               enum mlx3_qp_optpar optpar,
               int sqd_event, 
               int qpn)
{
    static const uint16_t op[MLX3_QP_NUM_STATE][MLX3_QP_NUM_STATE] = {
        [MLX3_QP_STATE_RST] = {
            [MLX3_QP_STATE_RST]     = MLX3_CMD_2RST_QP,
            [MLX3_QP_STATE_ERR]     = MLX3_CMD_2ERR_QP,
            [MLX3_QP_STATE_INIT]    = MLX3_CMD_RST2INIT_QP,
        },
        [MLX3_QP_STATE_INIT]  = {
            [MLX3_QP_STATE_RST]     = MLX3_CMD_2RST_QP,
            [MLX3_QP_STATE_ERR]     = MLX3_CMD_2ERR_QP,
            [MLX3_QP_STATE_INIT]    = MLX3_CMD_INIT2INIT_QP,
            [MLX3_QP_STATE_RTR]     = MLX3_CMD_INIT2RTR_QP,
        },
        [MLX3_QP_STATE_RTR]   = {
            [MLX3_QP_STATE_RST]     = MLX3_CMD_2RST_QP,
            [MLX3_QP_STATE_ERR]     = MLX3_CMD_2ERR_QP,
            [MLX3_QP_STATE_RTS]     = MLX3_CMD_RTR2RTS_QP,
        },
        [MLX3_QP_STATE_RTS]   = {
            [MLX3_QP_STATE_RST]     = MLX3_CMD_2RST_QP,
            [MLX3_QP_STATE_ERR]     = MLX3_CMD_2ERR_QP,
            [MLX3_QP_STATE_RTS]     = MLX3_CMD_RTS2RTS_QP,
            [MLX3_QP_STATE_SQD]     = MLX3_CMD_RTS2SQD_QP,
        },
        [MLX3_QP_STATE_SQD] = {
            [MLX3_QP_STATE_RST]     = MLX3_CMD_2RST_QP,
            [MLX3_QP_STATE_ERR]     = MLX3_CMD_2ERR_QP,
            [MLX3_QP_STATE_RTS]     = MLX3_CMD_SQD2RTS_QP,
            [MLX3_QP_STATE_SQD]     = MLX3_CMD_SQD2SQD_QP,
        },
        [MLX3_QP_STATE_SQER] = {
            [MLX3_QP_STATE_RST]     = MLX3_CMD_2RST_QP,
            [MLX3_QP_STATE_ERR]     = MLX3_CMD_2ERR_QP,
            [MLX3_QP_STATE_RTS]     = MLX3_CMD_SQERR2RTS_QP,
        },
        [MLX3_QP_STATE_ERR] = {
            [MLX3_QP_STATE_RST]     = MLX3_CMD_2RST_QP,
            [MLX3_QP_STATE_ERR]     = MLX3_CMD_2ERR_QP,
        }
    };

    mlx3_cmd_box_t* mailbox;

    int ret = 0;
    int real_qp0 = 0;
    int proxy_qp0 = 0;
    uint8_t port;

    if (cur_state >= MLX3_QP_NUM_STATE || new_state >= MLX3_QP_NUM_STATE ||
            !op[cur_state][new_state]) {
        ERROR("Cannot make state transition (%d) -> (%d)\n", cur_state, new_state);
        return -1;
    }

    if (op[cur_state][new_state] == MLX3_CMD_2RST_QP) {
        ret = mlx3_mailbox_cmd(mlx, 0, 0, qpn, 0, MLX3_CMD_2RST_QP, CMD_TIME_CLASS_A);
        DEBUG("MLX3_CMD_2RST_QP %d\n", ret); 
        return ret;
    }

    mailbox = create_cmd_mailbox(mlx);

    if (!mailbox) {
        ERROR("Could not create mailbox\n");
        return -1;
    }

    if (cur_state == MLX3_QP_STATE_RST && new_state == MLX3_QP_STATE_INIT) {
        context->mtt_base_addr_h = mtt_addr >> 32;
        context->mtt_base_addr_l = bswap32(mtt_addr & 0xffffffff);
        context->log_page_size   = PAGE_SHIFT_4KB - MLX3_ICM_PAGE_SHIFT;
    }

    *(uint32_t*) mailbox->buf = bswap32(optpar);
    memcpy((void*)mailbox->buf + 8, context, sizeof(*context));

    ((struct mlx3_qp_context *) (mailbox->buf + 8))->local_qpn = bswap32(qpn);

    ret = mlx3_mailbox_cmd(mlx, 
                           mailbox->buf,
                           0,
                           qpn | (!!sqd_event << 31),
                           0, //new_state == MLX3_QP_STATE_RST ? 2 : 0,
                           op[cur_state][new_state], 
                           CMD_TIME_CLASS_A);

    destroy_cmd_mailbox(mailbox);

    return ret;
}


static int 
mlx3_qp_modify_state (struct mlx3_ib * mlx, struct mlx3_qp * qp, int nstate)
{
    int err;
    int i;

     enum mlx3_qp_state states[] = {
        MLX3_QP_STATE_RST,
        MLX3_QP_STATE_INIT,
        MLX3_QP_STATE_RTR,
        MLX3_QP_STATE_RTS
    };


    for (i = 0; i < nstate; i++) {

        qp->ctx->flags &= bswap32(~(0xf << 28));
        qp->ctx->flags |= bswap32(states[i + 1] << 28);
        
    if (states[i + 1] != MLX3_QP_STATE_RTR) {
        qp->ctx->params2 &= ~bswap32(MLX3_QP_BIT_FPP);
    }
      err =  mlx3_qp_modify(mlx, (uint64_t)qp->buf->mtt, states[i], states[i + 1], qp->ctx, 0, 0, qp->qpn);

        if (err) {
            ERROR("Failed to bring QP to state: %d with error: %d\n",
                    states[i + 1], err);
            return err;
        }

        DEBUG("Successful State Transition of QP from state %d to state %d\n", states[i], states[i + 1]);

        qp->state = states[i + 1];
    }

    return 0;
}


static int
mlx3_qp_to_rtr (struct mlx3_ib * mlx, struct mlx3_qp * qp) 
{
   return mlx3_qp_modify_state(mlx, qp, MLX3_QP_STATE_RTR);
}


static int
mlx3_qp_to_rts (struct mlx3_ib * mlx, struct mlx3_qp * qp) 
{
    return mlx3_qp_modify_state(mlx, qp, MLX3_QP_STATE_RTS);
}


static int
mlx3_qp_to_rst(struct mlx3_ib * mlx, struct mlx3_qp * qp) {
    
    return mlx3_mailbox_cmd(mlx,
                            0,
                            0,
                            qp->qpn,
                            0,
                            MLX3_CMD_2RST_QP,
                            CMD_TIME_CLASS_A);

}


static int 
mlx3_SW2HW_CQ (struct mlx3_ib * mlx, mlx3_cmd_box_t * mbx, int cqn)
{

    DEBUG("Executing SW2HW_CQ\n");

    return mlx3_mailbox_cmd(mlx, 
                            mbx->buf, 
                            0,
                            cqn,
                            0,
                            CMD_SW2HW_CQ, 
                            CMD_TIME_CLASS_A);
}


static struct mlx3_cq *
mlx3_create_cq (struct mlx3_ib * mlx, 
                struct mlx3_eq * eq,
                int nent)
{
    mlx3_cmd_box_t  * mailbox           = NULL;
    struct mlx3_cq_context * ctx        = NULL;
    struct mlx3_cq * cq                 = NULL;
    void * buf                          = NULL;
    void * db_rec = NULL;
	uint64_t mtt_addr;
    int err;	
    int npages;

    __mzero_checked(cq, sizeof(struct mlx3_cq), "Could not create CQ\n", return NULL);

    cq->cqn     = mlx3_alloc_cqn(mlx);
    cq->uar_idx = mlx3_alloc_uar_scq_idx(mlx);
    cq->uar     = (void*)(mlx->uar_start + (cq->uar_idx << PAGE_SHIFT_4KB));
    cq->eq      = eq;
    cq->nentry  = nent;

    mailbox = create_cmd_mailbox(mlx);
    if (!mailbox) {
        ERROR("Could not create cmd mailbox in %s\n", __func__);
        goto out_err0;
    }

    ctx = (struct mlx3_cq_context *)mailbox->buf;
  //  ctx->flags          |= bswap32(1 << 18);
    ctx->logsize_usrpage = bswap32((ilog2(nent) << 24) | mlx3_to_hw_uar_index(mlx, cq->uar_idx));
    ctx->comp_eqn        = eq ? eq->eqn : 0;
    ctx->log_page_size   = PAGE_SHIFT_4KB - MLX3_ICM_PAGE_SHIFT;

    npages = ((CQE_SIZE * nent) / PAGE_SIZE_4KB);    

    cq->size = npages * PAGE_SIZE_4KB;

    mtt_addr = mlx3_alloc_mtt(mlx, npages);

    __mzero_checked(buf, CQE_SIZE * nent, "Could not allocate CQ buffer\n", goto out_err1);

    cq->buffer = buf;

    if (mlx3_WRITE_MTT(mlx, mtt_addr, buf, npages)) {
        ERROR("Could not write MTT in %s\n", __func__);
        goto out_err2;
    }

    ctx->mtt_base_addr_h = mtt_addr >> 32;
    ctx->mtt_base_addr_l = bswap32(mtt_addr & 0xffffffff);
	
    __mzero_checked(db_rec, CQ_DB_SIZE, "Could not allocate DB record\n", goto out_err2);

    cq->set_ci_db           = db_rec;
    cq->arm_db              = db_rec + 1;
    cq->arm_sn              = 1;
    cq->cons_index          = 0;

    ctx->db_rec_addr = bswap64((uint64_t)db_rec << 2);
 
    err = mlx3_SW2HW_CQ(mlx, mailbox, cq->cqn);

	if (err) {
		ERROR("SW2HW CQ Failed (%d)\n", err);
		goto out_err3;
	}
    
    destroy_cmd_mailbox(mailbox);

    mlx3_cq_arm(cq, MLX3_CQ_DB_REQ_NOT, (uint32_t*)cq->uar, cq->cons_index);

    DEBUG("CQN offset %d \n", mlx3_get_cqn_offset(mlx, cq->cqn));
    DEBUG("Created (and armed) new CQ:\n");
    DEBUG("  # %d\n", cq->cqn);
    DEBUG("  Arm SN: 0x%x\n", cq->arm_sn);
    DEBUG("  Arm Doorbell addr: %p\n", (void*)cq->arm_db);
    DEBUG("  UAR Index: %d\n", cq->uar_idx);
    DEBUG("  UAR Addr: %p\n", cq->uar);
    DEBUG("  Buf Addr: %p\n", cq->buffer);
    DEBUG("  Associated EQN: %d\n", cq->eq ? cq->eq->eqn : -1);

    return cq;

out_err3:
    free(db_rec);
out_err2:
    free(cq->buffer);
out_err1:
    destroy_cmd_mailbox(mailbox);
out_err0:
    free(cq);
    return NULL;
}


// TODO BRIAN: should make sure to undo any card state tied
// to this CQ (e.g. unarm it, untransfer ownership etc.)
static void
destroy_cq (struct mlx3_cq * cq)
{
    free(cq->buffer);
    free(cq);
}


static void 
default_ring (struct mlx3_qp * qp)
{

    barrier();
    writel(bswap32(qp->qpn << 8), (uint32_t*)(qp->mlx->uar_start + (qp->uar << PAGE_SHIFT_4KB) + MLX3_SEND_DOORBELL));
}


/*
 * Creates a new  QP structure. 
 * This includes allocating the area for the buffer
 * itself and allocating an MTT entry for the buffer. Does *not* allocate
 * a send queue or receivie queue for the WQ.
 *
 */
static struct mlx3_qp *
create_qp (struct mlx3_ib * mlx, struct mlx3_cq * cq, int size, int is_sqp, int transport_ctx)
{
    DEBUG("tRANSPORT INSIDE QP %d\n", transport_ctx);
    struct mlx3_qp * qp = NULL;
    void * db_rec = NULL;

    __mzero_checked(qp, sizeof(struct mlx3_qp), "Could not allocate work queue\n", return NULL);

    qp->mlx    = mlx;
    qp->uar    = mlx3_alloc_uar_scq_idx(mlx);
    qp->cq     = cq;
	qp->is_sqp = is_sqp;
    qp->qpn    = mlx3_alloc_qpn(mlx);
    qp->state  = MLX3_QP_STATE_RST; // we start out in reset state
    qp->tp_cx  = transport_ctx; 
    qp->ring_func = default_ring;

    __mzero_checked(qp->buf, sizeof(struct mlx3_wq_buf), "Could not allocate WQ buffer\n", goto out_err0);
    
    /* TODO: The buffer must be aligned on a WQEBB/Stride (the larger of the two); the
	 * offset of the WQE in the first page (page_offset) is specified while creating
	 * this control region in units of 64 bytes.
	 */

    __mzero_checked(qp->buf->data, size, "Could not allocate WQ buffer data area\n", goto out_err1);

    DEBUG("WQE Buffer %p\n", qp->buf->data);

    qp->buf->mtt = (uint64_t*)mlx3_alloc_mtt(mlx, size / PAGE_SIZE_4KB);

	if (mlx3_WRITE_MTT(mlx, (uint64_t)qp->buf->mtt, qp->buf->data, size / PAGE_SIZE_4KB)) {
		ERROR("Could not write MTT for QP\n");
		goto out_err2;
	}
	
	DEBUG("Using MTT offset 0x%lx for QP %d\n", (uint64_t)qp->buf->mtt, qp->qpn);

    __mzero_checked(qp->db_rec, QP_DB_SIZE, "Could not create WQ Doorbell\n", goto out_err2);

    DEBUG("DB REC %p\n", db_rec);

    return qp;

out_err2:
    free(qp->buf->data);
out_err1:
    free(qp->buf);
out_err0:
    free(qp);

    return NULL;
}


static void
destroy_qp (struct mlx3_qp * qp)
{
    DEBUG("Destroying QP %d\n", qp->qpn);
    free(qp->db_rec);
    free(qp->buf->data);
    free(qp->buf);
    free(qp);
}


/*
 * Creates a new QP, attaches SQs and RQs (if appropriate), and
 * transitions the queue to a ready state. 
 *
 * TODO: we shouldn't transition the queue here...
 */
static struct mlx3_qp *
mlx3_create_qp (struct mlx3_ib * mlx, 
                struct mlx3_cq * cq,
                struct mlx3_qp_parms * parms)
{
	struct mlx3_qp * qp = NULL;
    int sq_bb_sz, rq_wqe_sz;
	int err = 0;

    // TODO: "The driver must allocate enough memory to accomodate 
    // at least one WQE" ... we need to check that constraint here
    // on the parms passed in

    DEBUG("TP CTX %d\n", parms->tp);
    
    qp = create_qp(mlx, cq, parms->rq_size + parms->sq_size, parms->is_sqp, parms->tp);

    if (!qp) {
        ERROR("Could not create QP\n");
        return NULL;
    }

	if (!parms->sq_stride) {
		parms->sq_stride = SQ_STRIDE_DEFAULT;
	}

	if (!parms->rq_stride) {
		parms->rq_stride = RQ_STRIDE_DEFAULT;
	}

    sq_bb_sz  = 16 * parms->sq_stride;
    rq_wqe_sz = 16 * parms->rq_stride;

	// if RQ stride is bigger, RQ comes first in the WQ buf
    mlx3_attach_sq(mlx, qp, parms->sq_size, parms->sq_stride,
		(sq_bb_sz >= rq_wqe_sz) ? qp->buf->data : (qp->buf->data + parms->rq_size));

    if (parms->rq_size != 0) {
        mlx3_attach_rq(mlx, qp, parms->rq_size, parms->rq_stride,
		(sq_bb_sz >= rq_wqe_sz) ? (qp->buf->data + parms->sq_size) : qp->buf->data, qp->db_rec);
    }

	if (mlx3_fill_qp_ctx(mlx, qp) != 0) {
        ERROR("Could not fill QP context\n");
        goto out_err;
    }

    DEBUG("Created new QP:\n");
    DEBUG("  # %d\n", qp->qpn);
    DEBUG("  CQN: %d\n", qp->cq->cqn);
    DEBUG("  Type: %s\n", qp->is_sqp ? "Special QP" : "Regular QP");
    DEBUG("  UAR #: %d\n", qp->uar);
    DEBUG("  SQ Size: %d Bytes\n", qp->tx->buf_size);
    DEBUG("  SQ Stride: %d\n", qp->tx->stride);
    DEBUG("  SQ WQEBB size: %d Bytes\n", qp->tx->bb_size);
    DEBUG("  TX Descriptors per WQEBB: %d\n", qp->tx->bb_size / 64);
    DEBUG("  SQ Buffer Addr: %p\n", qp->tx->buf);
    DEBUG("  WQE Count: %d (max supported: %d)\n", qp->tx->bb_count, (1<<mlx->caps->log_max_qp_sz));
    DEBUG("  Qp DB_REC                      %p\n", qp->db_rec);

    if (qp->rx) {
        DEBUG("  RQ Size: %d Bytes\n", qp->rx->buf_size);
        DEBUG("  RQ Stride: %d\n", qp->rx->stride);
        DEBUG("  RQ WQE size: %d Bytes\n", qp->rx->wqe_size);
        DEBUG("  RX Descriptors per WQE: %d\n", qp->rx->wqe_size / 16);
        DEBUG("  RQ Buffer Addr: %p\n", qp->rx->buf);
        DEBUG("  WQE Count: %d (max supported: %d)\n", qp->rx->size, (1<<mlx->caps->log_max_qp_sz));
    }

	return qp;

out_err:
    destroy_qp(qp);
    return NULL;
}


static inline struct mlx3_qp * 
create_qp_with_parms (struct mlx3_ib * mlx, 
                      struct mlx3_cq * cq, 
                      transport_type_t tpt_ctx, 
                      int sq_size, 
                      int rq_size, 
                      int sqp) {

    struct mlx3_qp_parms parms = {
        .sq_size = sq_size,
        .rq_size = rq_size,
        .is_sqp  = sqp,
        .tp      = tpt_ctx,
    };

    return mlx3_create_qp(mlx, cq, &parms);
}

static int
mlx3_get_characteristics (void * mlx, struct nk_net_dev_characteristics *c)
{
	ERROR("%s NOT IMPLEMENTED\n", __func__);
	return -1;
}


static inline struct mlx3_qp * 
build_default_qp (struct mlx3_ib * mlx, struct ib_context * ctx) 
{
    return create_qp_with_parms(mlx, mlx->cqs[0], ctx->transport, ctx->sq_size, ctx->rq_size, 0);
}


static int
mlx3_build_transport_context (struct mlx3_qp * qp, 
                              user_trans_op_t opcode, 
                              int dst_qpn, 
                              int dlid, 
                              int slid, 
                              uint32_t qkey)
{
    qp->ctx->qkey               = bswap32(qkey);
    qp->ctx->remote_qpn         = bswap32(dst_qpn);
    qp->ctx->pri_path.grh_mylmc = (slid & 0x7f);
    qp->ctx->pri_path.rlid      = bswap16(dlid);

    switch (opcode) {

        case OP_RC_SEND:
        case OP_RAW_SEND:
        case OP_RAW_RECV:
        case OP_UC_SEND:
        case OP_RC_RDMA_WRITE:
        case OP_RC_RDMA_READ:
        case OP_UC_RDMA_WRITE:
            qp->ctx->rlkey_roce_mode = 1 << 4; // This qp can use reserved Lkey
            break;
        case OP_RC_RECV:
            qp->ctx->rlkey_roce_mode  = 1 << 4; // This qp can use reserved Lkey
            qp->ctx->params2         |= bswap32((1 << 15) | (1 << 14) | (1 << 13));  //Enable RDMA Write, Read and Atomics on RQ
            break;
        case OP_UC_RECV:
            qp->ctx->rlkey_roce_mode  = 1 << 4; // this qp can use reserved lkey
            qp->ctx->params2         |= bswap32((1 << 13) | (1 << 14) );  //enable rdma write, read and atomics on rq
            break;
        case OP_UC_SEND_IMM:
        case OP_UD_SEND_IMM:
        case OP_RC_SEND_IMM:
            break;
        case OP_UD_SEND:
        case OP_UD_RECV:
            DEBUG("UD Send/Recv dst_qpn %d Qkey %x\n", dst_qpn, qkey); 
            qp->ctx->pri_path.ackto  = 1 & 0x7;
            qp->ctx->rlkey_roce_mode = 1 << 4;
            break;
        default:
            ERROR("Invalid transport opcode (%d)\n", opcode);
            return -1;
    }

    return 0;
}


static struct mlx3_qp * 
get_or_create_qp (struct mlx3_ib * mlx, struct ib_context * ctx)
{
    struct mlx3_qp * qp = NULL;
    int src_qpn         = ctx->src_qpn;
    int i;

    // first few QPs are reserved
    if (src_qpn >= 0 && src_qpn < (1 << mlx->caps->log2_rsvd_qps)) {
        ERROR("Cannot use reserved QPN (%d)\n", src_qpn);
        return NULL;
    }

    // user doesn't care, give them any available QP
    if (src_qpn == QP_ANY) {

        for (i = 0; i < NUM_QPS; i++) {
            if (mlx->qps[i]) {
                return mlx->qps[i];
            }
    }
    }
    qp = mlx->qps[mlx3_get_qpn_offset(mlx, src_qpn)];

    // the QP doesn't exist yet, so we'll have to create it
    if (!qp) { 
        qp = build_default_qp(mlx, ctx);
        if (!qp) {
            ERROR("Could not build default QP\n");
            return NULL;
        }

        mlx->qps[mlx3_get_qpn_offset(mlx, src_qpn)] = qp;
        
        mlx3_build_transport_context(qp, ctx->user_op, ctx->dst_qpn, ctx->dlid, ctx->slid, ctx->qkey);

        if (mlx3_qp_to_rts(mlx, qp) != 0) {
            ERROR("Could not transition QP to send state\n");
            return NULL;
        }
        
    }

    return qp;
}


static void bf_copy(uint64_t *dst, uint64_t *src,
             unsigned int bytecnt)
{
    for( int i = 0; i < bytecnt; i++)
        dst[i] =  src[i];
}

static int
build_wqe (struct mlx3_ib * mlx, 
           struct mlx3_qp * qp, 
           int opcode, 
           int port, 
           void * pkt, 
           int pkt_size, 
           int dst_qpn, 
           uint32_t qkey, 
           int dlid, 
           int slid, 
           int va, 
           int rkey)
{
    struct mlx3_tx_raw * tx_desc    = NULL;
    struct mlx3_wqe_ctrl_seg * ctrl = NULL;
    int send_counter                = get_qp_send_counter(mlx, qp);
    void * buf_start                = NULL;

    ctrl = (((send_counter * qp->tx->bb_size)) % qp->tx->buf_size) + qp->tx->buf;  
    buf_start   = (void*) ctrl;
    switch (opcode) {

        case OP_RC_SEND:
            build_mlx_uc_wqe(mlx, (void*)ctrl, pkt, pkt_size);
            barrier();
            ctrl->owner_opcode = bswap32((MLX3_OPCODE_SEND) | ((qp->tx->valid) << 31));
            break;
        case OP_RC_SEND_IMM:
            ERROR("RC SEND IMMEDIATE NOT SUPPORTED!\n");
            return -1;
        case OP_UC_SEND:
            build_mlx_uc_wqe(mlx, (void*)ctrl, pkt, pkt_size);
            barrier();
            ctrl->owner_opcode = bswap32((MLX3_OPCODE_SEND) | ((qp->tx->valid) << 31));
            break;
        case OP_UC_SEND_IMM:
            ERROR("UC SEND IMMEDIATE NOT SUPPORTED!\n");
            return -1;
        case OP_UD_SEND:
            build_mlx_ud_wqe(mlx, (void*)ctrl, port, pkt, dst_qpn, qkey, pkt_size, slid, dlid);
            barrier();
            ctrl->owner_opcode = bswap32((MLX3_OPCODE_SEND) | ((qp->tx->valid) << 31));
            break;
        case OP_UD_SEND_IMM:
            ERROR("UD SEND IMMEDIATE NOT SUPPORTED!\n");
            break;
        case OP_RAW_SEND:
            build_mlx_raw_swqe(mlx, (void*)ctrl, pkt, dlid, pkt_size);
            tx_desc = (struct mlx3_tx_raw*)qp->tx->buf;
            barrier();
            tx_desc->ctrl.opcode = MLX3_OPCODE_SEND & 0x1f;
            tx_desc->ctrl.owner  = (send_counter & qp->tx->buf_size) << 7;
            break;
        case OP_RC_RDMA_WRITE:
            build_mlx_rdma_wqe(mlx, (void*)ctrl, pkt, pkt_size, va, rkey);
            barrier();
            ctrl->owner_opcode = bswap32((MLX3_OPCODE_RDMA_WRITE) | ((qp->tx->valid) << 31));
            break;
        case OP_RC_RDMA_READ:
            build_mlx_rdma_wqe(mlx, (void*)ctrl, pkt, pkt_size, va, rkey);
            barrier();
            ctrl->owner_opcode = bswap32((MLX3_OPCODE_RDMA_READ) | ((qp->tx->valid) << 31));
            break;
        case OP_UC_RDMA_WRITE:
            build_mlx_rdma_wqe(mlx, (void*)ctrl, pkt, pkt_size, va, rkey);
            barrier();
            ctrl->owner_opcode = bswap32((MLX3_OPCODE_RDMA_WRITE) | ((qp->tx->valid) << 31));
            break;
        case OP_RC_RECV:
        case OP_UC_RECV:
        case OP_UD_RECV:
        case OP_RAW_RECV:
            break;
        default:
            ERROR("Invalid transport opcode (%d)\n", opcode);
            return -1;

    }
    
    if (send_counter % qp->tx->bb_count)
        qp->tx->valid += 1;


    return 0;
}


static int
send_wqe (struct mlx3_qp * qp)
{
    qp->ring_func(qp);
    return 0;
}



static inline int
is_valid_dlid (int dlid)
{
    // TODO: is 0 a valid DLID???
    return dlid != 0;
}


static inline int
ctx_setup_transport_op (struct ib_context * ctx)
{
    switch (ctx->user_op) {
        case OP_RC_SEND:
        case OP_RC_RECV:
        case OP_RC_SEND_IMM:
        case OP_RC_RDMA_WRITE:
        case OP_RC_RDMA_READ:
            ctx->transport = MLX3_TRANS_RC;
            break;
        case OP_UC_SEND:
        case OP_UC_RECV:
        case OP_UC_SEND_IMM:
        case OP_UC_RDMA_WRITE:
            ctx->transport = MLX3_TRANS_UC;
            break;
        case OP_UD_SEND:
        case OP_UD_RECV:
        case OP_UD_SEND_IMM:
            ctx->transport = MLX3_TRANS_UD;
            break;
        case OP_RAW_SEND:
        case OP_RAW_RECV:
            ctx->transport = MLX3_TRANS_RAW;
            break;
        default:
            ERROR("Invalid transport op (0x%x)\n", ctx->user_op);
            return -1;
    }

    return 0;
}


static inline int
ctx_setup_queue_parms (struct ib_context * ctx)
{
    if (!ctx->sq_size)
        ctx->sq_size = SQ_SIZE_DEFAULT;

    if (!ctx->rq_size)
        ctx->rq_size = RQ_SIZE_DEFAULT;

    if (!ctx->sq_stride)
        ctx->sq_stride = SQ_STRIDE_DEFAULT;

    if (!ctx->rq_stride)
        ctx->rq_stride = RQ_STRIDE_DEFAULT;  

    return 0;
}


static inline int
ctx_setup_misc_parms (struct ib_context * ctx)
{
    if (!ctx->qkey)
        ctx->qkey = QKEY_DEFAULT;

    if (!ctx->port)
        ctx->port = PORT_DEFAULT;

    if (!ctx->dst_qpn)
        ctx->dst_qpn = DQPN_DEFAULT;

    return 0;
}


static int
fixup_ib_context (struct ib_context * ctx)
{
    if (!is_valid_dlid(ctx->dlid)) {
        ERROR("Invalid destination LID (%d)\n", ctx->dlid);
        return -1;
    }

    if (ctx_setup_transport_op(ctx) != 0) {
        ERROR("Could not setup tranport op\n");
        return -1;
    }

    if (ctx_setup_queue_parms(ctx) != 0) {
        ERROR("Could not setup queue parameters\n");
        return -1;
    }

    if (ctx_setup_misc_parms(ctx) != 0) {
        ERROR("Could not setup general parameters for IB context\n");
        return -1;
    }

    return 0;
}
static int
send_using_bf(struct mlx3_ib * mlx, struct mlx3_qp * qp)
{

    struct mlx3_wqe_ctrl_seg * ctrl = NULL;
    int send_counter                = get_qp_send_counter(mlx, qp);
    ctrl = (((send_counter * qp->tx->bb_size)) % qp->tx->buf_size) + qp->tx->buf;  
    ctrl->vlan_cv_f_ds  =  bswap32((qp->qpn << 8) | (sizeof(struct  mlx3_tx_ud_desc) / 16)) ; // size in 16 bytes
    ctrl->owner_opcode |= bswap32(send_counter << 8);
    bf_copy((uint64_t*)((mlx->uar_start + mlx->caps->uar_size) + (qp->uar * PAGE_SIZE_4KB)) , (uint64_t*)ctrl, sizeof(*ctrl));
    return 0;
}
static int 
mlx3_post_send (void * state,
                uint8_t * src,
                uint64_t len,
                void (*callback)(nk_net_dev_status_t, void *),
                void * context) 
{
    struct mlx3_ib * mlx    = (struct mlx3_ib *)state;
    struct ib_context * ctx = (struct ib_context *) context;
    struct mlx3_qp * qp     = NULL;
    fixup_ib_context(ctx);

    qp = get_or_create_qp(mlx, ctx);
    if (!qp) {
        ERROR("Could not post send, no available QPs\n");
        return -1;
    }

    build_wqe(mlx, qp, ctx->user_op, ctx->port, src, len, ctx->dst_qpn, ctx->qkey, ctx->dlid, ctx->slid, ctx->va, ctx->rkey);

    barrier();
    send_wqe(qp); 
    //send_using_bf(mlx, qp);    
    send_start = rdtsc();
    return 0;

}


static int 
mlx3_post_receive(void * state,
                  uint8_t * src,
                  uint64_t len,
                  void (*callback)(nk_net_dev_status_t, void *),
                  void * context) 
{
	
    struct mlx3_ib * mlx    = (struct mlx3_ib *)state;
    struct ib_context * ctx = (struct ib_context *) context;
    struct mlx3_qp * qp     = NULL;

    fixup_ib_context(context);

    qp = get_or_create_qp(mlx, ctx);
    if (!qp) {
        ERROR("Could not post receive, no available QPs\n");
        return -1; 
    }

    return 0;
}


static struct nk_net_dev_int mlx3_ops = {
	.get_characteristics = mlx3_get_characteristics,
	.post_receive        = mlx3_post_receive,
	.post_send           = mlx3_post_send,
};


static void 
mlx3_build_raw_pkt (void * pkt, int pkt_size, int src_qpn, int dst_qpn, uint32_t qkey, bth_op_t b_op, int src_lid, int dest_lid)
{
    struct ib_packet *  packet = NULL;
    uint32_t op_own;	
    int err = 0;


    packet = malloc(sizeof(struct ib_packet));
    if (!packet) {
        ERROR("Could not allocate IB packet\n");
    }
    memset(packet, 0, sizeof(struct ib_packet));

    int header_len = LRH_LEN + BTH_LEN + DETH_LEN + ICRC_LEN;

    build_ib_packet(packet, PKT_UD, dst_qpn, src_qpn, qkey, pkt_size, 0, 0, 0, b_op, src_lid, dest_lid);
    memcpy(pkt,                     (void*)  packet->header->lrh,                LRH_LEN);
    memcpy(pkt + LRH_LEN,                (void*) packet->header->bth,                BTH_LEN);
    memcpy(pkt + (LRH_LEN + BTH_LEN),  (void*) packet->header->deth,                      DETH_LEN);
    memcpy(pkt + (LRH_LEN + BTH_LEN + DETH_LEN),  (void*) packet->header->deth,                pkt_size - header_len);
}


static int 
mlx3_init_port (struct mlx3_ib * mlx, int port)
{
    int err;
    
    mzero_checked(mlx->port_cap, sizeof(struct mlx3_port_cap), "port caps struct");

    err = mlx3_mailbox_cmd(mlx, 0, 0, port, 0, CMD_INIT_PORT,
            CMD_TIME_CLASS_A);

    /* Wait for the port to be initialized */
    while (mlx->port_cap->link_state != 1) {
        mlx3_query_port(mlx, 1, mlx->port_cap, 0);  
    }

    DEBUG("PORT INITIALIZED\n");

    return 0;
}


static int
mlx3_init_sp_qp (struct mlx3_ib * mlx, struct mlx3_qp * sqp, int qp_type)
{
    int reserved_from_top = 0;
    int reserved_from_bot;
    int k;
    int fixed_reserved_from_bot_rv = 0;
    int bottom_reserved_for_rss_bitmap;

    DEBUG("Initializing special QP\n");

    // NOTE: Receive Side Scaling (Windows)/Receive Core Affinity(Linux) not considered for now
    if (mlx3_CONF_SPECIAL_QP(mlx, sqp->qpn, qp_type) != 0) {	
        ERROR("Could not conf special QP\n");
        return -1;
    }

    DEBUG("Special QP sucessfully inited\n");

    return 0;
}


static int 
map_eq (struct mlx3_ib * mlx, struct mlx3_eq* eq) 
{

    uint64_t async_ev_mask;
    nk_unmask_irq(eq->intr_vec);
    int err;
    //  We now map the *ALL* event types to this EQ
    //  TODO: should parameterize the types of events given to this EQ
    async_ev_mask = MLX3_ASYNC_EVENT_MASK;

    async_ev_mask |= (1ull << MLX3_EVENT_TYPE_PORT_MNG_CHG_EVENT);
    async_ev_mask |= (1ull << MLX3_EVENT_TYPE_RECOVERABLE_ERROR_EVENT);

    err = mlx3_MAP_EQ(mlx, async_ev_mask, 0, eq->eqn);

    if (err) {
        ERROR("Could not map EQ (code=%d)\n", err);
        return -1;
    }
    return 0;
}


static int
reg_eq_irq (struct mlx3_ib * mlx, struct mlx3_eq * eq) {

    if (register_irq_handler(eq->intr_vec, 
                eq_irq_handler, 
                (void*)mlx) != 0) {
        ERROR("Could not register MLX IRQ handler\n");
        return -1; 
    }

    eq->inta_pin = mlx->query_adapter->intapin;

    uint64_t clr_base = pci_dev_get_bar_addr(mlx->dev,
            mlx->fw_info->clr_int_bar) +
        mlx->fw_info->clr_base;

    // TODO: why is this creation affecting global state?
    eq->clr_mask = bswap32(1 << (eq->inta_pin ));

    eq->clr_int  = (uint32_t*)(clr_base +
            (eq->inta_pin < 32 ? 4 : 0));
    return 0;
}


static struct mlx3_eq *
init_eq (struct mlx3_ib * mlx)
{
    struct mlx3_eq * eq = NULL;
	int err = 0;

    eq = create_eq(mlx);
    
    if (!eq) {
		ERROR("Create EQ Failed");
		return NULL;
	}

    if (reg_eq_irq(mlx, eq)) {
        ERROR("Could not Register IRQ fo EQ\n");
        goto out_err;
    }
    if (map_eq(mlx, eq)) {
        goto out_err;
    }
        
	eq_set_ci(eq, 1);

    DEBUG("Created new EQ:\n");
    DEBUG("  # %d\n", eq->eqn);
    DEBUG("  Entry Count: %d\n", eq->nentry);
    DEBUG("  PCI INT pin: 0x%x\n", eq->pci_intr);
    DEBUG("  Host INT vector: 0x%x\n", eq->intr_vec);
    DEBUG("  Doorbell Addr: %p\n", eq->doorbell);
    DEBUG("  MTT addr: %p\n", eq->mtt);
    DEBUG("  Buffer Addr: %p\n", eq->buffer);

	return eq;

out_err:
    free(eq);
    return NULL;
}

static int log_num_mac = 7;

static int
mlx3_SET_PORT_qpn_calc(struct mlx3_ib *mlx, uint8_t port, uint32_t base_qpn,
        uint8_t promisc)
{
    mlx3_cmd_box_t *mailbox;
    struct mlx3_set_port_rqp_calc_context *context;
    int err;
    uint32_t in_mod ;
    uint32_t m_promisc = (mlx->caps->flags & MLX3_DEV_CAP_FLAG_VEP_MC_STEER) ?
        MCAST_DIRECT : MCAST_DEFAULT;

    mailbox = create_cmd_mailbox(mlx);
    if (!mailbox)
        ERROR("Could not create mailbox\n");
    context = (struct mlx3_set_port_rqp_calc_context *)mailbox->buf;
    context->base_qpn = bswap32(base_qpn);
    context->n_mac = 7; //linux value 7  can be 1 for minimum low profile configuration in linux
    context->promisc = bswap32(promisc << SET_PORT_PROMISC_SHIFT |
            base_qpn);
    context->mcast = bswap32(m_promisc << SET_PORT_MC_PROMISC_SHIFT |
            base_qpn);
    context->intra_no_vlan = 0;
    context->no_vlan = MLX3_NO_VLAN_IDX;
    context->intra_vlan_miss = 0;
    context->vlan_miss = MLX3_VLAN_MISS_IDX;

    in_mod = MLX3_SET_PORT_RQP_CALC << 8 | port;

    err = mlx3_mailbox_cmd(mlx, mailbox->buf, 0,in_mod, MLX3_SET_PORT_ETH_OPCODE,
            CMD_SET_PORT, CMD_TIME_CLASS_A);
    if (err) {
        ERROR("Set Port QPN failed %d\n", err);
        return -1;
    }

    destroy_cmd_mailbox(mailbox);
    return err;
}

#define SET_PORT_ROCE_2_FLAGS          0x10
#define MLX3_SET_PORT_ROCE_V1_V2       0x2
static int 
mlx3_SET_PORT_general(struct mlx3_ib *mlx, 
                      uint8_t port, 
                      int mtu,
                      uint8_t pptx, 
                      uint8_t pfctx, 
                      uint8_t pprx, 
                      uint8_t pfcrx)
{
    mlx3_cmd_box_t *mailbox;
    struct mlx3_set_port_general_context *context;
    int err;
    uint32_t in_mod;

    mailbox = create_cmd_mailbox(mlx);
    if (!mailbox)
        ERROR("Cannot create mailbox\n");
    context = (struct mlx3_set_port_general_context *)mailbox->buf;
    context->flags  = SET_PORT_GEN_MTU_ETH_VALID; 
    context->flags2 = MLX3_EN_10GE; 
    context->mtu    = bswap16(mtu);
  //  context->pptx = (pptx * (!pfctx)) << 7;
  //  context->pfctx = pfctx;
  //  context->pprx = (pprx * (!pfcrx)) << 7;
  //  context->pfcrx = pfcrx;

    in_mod = MLX3_SET_PORT_GENERAL << 8 | port;

    err = mlx3_mailbox_cmd(mlx, mailbox->buf, 0,in_mod, MLX3_SET_PORT_ETH_OPCODE,
            CMD_SET_PORT, CMD_TIME_CLASS_A);

    if (err) {
        ERROR("Set Port General failed %d\n", err);
        return -1;
    }
    destroy_cmd_mailbox(mailbox);
    return err;
}


static int 
mlx3_set_port_promiscuous (struct mlx3_ib *mlx, 
        uint8_t port,
        uint16_t qpn,
        uint8_t mode) 
{
    mlx3_cmd_box_t * mailbox;
    struct mlx3_set_port_ib_context *context;
    int err;
    uint32_t in_mod = port;

    mailbox = create_cmd_mailbox(mlx);
    if (!mailbox) {
        ERROR("Cannot create mailbox\n");
        return -1;
    }
    context = (struct mlx3_set_port_ib_context *)mailbox->buf;

    context->ingress = bswap32(qpn| (mode << 24)); // sniff all packets on QP 64

    context->egress = bswap32(qpn| (mode << 24)); // sniff all packets on QP 64
    err = mlx3_mailbox_cmd(mlx, mailbox->buf, 0, in_mod, MLX3_SET_PORT_IB_OPCODE,
            CMD_SET_PORT, CMD_TIME_CLASS_A);

    if (err) {
        ERROR("Set Port Promiscuous failed %d\n", err);
        return -1;
    }
    destroy_cmd_mailbox(mailbox);

    DEBUG("Set Port Promiscuous Succeded\n");
    return err;
}


static int 
post_nop_wqe (struct mlx3_ib * mlx, struct mlx3_cq * cq)
{
    DEBUG("Test NOP WQE \n");
    struct mlx3_wqe_ctrl_seg *ctrl;
    struct mlx3_wqe_inline_seg *inl;
    void *wqe;
    int s;
    int port = 1;

    struct mlx3_qp * qp;

    struct mlx3_qp_parms parms = {
        .sq_size = PAGE_SIZE_4KB,
        .is_sqp  = 0,
        .tp      = MLX3_TRANS_UD,
    };

    qp = mlx3_create_qp(mlx, cq, &parms);

    if (!qp) {
        ERROR("Could not create special QP\n");
        goto out_err;
    }

    /* transition it to RTS */
    if (mlx3_qp_to_rts(mlx, qp) != 0) {
        ERROR("QP to RTS Failed\n");
        return -1;
    }

    ctrl = qp->tx->buf;

    build_mlx_ud_nops_wqe(mlx, qp->tx->buf, port);

    /*
     * Make sure descriptor is fully written before setting ownership bit
     * (because HW can start executing as soon as we do).
     */

    ctrl->owner_opcode = bswap32(MLX3_OPCODE_NOP | qp->tx->valid << 7);

    qp->ring_func(qp);

out_err:
    return -1;
}


enum mlx3_ib_mad_ifc_flags {
    MLX3_MAD_IFC_IGNORE_MKEY    = 1,
    MLX3_MAD_IFC_IGNORE_BKEY    = 2,
    MLX3_MAD_IFC_IGNORE_KEYS    = (MLX3_MAD_IFC_IGNORE_MKEY |
                       MLX3_MAD_IFC_IGNORE_BKEY),
    MLX3_MAD_IFC_NET_VIEW       = 4,
};


/**
 * ib_lid_cpuint16_t - Return lid in 16bit CPU encoding.
 *     In the current implementation the only way to get
 *     get the 32bit lid is from other sources for OPA.
 *     For IB, lids will always be 16bits so cast the
 *     value accordingly.
 *
 * @lid: A 32bit LID
 */
static inline uint16_t 
ib_lid_cpuint16_t(uint32_t lid)
{
    return (uint16_t)lid;
}


static int 
mlx3_MAD_IFC(struct mlx3_ib *mlx, int mad_ifc_flags,
             int port, const struct ib_wc *in_wc,
             const struct ib_grh *in_grh,
             const void *in_mad, void *response_mad)
{   
    mlx3_cmd_box_t *inmailbox, *outmailbox;
    void *inbox;
    int err;
    uint32_t in_modifier = port; 
    uint8_t op_modifier = 0;

    inmailbox = create_cmd_mailbox(mlx);
    if (!inmailbox)
        return -1; 
    inbox = (void *)inmailbox->buf;

    outmailbox = create_cmd_mailbox(mlx);
    if (!outmailbox) {
        destroy_cmd_mailbox(inmailbox);
        return -1;
    }

    memcpy(inbox, in_mad, 256);

    /*
     * Key check traps can't be generated unless we have in_wc to
     * tell us where to send the trap.
     */
    if ((mad_ifc_flags & MLX3_MAD_IFC_IGNORE_MKEY) || !in_wc)
        op_modifier |= 0x1;
    if ((mad_ifc_flags & MLX3_MAD_IFC_IGNORE_BKEY) || !in_wc)
        op_modifier |= 0x2;

    if (in_wc) {
        struct {
            uint32_t my_qpn;
            uint32_t reserved1;
            uint32_t rqpn;
            uint8_t  sl;
            uint8_t  g_path;
            uint16_t reserved2[2];
            uint16_t pkey;
            uint32_t reserved3[11];
            uint8_t  grh[40];
        } *ext_info;

        memset(inbox + 256, 0, 256);
        ext_info = inbox + 256;

        ext_info->my_qpn = bswap32(in_wc->qp->qpn);
        ext_info->rqpn   = bswap32(in_wc->src_qp);
        ext_info->sl     = in_wc->sl << 4;
        ext_info->g_path = in_wc->dlid_path_bits |
                            (in_wc->wc_flags & IB_WC_GRH ? 0x80 : 0);
        ext_info->pkey   = bswap16(in_wc->pkey_index);

        if (in_grh)
            memcpy(ext_info->grh, in_grh, 40);

        op_modifier |= 0x4;

        in_modifier |= ib_lid_cpuint16_t(in_wc->slid) << 16;
    }

    err = mlx3_mailbox_cmd(mlx, 
            inmailbox->buf, 
            outmailbox->buf, 
            in_modifier,
            (op_modifier & ~0x8) ,
            CMD_MAD_IFC, 
            CMD_TIME_CLASS_A);

    if (!err)
        memcpy(response_mad, (void*)outmailbox->buf, 256);

    destroy_cmd_mailbox(inmailbox);
    destroy_cmd_mailbox(outmailbox);

    return err;
}


static void 
init_query_mad (struct ib_smp *mad) 
{   
    mad->base_version  = 1;
    mad->mgmt_class    = IB_MGMT_CLASS_SUBN_LID_ROUTED;
    mad->class_version = 1;
    mad->method        = IB_MGMT_METHOD_GET;
} 


static int
query_mad_ifc (struct mlx3_ib * mlx)
{
    struct ib_smp *in_mad  = NULL;
    struct ib_smp *out_mad = NULL;
    int mad_ifc_flags = MLX3_MAD_IFC_IGNORE_KEYS;
    int err = -ENOMEM; 
    uint8_t node_guid[64];
    uint8_t node_desc[64];
    uint32_t rev_id;
    in_mad  = malloc(sizeof *in_mad);

    out_mad = malloc(sizeof *out_mad);
    if (!in_mad || !out_mad) {
        ERROR("Malloc Failed\n");
        goto out;
    }

    init_query_mad(in_mad);
    in_mad->attr_id = IB_SMP_ATTR_NODE_DESC;
        mad_ifc_flags |= MLX3_MAD_IFC_NET_VIEW;

    err = mlx3_MAD_IFC(mlx, mad_ifc_flags, 1, NULL, NULL, in_mad, out_mad);
    if (err)
        goto out;

    memcpy(node_desc, out_mad->data, IB_DEVICE_NODE_DESC_MAX);

    in_mad->attr_id = IB_SMP_ATTR_NODE_INFO;

    err = mlx3_MAD_IFC(mlx, mad_ifc_flags, 1, NULL, NULL, in_mad, out_mad);
    if (err)
        goto out;

    //rev_id = bswap32((uint32_t)(out_mad->data + 32));
    memcpy((void*)node_guid, out_mad->data + 12, 8);

out:
    return -1;
}


static int 
mlx3_ib_query_sl2vl (struct mlx3_ib * mlx, uint8_t port, uint64_t *sl2vl_tbl)
{
    union sl2vl_tbl_to_u64 sl2vl64;
    struct ib_smp *in_mad  = NULL;
    struct ib_smp *out_mad = NULL;
    int mad_ifc_flags = MLX3_MAD_IFC_IGNORE_KEYS;
    int err = -ENOMEM;
    int jj;

    in_mad  = malloc(sizeof(*in_mad));
    out_mad = malloc(sizeof(*out_mad));
    if (!in_mad || !out_mad)
        goto out;

    init_query_mad(in_mad);
    in_mad->attr_id  = IB_SMP_ATTR_SL_TO_VL_TABLE;
    in_mad->attr_mod = 0;

    err = mlx3_MAD_IFC(mlx, mad_ifc_flags, port, NULL, NULL,
            in_mad, out_mad);
    if (err)
        goto out;   

    for (jj = 0; jj < 8; jj++)
        sl2vl64.sl8[jj] = ((struct ib_smp *)out_mad)->data[jj];
    *sl2vl_tbl = sl2vl64.sl64;

out:
    free(in_mad); 
    free(out_mad);
    return err;
}


/* 
 * This sets the service layer to virtual lane mapping.
 * This information is derived from the SM (subnet manager) 
 * TODO: this isn't correct yet
 */
static void 
mlx3_init_sl2vl_tbl (struct mlx3_ib * mlx)
{
    uint64_t sl2vl = -1;
    int i;
    int err;

    for (i = 1; i <= 1; i++) {
        err = mlx3_ib_query_sl2vl(mlx, i, &sl2vl);
        if (err) {
            ERROR("Unable to get default sl to vl mapping for port %d.  Using all zeroes (%d)\n",
                    i, err);
            sl2vl = -1;
        }
        //      atomic64_set(&mdev->sl2vl[i - 1], sl2vl);
    }

    DEBUG("SL2VL %d\n", sl2vl);
}


/*
 * enable packet sniffing (promisc. mode) for port 1
 */
static inline int 
mlx3_sniff_all_packets (struct mlx3_ib * mlx)
{
    return mlx3_set_port_promiscuous(mlx, 1, 64, 0x2);
}


/*
 * enables this port for Ethernet mode 
 */
static int 
mlx3_enable_ethernet (struct mlx3_ib * mlx, int port, int mtu)
{
    // TODO: If using Ethernet set port should be configued by default it is infiniband
    int ret;

    ret = mlx3_SET_PORT_general(mlx, port,
            mtu + ETH_FCS_LEN,
            0,
            0,
            0,
            0);

    if (ret) {
        ERROR("Failed setting port general configurations for port %d, with error %d\n", port, ret);
        return -1;
    }

    ret = mlx3_SET_PORT_qpn_calc(mlx, port, mlx->nt_rsvd_qpn, 1);

    if (ret) {
        ERROR("Failed setting default ethernet QP numbers (err=%d)\n", ret);
        return -1;
    }

    return 0;
}


static int
mlx3_test_completion (struct mlx3_ib* mlx)
{
   return post_nop_wqe(mlx, mlx->cqs[0]);
} 


static int
mlx3_create_special_qp (struct mlx3_ib * mlx)
{
#define NUM_SP_QP 2
    struct mlx3_qp ** qps = mlx->qps;

    for (int i = 0; i < NUM_SP_QP; i++) {

        qps[i] = create_qp_with_parms(mlx, mlx->cqs[0], MLX3_TRANS_RAW, 2048, 2048, 1);

        if (!qps[i]) {
            ERROR("Could not create special QP\n");
            goto out_err;
        }
    }

    for (int i = 0; i < NUM_SP_QP; i++) {
        // transition it to RTS 
        if (mlx3_qp_to_rtr(mlx, qps[i]) != 0) {
            ERROR("QP to RTS Failed\n");
            return -1;
        }
    }
    // Qp Type 0 Management Qp 1 Service Qp
    if (mlx3_init_sp_qp(mlx, qps[0], MANAGEMENT_QP) != 0) {
        ERROR("Could not init special QP\n");
        goto out_err;
    }

    if (mlx3_init_sp_qp(mlx, qps[1], MANAGEMENT_QP) != 0) {
        ERROR("Could not init special QP\n");
        goto out_err;
    }
    return 0;
out_err:
    return -1;
}


/* 
 * initializes the EQ map *and* creates all the EQs ahead of time
 */
static struct mlx3_eq ** 
init_eq_map (struct mlx3_ib * mlx, int num_eqs)
{
    int i;

    struct mlx3_eq ** eq_map = NULL;

    eq_map = malloc(sizeof(struct mlx3_eq*) * num_eqs);

    if (!eq_map) {
        ERROR("Could not create CQ map\n");
        return NULL;
    }

    for (i = 0; i < num_eqs; i++) {

        eq_map[i] = init_eq(mlx);
        
        if (!eq_map[i]) {
            ERROR("Could not Initialize EQ %d\n", i);
            goto out_err; 
        }
    }

    return eq_map;

out_err:
    free(eq_map);
    return NULL;
}

/* 
 * initializes the CQ map *and* creates all the CQs ahead of time
 */
static struct mlx3_cq ** 
init_cq_map (struct mlx3_ib * mlx, struct mlx3_eq * eq, int num_cqs)
{
    int i;

    struct mlx3_cq ** cq_map = NULL;

    cq_map = malloc(sizeof(struct mlx3_cq*) * num_cqs);

    if (!cq_map) {
        ERROR("Could not create CQ map\n");
        return NULL;
    }

    for (i = 0; i < num_cqs; i++) {

        cq_map[i] = mlx3_create_cq(mlx, eq, DEFAULT_NUM_CQE);

        if (!cq_map[i]) {
            ERROR("Could not create CQ %d\n", i);
            return NULL;
        }
    }

    return cq_map;
}


/*
 * Creates a map of QPs. The QPs themselves will be created
 * later on demand.
 */
static struct mlx3_qp **
init_qp_map (struct mlx3_ib * mlx, int num_qps)
{
    struct mlx3_qp ** qp_map = NULL;

    qp_map = malloc(sizeof(struct mlx3_qp*) * num_qps);
    if (!qp_map) {
        ERROR("Could not create QP map\n");
        return NULL;
    }

    memset(mlx->qps, 0, sizeof(struct mlx3_qp*) * num_qps);

    for (int i = 0; i < num_qps; i++) {
        qp_map[i] = NULL;    
    }


    return qp_map;
}


static inline void 
free_cqs (struct mlx3_cq ** cqs) 
{

    for (int i = 0; i < NUM_CQS; i++) {
        if(cqs[i]) {
            free(cqs[i]);
        }
    }
}


static inline void 
free_eqs (struct mlx3_eq ** eqs) 
{
    for (int i = 0; i < NUM_EQS; i++) {
        if (eqs[i]) {
            free(eqs[i]);
        }
    }
}


static inline void 
free_qps (struct mlx3_qp ** qps) 
{
    for (int i = 0; i < NUM_QPS; i++) {
        if (qps[i]) {
            free(qps[i]);
        }
    }
}


static inline void
free_icm_tables (struct mlx3_ib * mlx)
{
    free(mlx->eq_table.table.icm);
    free(mlx->eq_table.cmpt_table.icm);
    free(mlx->cq_table.table.icm);
    free(mlx->cq_table.cmpt_table.icm);
    free(mlx->qp_table.qp_table.icm);
    free(mlx->qp_table.rdmarc_table.icm);
    free(mlx->qp_table.altc_table.icm);
    free(mlx->qp_table.auxc_table.icm);
    free(mlx->qp_table.cmpt_table.icm);
    free(mlx->mr_table.dmpt_table.icm);
    free(mlx->mr_table.mtt_table.icm);
    free(mlx->mcg_table.table.icm);
    free(mlx->srq_table.table.icm);
    free(mlx->srq_table.cmpt_table.icm);
}


int
ud_pingpong (struct mlx3_ib * mlx)
{


    int limit = 512 * 1024 * 1024;
    int pkt_size = 1024;
    uint64_t start, end, diff;
    for (int size = 0 ; size < 3 * (64 *1024); size += 64 * 1024) { 

        void * pkt = malloc(pkt_size);
        memset(pkt, 0x7a, pkt_size);
        struct ib_context * ctx = malloc(sizeof(struct ib_context));
        memset(ctx, 0, sizeof(struct ib_context));
        ctx->dlid   = 0x21;
        ctx->slid   = 0x6;
        ctx->user_op = OP_UD_SEND;
        // ctx->sq_size = PAGE_SIZE_4KB ;
        ctx->src_qpn = 64;
        // TODO BRIAN: For some reason packet above 2k causing send to fail
        start = rdtsc();
        mlx3_post_send(mlx, pkt, pkt_size, NULL, ctx);
        end =rdtsc();
        DEBUG("Time Taken Post Send  %d\n", (end - start));
        free(pkt);
    } 
    return 0;
}
    
static inline void
init_queue_offsets (struct mlx3_ib * mlx) 
{
    // This should return the first non reserved cq,qp,eq number
    mlx->nt_rsvd_cqn  = 1 << mlx->caps->log2_rsvd_cqs;     
    mlx->nt_rsvd_qpn  = 1 << mlx->caps->log2_rsvd_qps;
    mlx->nt_rsvd_dmpt = 1 << mlx->caps->log2_rsvd_mrws;
    mlx->nt_rsvd_eqn  = mlx->caps->num_rsvd_eqs; 
    // For SQ and CQ Uar Doorbell index starts from 128
    mlx->uar_ind_scq_db = 128;

    /*
     * Each UAR has 4 EQ doorbells; so if a UAR is reserved, then
     * we can't use any EQs whose doorbell falls on that page,
     * even if the EQ itself isn't reserved.
     */

    mlx->uar_ind_eq_db = mlx->caps->num_rsvd_eqs / 4;
}


int 
mlx3_init (struct naut_info * naut) 
{
    struct mlx3_init_hca_param init_hca;
    struct pci_dev * idev = NULL;
    struct mlx3_ib * mlx  = NULL;
    uint32_t max          = 1;
    uint64_t icm_size     = 0;
    uint64_t aux_pages    = 0;
    int ret               = 0;
    int port =1;
    int mtu = 1500 - ETH_FCS_LEN;
    int i;

    DEBUG("Searching for InfiniBand ConnectX-3 Card...\n");

    ret = pci_find_matching_devices(MLX3_VENDORID, 
            MLX3_DEV_ID, 
            &idev, 
            &max);

    if (ret) {
        DEBUG("No ConnectX-3 found\n");
        return 0;
    }

    //TODO: Use GOTO on error returns properly
    INFO("InfiniBand ConnectX-3 Initializing...\n");

    mzero_checked(mlx, sizeof(struct mlx3_ib), "mlx state");

    mlx->dev      = idev;
    mlx->pci_intr = idev->cfg.dev_cfg.intr_pin;

    mlx->mmio_start = pci_dev_get_bar_addr(idev, 0);
    mlx->mmio_sz    = pci_dev_get_bar_size(idev, 0);

    mlx->uar_start  = pci_dev_get_bar_addr(idev, 2);
    mlx->netdev     = nk_net_dev_register("mlx3-ib", 0, &mlx3_ops, (void*)mlx);

    if (!mlx->netdev) {
        ERROR("Could not register mlx3 device\n");
        goto out_err;
    }

    __mzero_checked(mlx->port_cap, sizeof(struct mlx3_port_cap), "Could not allocate port caps\n", goto out_err);

    pci_dev_enable_mmio(idev);
    pci_dev_enable_master(idev);

    if (mlx3_reset(mlx) != 0) {
        ERROR("Could not reset device\n");
        goto out_err1;
    }

    // TODO: this shouldn't be necessary if we're properly
    // restoring the config space in reset()
    pci_dev_enable_mmio(idev);
    pci_dev_enable_master(idev);

    if (!mlx3_check_ownership(mlx)) {
        ERROR("We don't have card ownership, aborting\n");
        goto out_err1;
    }

    if (mlx3_query_fw(mlx) != 0) {
        ERROR("Could not query firwmware\n");
        goto out_err1;
    }

    if (mlx3_map_fw_area(mlx) != 0) {
        ERROR("Could not map firwmare area\n");
        goto fw_err;
    }

    if (mlx3_run_fw(mlx) != 0) {
        ERROR("Could not run firmware\n");
        goto cap_err;
    }

    if (mlx3_query_dev_cap(mlx) != 0) {
        ERROR("Could not query device capabilities\n");
        goto cap_err;
    }

    if (mlx3_query_port(mlx, 1, mlx->port_cap, 1) != 0) {
        ERROR("Query Port Failed\n");
        goto out_err1;
    }

    init_queue_offsets(mlx);

    if (mlx3_get_icm_size(mlx, &icm_size, &init_hca) != 0) {
        ERROR("Could not get ICM size\n");
        goto icm_err;
    }

    if (mlx3_set_icm(mlx, icm_size, &aux_pages) != 0) {
        ERROR("Could not set ICM size\n");
        goto icm_err;
    }

    if (mlx3_map_icm_aux(mlx, aux_pages) != 0) {
        ERROR("Could not map ICM aux space\n");
        goto icm_err;
    }

    if (mlx3_map_icm_tables(mlx, &init_hca) != 0) {
        ERROR("Could not map ICM space\n");
        goto icm_err1;
    }

    if (mlx3_init_hca(mlx, &init_hca)) {
        ERROR("Could not init HCA\n");
        goto hca_err;
    }	

    // Give us the Interrupt pin
    if (mlx3_query_adapter(mlx)) {
        ERROR("\n Could not Query Adapter\n");
        goto out_err;
    }

    mlx->eqs = init_eq_map(mlx, NUM_EQS);
    if (!mlx->eqs) {
        ERROR("Could not create EQ map\n");
        goto hca_err;
    }

    mlx->cqs = init_cq_map(mlx, mlx->eqs[0], NUM_CQS);
    if (!mlx->cqs) {
        ERROR("Could not create CQ map\n");
        goto cq_err; 
    }
   
    mlx->qps = init_qp_map(mlx, NUM_QPS);
    if (!mlx->qps) {
        ERROR("Could not init QP map\n");
        goto qp_err; 
    }

    if (mlx3_init_port(mlx, 1)) {
        ERROR("Could not init port\n");
        goto port_err;
    }

    // wait for the port to come up
    while (link != 1) {
        udelay(100);
    }

   
/**
 * Receive Packet Interface
 * For recv the src and dest lids will exchange
 */ 
#if 0 

    void * pkt = malloc (4096);
    memset(pkt, 0x7a, 4096);

    struct ib_context * ctx = malloc(sizeof(struct ib_context));
    memset(ctx, 0, sizeof(struct ib_context));

    ctx->dlid   = 0x26;
    ctx->slid   = 0x21;
    ctx->opcode = UD_RECV;
    mlx3_post_receive (mlx, pkt, PAGE_SIZE_4KB, NULL, ctx);
    //mlx3_rcv_pkt(mlx, mlx->cqs[0] , MLX3_UC);
#endif

/** 
 *Send Packet Interface UUser only knows to know about the destination lid and opcode
 * For raw packets you need to build the packet before post send an UD exaple is implemented in  mlx3_build_raw_pkt()  
 */
#if 1 
    ud_pingpong(mlx);
#endif

    DEBUG("ConnectX3 up and running...\n");

    while (1) {
        udelay(600000);
        //dump_packet(mlx, (void*)(0x3fa1c000), 512); 
        //mlx3_ib_query_qp(mlx, mlx->qps[mlx3_get_qpn_offset(mlx, 64)]);
        //mlx3_query_port(mlx, 1, mlx->port_cap, 1);  
    }

    return 0;

port_err:
    free_qps(mlx->qps);
    free(mlx->qps);
qp_err:
    free_cqs(mlx->cqs);
    free(mlx->cqs);
cq_err:
    free_eqs(mlx->eqs);
    free(mlx->eqs);
hca_err:
    free_icm_tables(mlx);
icm_err1:
    free(mlx->fw_info->aux_icm);
icm_err:
    free(mlx->caps);
cap_err:
	free(mlx->fw_info->data);
fw_err:
    free(mlx->fw_info); 
out_err1:
    free(mlx->port_cap);
    free(mlx->query_adapter);
    nk_net_dev_unregister(mlx->netdev);
out_err:
    ERROR("ConnectX-3 Initialization failed\n");
    free(mlx);
    return -1;
}

