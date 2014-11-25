#ifndef __NAUT_DEBUG_H__
#define __NAUT_DEBUG_H__

/* WE DONT SUPPORT THESE YET! */
#undef __AVX__
#undef __AVX2__
#undef __SSE2__

#ifdef NAUT_CONFIG_LEGION_RT_DEBUG

extern "C" int printk(const char* fmt, ...);
extern "C" void** nk_get_tid();
#undef NAUTILUS_DEEP_DEBUG
#define NAUTILUS_DEEP_DEBUG(fmt, args...) printk("(%x) LEGION %s:%s:%d: " fmt, nk_get_tid(),  __FILE__, __func__, __LINE__, ##args)

#else

#undef NAUTILUS_DEEP_DEBUG
#define NAUTILUS_DEEP_DEBUG(fmt, args...) 

#endif

#endif /* !NAUT_DEBUG_H__ */
