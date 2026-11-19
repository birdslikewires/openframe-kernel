// SPDX-License-Identifier: GPL-2.0
#include <linux/kernel.h>
#include <linux/dma-buf.h>
#include <linux/dma-mapping.h>
#include <linux/errno.h>
#include <linux/file.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/fcntl.h>
#include <linux/iosys-map.h>
#include <linux/module.h>
#include <linux/version.h>

#include "crystalhd_lnx.h"
#include "crystalhd_cmds.h"
#include "crystalhd_dmabuf.h"

/* dma_buf_* are exported in the DMA_BUF namespace; 6.13 made the name a string literal. */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 13, 0)
MODULE_IMPORT_NS("DMA_BUF");
#else
MODULE_IMPORT_NS(DMA_BUF);
#endif

#define CHD_DMABUF_ALIGN	64U
#define CHD_DMABUF_NAME	"crystalhd-rx"

struct crystalhd_dmabuf_slot {
	struct device	*dev;
	struct dma_buf	*dmabuf;
	void		*cpu_addr;
	dma_addr_t	dma_addr;
	size_t		size;
	uint32_t	index;
	uint32_t	y_stride;
	uint32_t	uv_stride;
	uint32_t	uv_offset;
	unsigned long	attrs;
};

struct crystalhd_dmabuf_pool {
	struct crystalhd_dmabuf_slot	slots[BC_MAX_DMABUF_EXPORT];
	uint32_t	count;
};

static inline uint32_t chd_align(uint32_t val)
{
	return ALIGN(val, CHD_DMABUF_ALIGN);
}

static struct crystalhd_dmabuf_pool *chd_get_pool(struct crystalhd_cmd *ctx)
{
	return (struct crystalhd_dmabuf_pool *)ctx->dmabuf_priv;
}

static void chd_put_pool(struct crystalhd_cmd *ctx,
			       struct crystalhd_dmabuf_pool *pool)
{
	ctx->dmabuf_priv = pool;
}

static int chd_dmabuf_attach(struct dma_buf *dbuf,
			 struct dma_buf_attachment *attach)
{
	struct crystalhd_dmabuf_slot *slot = dbuf->priv;
	struct sg_table *sgt;
	int ret;

	sgt = kzalloc(sizeof(*sgt), GFP_KERNEL);
	if (!sgt)
		return -ENOMEM;

	ret = dma_get_sgtable_attrs(slot->dev, sgt, slot->cpu_addr,
					   slot->dma_addr, slot->size,
					   slot->attrs);
	if (ret) {
		kfree(sgt);
		return ret;
	}

	attach->priv = sgt;
	return 0;
}

static void chd_dmabuf_detach(struct dma_buf *dbuf,
			     struct dma_buf_attachment *attach)
{
	struct sg_table *sgt = attach->priv;

	if (!sgt)
		return;

	sg_free_table(sgt);
	kfree(sgt);
}

static struct sg_table *chd_dmabuf_map(struct dma_buf_attachment *attach,
				        enum dma_data_direction dir)
{
	int ret;
	struct sg_table *sgt = attach->priv;

	ret = dma_map_sgtable(attach->dev, sgt, dir, 0);
	if (ret)
		return ERR_PTR(ret);

	return sgt;
}

static void chd_dmabuf_unmap(struct dma_buf_attachment *attach,
			        struct sg_table *sgt,
			        enum dma_data_direction dir)
{
	dma_unmap_sgtable(attach->dev, sgt, dir, 0);
}

static int chd_dmabuf_mmap(struct dma_buf *dbuf, struct vm_area_struct *vma)
{
	struct crystalhd_dmabuf_slot *slot = dbuf->priv;

	return dma_mmap_attrs(slot->dev, vma, slot->cpu_addr,
			       slot->dma_addr, slot->size,
			       slot->attrs);
}

static int chd_dmabuf_vmap(struct dma_buf *dbuf, struct iosys_map *map)
{
	struct crystalhd_dmabuf_slot *slot = dbuf->priv;

	if (!map || !slot)
		return -EINVAL;

	iosys_map_set_vaddr(map, slot->cpu_addr);
	return 0;
}

static void chd_dmabuf_vunmap(struct dma_buf *dbuf, struct iosys_map *map)
{
	/* Nothing to do for linear WC buffers */
}

static int chd_dmabuf_begin_cpu(struct dma_buf *dbuf,
			      enum dma_data_direction dir)
{
	return 0;
}

static int chd_dmabuf_end_cpu(struct dma_buf *dbuf,
			    enum dma_data_direction dir)
{
	return 0;
}

static void chd_dmabuf_release(struct dma_buf *dbuf)
{
	struct crystalhd_dmabuf_slot *slot = dbuf->priv;

	if (!slot)
		return;

	if (slot->cpu_addr)
		dma_free_attrs(slot->dev, slot->size, slot->cpu_addr,
			      slot->dma_addr, slot->attrs);

	slot->cpu_addr = NULL;
	slot->dmabuf = NULL;
}

static const struct dma_buf_ops chd_dmabuf_ops = {
	.attach		= chd_dmabuf_attach,
	.detach		= chd_dmabuf_detach,
	.map_dma_buf	= chd_dmabuf_map,
	.unmap_dma_buf	= chd_dmabuf_unmap,
	.mmap		= chd_dmabuf_mmap,
	.vmap		= chd_dmabuf_vmap,
	.vunmap		= chd_dmabuf_vunmap,
	.begin_cpu_access = chd_dmabuf_begin_cpu,
	.end_cpu_access	= chd_dmabuf_end_cpu,
	.release	= chd_dmabuf_release,
};

static void chd_dmabuf_reset_slot(struct crystalhd_dmabuf_slot *slot)
{
	memset(slot, 0, sizeof(*slot));
}

void crystalhd_dmabuf_release_all(struct crystalhd_cmd *ctx)
{
	struct crystalhd_dmabuf_pool *pool = chd_get_pool(ctx);
	uint32_t i;

	if (!pool)
		return;

	for (i = 0; i < pool->count && i < BC_MAX_DMABUF_EXPORT; i++) {
		if (pool->slots[i].dmabuf)
			dma_buf_put(pool->slots[i].dmabuf);
		else if (pool->slots[i].cpu_addr)
			dma_free_attrs(pool->slots[i].dev,
				       pool->slots[i].size,
				       pool->slots[i].cpu_addr,
				       pool->slots[i].dma_addr,
				       pool->slots[i].attrs);
		chd_dmabuf_reset_slot(&pool->slots[i]);
	}

	pool->count = 0;
}

static struct crystalhd_dmabuf_pool *chd_dmabuf_get_or_create_pool(struct crystalhd_cmd *ctx)
{
	struct crystalhd_dmabuf_pool *pool = chd_get_pool(ctx);

	if (pool)
		return pool;

	pool = kzalloc(sizeof(*pool), GFP_KERNEL);
	if (!pool)
		return NULL;

	chd_put_pool(ctx, pool);
	return pool;
}

int crystalhd_dmabuf_export(struct crystalhd_cmd *ctx,
				BC_RX_DMABUF_EXPORT *req)
{
	struct device *dev = chddev();
	struct crystalhd_dmabuf_pool *pool;
	unsigned int count, i;
	uint32_t width, height;
	uint32_t y_stride, y_height, uv_stride, uv_height;
	size_t total;
	int ret = 0;

	if (!ctx || !req)
		return BC_STS_INV_ARG;

	pool = chd_dmabuf_get_or_create_pool(ctx);
	if (!pool)
		return BC_STS_INSUFF_RES;

	crystalhd_dmabuf_release_all(ctx);

	count = req->requested;
	if (!count)
		count = min_t(unsigned int, BC_MAX_DMABUF_EXPORT, 6);
	else if (count > BC_MAX_DMABUF_EXPORT)
		count = BC_MAX_DMABUF_EXPORT;

	width = req->width ? req->width : 1280;
	height = req->height ? req->height : 720;

	y_stride = chd_align(width);
	y_height = ALIGN(height, 2);
	uv_stride = y_stride;
	uv_height = ALIGN(height / 2, 1);
	total = (size_t)y_stride * y_height +
		(size_t)uv_stride * uv_height;

	memset(req->desc, 0, sizeof(req->desc));
	req->allocated = 0;
	req->width = width;
	req->height = height;

	for (i = 0; i < count; i++) {
		struct crystalhd_dmabuf_slot *slot = &pool->slots[i];
		struct dma_buf_export_info exp = {
			.ops = &chd_dmabuf_ops,
			.size = total,
			.flags = O_RDWR,
			.priv = slot,
			.exp_name = CHD_DMABUF_NAME,
		};
		int fd;

		chd_dmabuf_reset_slot(slot);
		slot->dev = dev;
		slot->attrs = DMA_ATTR_WRITE_COMBINE;
		slot->cpu_addr = dma_alloc_attrs(dev, total, &slot->dma_addr,
					       GFP_KERNEL | __GFP_ZERO,
					       slot->attrs);
		if (!slot->cpu_addr) {
			ret = BC_STS_INSUFF_RES;
			break;
		}

		slot->size = total;
		slot->index = i;
		slot->y_stride = y_stride;
		slot->uv_stride = uv_stride;
		slot->uv_offset = y_stride * y_height;

		slot->dmabuf = dma_buf_export(&exp);
		if (IS_ERR(slot->dmabuf)) {
			ret = BC_STS_ERROR;
			dma_free_attrs(dev, total, slot->cpu_addr,
				       slot->dma_addr, slot->attrs);
			slot->cpu_addr = NULL;
			break;
		}

		fd = dma_buf_fd(slot->dmabuf, O_CLOEXEC);
		if (fd < 0) {
			ret = BC_STS_INSUFF_RES;
			dma_buf_put(slot->dmabuf);
			slot->dmabuf = NULL;
			break;
		}

		req->desc[i].index = slot->index;
		req->desc[i].y_stride = slot->y_stride;
		req->desc[i].uv_stride = slot->uv_stride;
		req->desc[i].uv_offset = slot->uv_offset;
		req->desc[i].length = slot->size;
		req->desc[i].dmabuf_fd = fd;
		req->allocated++;
	}

	pool->count = req->allocated;

	if (!req->allocated) {
		crystalhd_dmabuf_release_all(ctx);
		return ret ? ret : BC_STS_ERROR;
	}

	return ret ? ret : BC_STS_SUCCESS;
}
