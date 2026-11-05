// SPDX-License-Identifier: GPL-2.0-only
/*
 * fbstress: pixel-exact test of the gma500 console fill/copy operations.
 *
 * Calls fbcon's own entry points (info->fbops) with random rectangles and
 * checks the framebuffer against a software model. Every operation has its
 * last-written pixel read back immediately, to catch the engine reporting idle
 * before its writes have landed; the whole screen is compared periodically.
 * Not part of the shipped kernel; loaded by hand for testing.
 *
 *   insmod fbstress.ko ops=200000 seed=1 ; dmesg | tail
 */
#include <linux/console.h>
#include <linux/fb.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/io.h>
#include <linux/ktime.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/prandom.h>
#include <linux/sched.h>
#include <linux/vmalloc.h>

static int ops = 20000;
module_param(ops, int, 0444);
static int seed = 1;
module_param(seed, int, 0444);
static int check_every = 500;
module_param(check_every, int, 0444);
static int small_only;
module_param(small_only, int, 0444);
static int atomic;			/* run with interrupts disabled */
module_param(atomic, int, 0444);

/*
 * Modes for testing the failure paths:
 *  wedge=1      point the engine at unmapped memory so that it faults and stalls.
 *               The driver must time out once, fall back to software and carry
 *               on; drawing must stay correct. Reboot afterwards.
 *  oops_sim=1   run with oops_in_progress set: the engine must not be touched.
 *  sabotage=1   negative control: dirty one pixel behind the model's back just
 *               before the last full compare. The test must report FAIL, or it
 *               proves nothing.
 *  bench=1      time fixed console-shaped operations, engine against software.
 *
 * Later operations can paint over an earlier error, so check_every=1 (compare
 * the whole screen after every operation) is the only strict setting.
 *  real_oops=1  take a real oops once finished, with no locks held.
 */
static int wedge;
module_param(wedge, int, 0444);
static int oops_sim;
module_param(oops_sim, int, 0444);
static int sabotage;
module_param(sabotage, int, 0444);
static int bench;
module_param(bench, int, 0444);
static int real_oops;
module_param(real_oops, int, 0444);

#define CR_TWOD_REQ_BASE 0x0C88
#define CR_BLIT_STATUS	0x0E04
#define CR_BIF_INT_STAT	0x0C04

static struct rnd_state rnd;
static u32 r32(u32 n) { return n ? prandom_u32_state(&rnd) % n : 0; }

static int pick_dim(u32 max)
{
	static const u16 interesting[] = { 1, 2, 7, 8, 9, 15, 16, 17, 31, 32, 33, 64, 255, 256 };
	u32 v;

	switch (r32(small_only ? 2 : 4)) {
	case 0:
		v = interesting[r32(ARRAY_SIZE(interesting))];
		break;
	case 1:
		v = 1 + r32(64);
		break;
	default:
		v = 1 + r32(max);
		break;
	}
	return v > max ? max : v;
}

static u64 time_fill(struct fb_info *info, u32 w, u32 h, int n)
{
	struct fb_fillrect fr = { .width = w, .height = h, .color = 0, .rop = ROP_COPY };
	u64 t = ktime_get_ns();
	int i;

	for (i = 0; i < n; i++) {
		fr.color = i & 7;
		info->fbops->fb_fillrect(info, &fr);
	}
	return div64_u64(ktime_get_ns() - t, n * 1000ULL);
}

static u64 time_copy(struct fb_info *info, u32 w, u32 h, u32 hop, int n)
{
	struct fb_copyarea ca = { .sx = 0, .sy = hop, .dx = 0, .dy = 0, .width = w, .height = h };
	u64 t = ktime_get_ns();
	int i;

	for (i = 0; i < n; i++)
		info->fbops->fb_copyarea(info, &ca);
	return div64_u64(ktime_get_ns() - t, n * 1000ULL);
}

static void run_bench(struct fb_info *info)
{
	static const struct { u32 w, h; const char *what; } f[] = {
		{ 16, 16, "2 cells" }, { 800, 16, "one text row" },
		{ 800, 240, "half screen" }, { 800, 480, "clear screen" },
	};
	static const struct { u32 w, h, hop; const char *what; } c[] = {
		{ 16, 16, 16, "2 cells" }, { 400, 16, 16, "half a text row" },
		{ 800, 464, 16, "scroll one row" },
	};
	int i, pass;

	for (pass = 0; pass < 2; pass++) {
		const char *who = pass ? "software" : "engine";

		if (pass)
			oops_in_progress++;
		for (i = 0; i < ARRAY_SIZE(f); i++)
			pr_info("fbstress: BENCH %-8s fill %3ux%-3u %-14s %6llu us\n", who,
				f[i].w, f[i].h, f[i].what,
				time_fill(info, f[i].w, f[i].h, 40));
		for (i = 0; i < ARRAY_SIZE(c); i++)
			pr_info("fbstress: BENCH %-8s copy %3ux%-3u %-14s %6llu us\n", who,
				c[i].w, c[i].h, c[i].what,
				time_copy(info, c[i].w, c[i].h, c[i].hop, 40));
		if (pass)
			oops_in_progress--;
	}
}

static int __init fbstress_init(void)
{
	struct file *f;
	struct fb_info *info;
	struct pci_dev *pdev;
	void __iomem *sgx;
	u32 *model, *line;
	u32 w, h, pitch, blit0, blit1, bif;
	u64 fills = 0, copies = 0, soft = 0, tfill = 0, tcopy = 0, mism_last = 0, mism_full = 0;
	u64 maxns = 0;
	unsigned long irqflags = 0;
	int i, ret = 0, first_bad = -1;

	prandom_seed_state(&rnd, seed);
	pdev = pci_get_device(0x8086, 0x8108, NULL);
	if (!pdev)
		return -ENODEV;
	sgx = ioremap(pci_resource_start(pdev, 0) + 0x40000, 0x8000);
	pci_dev_put(pdev);
	if (!sgx)
		return -ENOMEM;

	f = filp_open("/dev/fb0", O_RDWR, 0);
	if (IS_ERR(f)) {
		iounmap(sgx);
		return PTR_ERR(f);
	}
	info = f->private_data;
	w = info->var.xres;
	h = info->var.yres;
	pitch = info->fix.line_length;

	model = vmalloc(array_size(pitch, h));
	line = vmalloc(pitch);
	if (!model || !line) {
		ret = -ENOMEM;
		goto out;
	}

	console_lock();
	if (bench) {
		run_bench(info);
		console_unlock();
		ret = -EAGAIN;
		goto out;
	}
	memcpy_fromio(model, info->screen_base, pitch * h);
	blit0 = ioread32(sgx + CR_BLIT_STATUS) & 0xffffff;
	if (wedge) {
		pr_info("fbstress: wedging engine, REQ_BASE was %08x\n",
			ioread32(sgx + CR_TWOD_REQ_BASE));
		iowrite32(ioread32(sgx + CR_TWOD_REQ_BASE) + 0x08000000,
			  sgx + CR_TWOD_REQ_BASE);
		(void)ioread32(sgx + CR_TWOD_REQ_BASE);
	}
	if (oops_sim)
		oops_in_progress++;

	for (i = 0; i < ops; i++) {
		u32 op = r32(10);
		u32 rw = pick_dim(w), rh = pick_dim(h);
		u32 dx = r32(w - rw + 1), dy = r32(h - rh + 1);
		u32 lastx, lasty, expect, got, y;
		ktime_t t0;
		u64 ns;

		if (atomic)
			local_irq_save(irqflags);
		if (op < 4) {
			struct fb_fillrect fr = {
				.dx = dx, .dy = dy, .width = rw, .height = rh,
				.color = r32(16), .rop = ROP_COPY,
			};
			u32 c = ((u32 *)info->pseudo_palette)[fr.color];

			t0 = ktime_get();
			info->fbops->fb_fillrect(info, &fr);
			got = readl(info->screen_base + (dy + rh - 1) * pitch + (dx + rw - 1) * 4);
			ns = ktime_get_ns() - ktime_to_ns(t0);
			tfill += ns;
			fills++;
			for (y = dy; y < dy + rh; y++)
				memset32(model + y * (pitch / 4) + dx, c, rw);
			expect = c;
		} else {
			/* Half the copies are short hops, so that they overlap. */
			u32 sx, sy;
			struct fb_copyarea ca;

			if (op < 7) {
				int ox = (int)dx + (int)r32(33) - 16;
				int oy = (int)dy + (int)r32(33) - 16;

				sx = clamp(ox, 0, (int)(w - rw));
				sy = clamp(oy, 0, (int)(h - rh));
			} else {
				sx = r32(w - rw + 1);
				sy = r32(h - rh + 1);
			}
			ca = (struct fb_copyarea){ .sx = sx, .sy = sy, .dx = dx, .dy = dy,
						   .width = rw, .height = rh };

			/* Model of the copy, honouring overlap. */
			if (sy < dy) {
				for (y = rh; y-- > 0; )
					memmove(model + (dy + y) * (pitch / 4) + dx,
						model + (sy + y) * (pitch / 4) + sx, rw * 4);
			} else {
				for (y = 0; y < rh; y++)
					memmove(model + (dy + y) * (pitch / 4) + dx,
						model + (sy + y) * (pitch / 4) + sx, rw * 4);
			}
			t0 = ktime_get();
			info->fbops->fb_copyarea(info, &ca);
			/* The corner the engine writes last depends on the direction. */
			lastx = sx < dx ? dx : dx + rw - 1;
			lasty = sy < dy ? dy : dy + rh - 1;
			got = readl(info->screen_base + lasty * pitch + lastx * 4);
			ns = ktime_get_ns() - ktime_to_ns(t0);
			tcopy += ns;
			copies++;
			expect = model[lasty * (pitch / 4) + lastx];
		}
		if (atomic)
			local_irq_restore(irqflags);
		if (rw == 8 || rh == 8)
			soft++;
		if (ns > maxns)
			maxns = ns;
		if (got != expect) {
			if (!mism_last)
				pr_err("fbstress: op %d: last pixel %08x, expected %08x\n", i, got, expect);
			mism_last++;
		}

		if (i % check_every == check_every - 1 || i == ops - 1) {
			if (sabotage && i == ops - 1) {
				model[123 * (pitch / 4) + 45] ^= 0x00ffffff;
			}
			for (y = 0; y < h; y++) {
				memcpy_fromio(line, info->screen_base + y * pitch, w * 4);
				if (memcmp(line, model + y * (pitch / 4), w * 4)) {
					mism_full++;
					if (first_bad < 0)
						first_bad = i;
					if (mism_full <= 8) {
						u32 x, n = 0, fx = 0, g = 0, m = 0, lx = 0;

						for (x = 0; x < w; x++)
							if (line[x] != model[y * (pitch / 4) + x]) {
								if (!n++) {
									fx = x;
									g = line[x];
									m = model[y * (pitch / 4) + x];
								}
								lx = x;
							}
						pr_err("fbstress: DIFF op %d row %u x %u..%u (%u px) got %08x expected %08x\n",
						       i, y, fx, lx, n, g, m);
					}
					memcpy_fromio(model + y * (pitch / 4),
						      info->screen_base + y * pitch, w * 4);
				}
			}
			/*
			 * Let the system breathe. The cursor can be drawn while the
			 * lock is dropped, so take the screen as read again afterwards.
			 * Every operation has already been checked under the lock.
			 */
			console_unlock();
			cond_resched();
			console_lock();
			memcpy_fromio(model, info->screen_base, pitch * h);
		}
	}
	if (oops_sim)
		oops_in_progress--;
	blit1 = ioread32(sgx + CR_BLIT_STATUS) & 0xffffff;
	bif = ioread32(sgx + CR_BIF_INT_STAT);
	console_unlock();

	pr_info("fbstress: RESULT seed=%d atomic=%d ops=%d fills=%llu copies=%llu engine_blits=%u 8px_software=%llu engine_total=%u\n",
		seed, atomic, ops, fills, copies, (blit1 - blit0) & 0xffffff, soft, blit1);
	pr_info("fbstress: RESULT mismatches last_pixel=%llu full_rows=%llu bif_int=%08x avg_fill_us=%llu avg_copy_us=%llu max_op_us=%llu\n",
		mism_last, mism_full, bif,
		fills ? div64_u64(tfill, fills * 1000) : 0,
		copies ? div64_u64(tcopy, copies * 1000) : 0, div64_u64(maxns, 1000));
	pr_info("fbstress: %s\n", (mism_last || mism_full) ? "FAIL" : "PASS");
	ret = -EAGAIN;	/* always unload: this is a one-shot test */
out:
	if (real_oops) {
		pr_info("fbstress: taking a deliberate oops now\n");
		*(volatile int *)NULL = 1;
	}
	vfree(line);
	vfree(model);
	filp_close(f, NULL);
	iounmap(sgx);
	return ret;
}
module_init(fbstress_init);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("gma500 console 2D pixel-exact stress test");
