/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _CRYSTALHD_DMABUF_H_
#define _CRYSTALHD_DMABUF_H_

#include <linux/types.h>
#include "bc_dts_glob_lnx.h"

struct crystalhd_cmd;

int crystalhd_dmabuf_export(struct crystalhd_cmd *ctx,
				BC_RX_DMABUF_EXPORT *req);
void crystalhd_dmabuf_release_all(struct crystalhd_cmd *ctx);

#endif
