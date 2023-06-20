#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <libdrm/drm.h>
#include <libdrm/drm_mode.h>

int main() {
  int dri_fd = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);

  uint64_t res_fb_buf[10] = {0}, res_crtc_buf[10] = {0}, res_conn_buf[10] = {0}, res_enc_buf[10] = {0};

  struct drm_mode_card_res res = {0};

  ioctl(dri_fd, DRM_IOCTL_SET_MASTER, 0);

  ioctl(dri_fd, DRM_IOCTL_MODE_GETRESOURCES, &res);
  res.fb_id_ptr = (uint64_t)res_fb_buf;
  res.crtc_id_ptr = (uint64_t)res_crtc_buf;
  res.connector_id_ptr = (uint64_t)res_conn_buf;
  res.encoder_id_ptr = (uint64_t)res_enc_buf;
  ioctl(dri_fd, DRM_IOCTL_MODE_GETRESOURCES, &res);

  printf("fb: %d, crtc: %d, conn: %d, enc: %d\n", res.count_fbs, res.count_crtcs, res.count_connectors, res.count_encoders);

  void *fb_base[10];
  long fb_w[10];
  long fb_h[10];

  for(int i = 0; i < res.count_connectors; i++) {
    struct drm_mode_modeinfo conn_mode_buf[20] = {0};
    uint64_t conn_prop_buf[20] = {0}, conn_propval_buf[20] = {0}, conn_enc_buf[20] = {0};

    struct drm_mode_get_connector conn = {0};

    conn.connector_id = res_conn_buf[i];

    ioctl(dri_fd, DRM_IOCTL_MODE_GETCONNECTOR, &conn);
    conn.modes_ptr = (uint64_t)conn_mode_buf;
    conn.props_ptr = (uint64_t)conn_prop_buf;
    conn.prop_values_ptr = (uint64_t)conn_propval_buf;
    conn.encoders_ptr = (uint64_t)conn_enc_buf;
    ioctl(dri_fd, DRM_IOCTL_MODE_GETCONNECTOR, &conn);

    if(conn.count_encoders < 1 || conn.count_modes < 1 || !conn.encoder_id || !conn.connection) {
      printf("Not connected\n");
      continue;
    }

    struct drm_mode_create_dumb create_dumb = {0};
    struct drm_mode_map_dumb map_dumb = {0};
    struct drm_mode_fb_cmd cmd_dumb = {0};

    create_dumb.width = conn_mode_buf[0].hdisplay;
    create_dumb.height = conn_mode_buf[0].vdisplay;
    create_dumb.bpp = 32;
    create_dumb.flags = 0;
    create_dumb.pitch = 0;
    create_dumb.size = 0;
    create_dumb.handle = 0;
    ioctl(dri_fd, DRM_IOCTL_MODE_CREATE_DUMB, &create_dumb);

    cmd_dumb.width = create_dumb.width;
    cmd_dumb.height = create_dumb.height;
    cmd_dumb.bpp = create_dumb.bpp;
    cmd_dumb.pitch = create_dumb.pitch;
    cmd_dumb.depth = 24;
    cmd_dumb.handle = create_dumb.handle;
    ioctl(dri_fd, DRM_IOCTL_MODE_ADDFB, &cmd_dumb);

    map_dumb.handle = create_dumb.handle;
    ioctl(dri_fd, DRM_IOCTL_MODE_MAP_DUMB, &map_dumb);

    fb_base[i] = mmap(0, create_dumb.size, PROT_READ | PROT_WRITE, MAP_SHARED, dri_fd, map_dumb.offset);
    fb_w[i] = create_dumb.width;
    fb_h[i] = create_dumb.height;

    printf("%d : mode: %d, prop: %d, enc: %d\n", conn.connection, conn.count_modes, conn.count_props, conn.count_encoders);
    printf("modes: %dx%d FB: %d\n", conn_mode_buf[0].hdisplay, conn_mode_buf[0].vdisplay, fb_base[i]);

    struct drm_mode_get_encoder enc = {0};

    enc.encoder_id = conn.encoder_id;
    ioctl(dri_fd, DRM_IOCTL_MODE_GETENCODER, &enc); 

    struct drm_mode_crtc crtc = {0};

    crtc.crtc_id = enc.crtc_id;
    ioctl(dri_fd, DRM_IOCTL_MODE_GETCRTC, &crtc);

    crtc.fb_id = cmd_dumb.fb_id;
    crtc.set_connectors_ptr = (uint64_t)&res_conn_buf[i];
    crtc.count_connectors = 1;
    crtc.mode = conn_mode_buf[0];
    crtc.mode_valid = 1;
    ioctl(dri_fd, DRM_IOCTL_MODE_SETCRTC, &crtc);
  }

  ioctl(dri_fd, DRM_IOCTL_DROP_MASTER, 0);

  for(int i = 0; i < 100; i++) {
    for(int j = 0; j < res.count_connectors; j++) {
      int col = 0xFF0000;
      for (int y = 0;y < fb_h[j]; y++) {
        for (int x = 0;x < fb_w[j]; x++) {
          int location = y*(fb_w[j]) + x;
          *(((uint32_t*)fb_base[j]) + location) = col;
        }
      }
    }
    usleep(100000);
  }

  return 0;
}
