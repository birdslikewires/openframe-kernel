/*
 * Runs fh.c's write path in user space against a fake 1 MB chip, to check the
 * writable/unprotect guards without writing a real one:
 *
 *	cc -I. -include kstub.h -o test_guards test_guards.c && ./test_guards
 *
 * Exits non-zero if any case fails.
 */
#include "../fh.c"

#define MB 0x100000
#define BLK 0x10000

static unsigned char chip[MB];
static int erased[16], programmed[16];
static int tamper_at = -1;	/* copy_from_user call after which the buffer changes */
static int copies;
static unsigned char *tamper_buf;

unsigned long test_copy_from_user(void *d, const void *s, unsigned long n)
{
	memcpy(d, s, n);
	if (++copies == tamper_at)
		tamper_buf[0] ^= 0xff;	/* the caller's buffer changes under us */
	return 0;
}

static int f_unlock(unsigned long b) { return 0; }
static int f_lock(unsigned long b) { return 0; }
static int f_erase(unsigned long b) { erased[b / BLK]++; memset(chip + b, 0xff, BLK); return 0; }
/* Like the real chip: programming can only clear bits. After an erase (was == NULL) that gives the
 * buffer; without one, each byte programmed becomes chip & buffer. */
static int f_program(unsigned long b, unsigned char *buf, const unsigned char *was)
{
	programmed[b / BLK]++;
	for (int i = 0; i < BLK; i++)
		if (was ? buf[i] != was[i] : buf[i] != 0xff)
			chip[b + i] &= buf[i];
	return 0;
}

static int fails;
static void check(int ok, const char *what) { printf("%s %s\n", ok ? "ok  " : "FAIL", what); fails += !ok; }

static void reset(void)
{
	for (int i = 0; i < MB; i++)
		chip[i] = (unsigned char)(i * 7 + (i >> 16));
	memset(erased, 0, sizeof erased);
	memset(programmed, 0, sizeof programmed);
	tamper_at = -1;
	copies = 0;
}

static int touched(void) { int n = 0; for (int i = 0; i < 16; i++) n += erased[i] + programmed[i]; return n; }

static ssize_t wr(unsigned char *img, unsigned long off, size_t n)
{
	struct file f = { .f_mode = FMODE_WRITE };
	loff_t pos = off;
	ssize_t r;
	int o = fh_open(NULL, &f);
	if (o < 0)
		return o;
	r = fh_write(&f, (const char *)img + off, n, &pos);
	fh_release(NULL, &f);
	return r;
}

static ssize_t wrp(unsigned char *img, unsigned long off, size_t n)
{
	struct file f = { .f_mode = FMODE_WRITE };
	loff_t pos = off;
	ssize_t r;
	int o = fh_open(NULL, &f);
	if (o < 0)
		return o;
	r = fhp_write(&f, (const char *)img + off, n, &pos);
	fh_release(NULL, &f);
	return r;
}

int main(void)
{
	static unsigned char img[MB];
	char m[128];

	fh.vdata = chip;
	fh.size = MB;
	fh.lock = f_lock; fh.unlock = f_unlock; fh.erase = f_erase; fh.program = f_program;
	sema_init(&fh.sem, 1);
	tamper_buf = img;

	/* read only by default */
	reset(); memcpy(img, chip, MB); img[9 * BLK] ^= 1;
	writable = false; unprotect = false;
	{ struct file f = { .f_mode = FMODE_WRITE }; check(fh_open(NULL, &f) == -EPERM, "open for writing refused without writable"); }
	{ struct file f = { .f_mode = 0 }; check(fh_open(NULL, &f) == 0, "open for reading allowed"); fh_release(NULL, &f); }
	{ struct file f = { .f_mode = 0 }; loff_t p = 9 * BLK; fh_open(NULL, &f);
	  check(fh_write(&f, (char *)img + 9 * BLK, BLK, &p) == -EPERM && !touched(), "write() refused without writable, even on a read-only open"); fh_release(NULL, &f); }

	writable = true;
	reset(); memcpy(img, chip, MB); img[9 * BLK + 5] ^= 1;
	check(wr(img, 9 * BLK, BLK) == BLK && programmed[9] == 1 && touched() == 2 && !memcmp(chip, img, MB), "block 9 written with writable");

	reset(); memcpy(img, chip, MB); img[3] ^= 1; img[9 * BLK] ^= 1;
	check(wr(img, 0, MB) == -EPERM && !touched(), "whole image with a changed block 0: refused, nothing written");

	reset(); memcpy(img, chip, MB); img[14 * BLK] ^= 1;
	for (int b = 1; b < 14; b++) img[b * BLK + 1] ^= 1;
	check(wr(img, 0, MB) == -EPERM && !touched(), "whole image with a changed block 14: refused before blocks 1-13");

	reset(); memcpy(img, chip, MB); img[15 * BLK + 0xffff] ^= 1;
	check(wr(img, 15 * BLK, BLK) == -EPERM && !touched(), "block 15 alone: refused");

	reset(); memcpy(img, chip, MB);
	for (int b = 1; b < 14; b++) img[b * BLK + 2] ^= 1;
	snprintf(m, sizeof m, "whole image, protected blocks unchanged: blocks 1-13 written, 0/14/15 untouched");
	check(wr(img, 0, MB) == MB && touched() == 26 && !erased[0] && !erased[14] && !erased[15] && !memcmp(chip, img, MB), m);

	reset(); memcpy(img, chip, MB);
	tamper_at = 1;	/* pre-scan copies block 0, then the buffer changes before the write loop's copy */
	check(wr(img, 0, BLK) == -EPERM && !touched() && copies == 2, "buffer changed after the check: block 0 still refused");

	unprotect = true;
	reset(); memcpy(img, chip, MB); img[7] ^= 1; img[14 * BLK] ^= 1;
	check(wr(img, 0, MB) == MB && programmed[0] == 1 && programmed[14] == 1 && touched() == 4 && !memcmp(chip, img, MB), "unprotect: blocks 0 and 14 written");
	unprotect = false;

	reset(); memcpy(img, chip, MB);
	check(wr(img, 9 * BLK + 512, BLK) == -EINVAL && !touched(), "unaligned offset still refused");
	check(wr(img, 9 * BLK, 512) == -EINVAL && !touched(), "partial block still refused");
	check(wr(img, 9 * BLK, BLK) == BLK && !touched(), "unchanged block skipped");

	/* /dev/fhp: program without erasing */
	writable = false;
	reset(); memcpy(img, chip, MB); memset(chip + 10 * BLK, 0xff, BLK); memcpy(img, chip, MB); img[10 * BLK + 4] = 0x5a;
	{ struct file f = { .f_mode = 0 }; loff_t p = 10 * BLK; fh_open(NULL, &f);
	  check(fhp_write(&f, (char *)img + 10 * BLK, BLK, &p) == -EPERM && !touched(), "fhp: refused without writable"); fh_release(NULL, &f); }
	writable = true;
	reset(); memset(chip + 10 * BLK, 0xff, BLK); memcpy(img, chip, MB);
	for (int i = 0; i < 24; i++) img[10 * BLK + i] = (unsigned char)(0xa5 ^ i);
	check(wrp(img, 10 * BLK, BLK) == BLK && programmed[10] == 1 && !erased[10] && !memcmp(chip, img, MB), "fhp: blank bytes programmed, block never erased");

	reset(); memcpy(img, chip, MB); img[10 * BLK + 9] = chip[10 * BLK + 9] | 0x01; chip[10 * BLK + 9] &= 0xfe; img[10 * BLK + 9] = chip[10 * BLK + 9] | 0x01;
	check(wrp(img, 10 * BLK, BLK) == -EINVAL && !touched(), "fhp: a bit 0 -> 1 refused, nothing written");

	reset(); chip[10 * BLK + 7] = 0xf8; memcpy(img, chip, MB); img[10 * BLK + 7] = 0xe8;
	check(wrp(img, 10 * BLK, BLK) == BLK && chip[10 * BLK + 7] == 0xe8 && !erased[10] && !memcmp(chip, img, MB), "fhp: one byte F8 -> E8 (clearing a bit) programmed");

	reset(); memset(chip + 10 * BLK, 0xff, 2 * BLK); memcpy(img, chip, MB); img[10 * BLK] = 0x00;
	chip[11 * BLK + 3] = 0x00; img[11 * BLK + 3] = 0x10;
	check(wrp(img, 10 * BLK, 2 * BLK) == -EINVAL && !touched() && chip[10 * BLK] == 0xff, "fhp: second block bad -> first block not written either");

	reset(); memcpy(img, chip, MB); chip[0x8000] = 0xff; memcpy(img, chip, MB); img[0x8000] = 0x00;
	check(wrp(img, 0, BLK) == -EPERM && !touched(), "fhp: NVRAM (block 0) change refused without unprotect");
	reset(); memcpy(img, chip, MB); chip[15 * BLK] = 0xff; memcpy(img, chip, MB); img[15 * BLK] = 0x00;
	check(wrp(img, 15 * BLK, BLK) == -EPERM && !touched(), "fhp: boot block change refused without unprotect");

	reset(); memcpy(img, chip, MB);
	check(wrp(img, 10 * BLK, BLK) == BLK && !touched(), "fhp: unchanged block skipped");
	check(wrp(img, 10 * BLK + 16, BLK) == -EINVAL && !touched(), "fhp: unaligned offset refused");
	check(wrp(img, 10 * BLK, 100) == -EINVAL && !touched(), "fhp: partial block refused");

	reset(); memset(chip + 10 * BLK, 0xff, BLK); memcpy(img, chip, MB); img[10 * BLK] = 0x00; img[10 * BLK + 1] = 0x7f;
	tamper_at = 1;	/* the pre-check passes, then the caller's buffer changes (byte 0: 00 -> FF, harmless) */
	check(wrp(img, 10 * BLK, BLK) == BLK && chip[10 * BLK + 1] == 0x7f && !erased[10], "fhp: buffer changed after the check: still only clears bits");

	printf("%s\n", fails ? "FAILED" : "all passed");
	return fails != 0;
}
