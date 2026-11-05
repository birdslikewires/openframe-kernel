/*
 * kmsclient: minimal KMS client. Takes the display from the console with a dumb
 * buffer, draws into it, then exits so that the console is restored. Exercises
 * the console/scanout handover around console scrolling. Not shipped.
 *   kmsclient [seconds]
 */
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <drm/drm.h>
#include <drm/drm_mode.h>

#define IO(fd, r, a) do { if (ioctl(fd, r, a)) { perror(#r); exit(1); } } while (0)

int main(int argc, char **argv)
{
	int secs = argc > 1 ? atoi(argv[1]) : 3, fd = open("/dev/dri/card0", O_RDWR);
	struct drm_mode_card_res res = {0};
	struct drm_mode_get_connector c = {0};
	struct drm_mode_get_encoder e = {0};
	struct drm_mode_create_dumb d = {0};
	struct drm_mode_fb_cmd f = {0};
	struct drm_mode_map_dumb m = {0};
	struct drm_mode_crtc crtc = {0};
	uint32_t conns[8], crtcs[8], encs[8], conn_id;
	struct drm_mode_modeinfo modes[32];
	uint32_t *px;

	if (fd < 0) { perror("card0"); return 1; }
	IO(fd, DRM_IOCTL_SET_MASTER, 0);
	res.connector_id_ptr = (uintptr_t)conns; res.count_connectors = 8;
	res.crtc_id_ptr = (uintptr_t)crtcs; res.count_crtcs = 8;
	IO(fd, DRM_IOCTL_MODE_GETRESOURCES, &res);
	conn_id = conns[0];
	c.connector_id = conn_id; c.modes_ptr = (uintptr_t)modes; c.count_modes = 32;
	c.encoders_ptr = (uintptr_t)encs; c.count_encoders = 8;
	IO(fd, DRM_IOCTL_MODE_GETCONNECTOR, &c);
	e.encoder_id = c.encoder_id;
	IO(fd, DRM_IOCTL_MODE_GETENCODER, &e);
	d.width = modes[0].hdisplay; d.height = modes[0].vdisplay; d.bpp = 32;
	IO(fd, DRM_IOCTL_MODE_CREATE_DUMB, &d);
	f.width = d.width; f.height = d.height; f.pitch = d.pitch; f.bpp = 32; f.depth = 24; f.handle = d.handle;
	IO(fd, DRM_IOCTL_MODE_ADDFB, &f);
	m.handle = d.handle;
	IO(fd, DRM_IOCTL_MODE_MAP_DUMB, &m);
	px = mmap(0, d.size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, m.offset);
	if (px == MAP_FAILED) { perror("mmap"); return 1; }
	crtc.crtc_id = e.crtc_id; crtc.fb_id = f.fb_id; crtc.set_connectors_ptr = (uintptr_t)&conn_id;
	crtc.count_connectors = 1; crtc.mode = modes[0]; crtc.mode_valid = 1;
	IO(fd, DRM_IOCTL_MODE_SETCRTC, &crtc);
	for (int t = 0; t < secs * 10; t++) {
		for (uint32_t y = 0; y < d.height; y++)
			for (uint32_t x = 0; x < d.width; x += 8)
				px[y * (d.pitch / 4) + x] = ((x + t * 8) & 255) << 16 | (y & 255) << 8 | 0x40;
		usleep(50000);
	}
	printf("kmsclient ok %ux%u\n", d.width, d.height);
	return 0;	/* exit: last close restores the console */
}
