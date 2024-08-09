#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <sys/mman.h>
#include <drm/drm.h>
#include <drm/drm_mode.h>
#include <string.h>


typedef struct {
    int fd;
    drmModeRes *res;
    drmModeConnector *connector;
    drmModeEncoder *encoder;
    drmModeCrtc *crtc;
} DrmDevice;


void cleanup_drm_device(DrmDevice *device) {
    if (device->crtc) {
        drmModeFreeCrtc(device->crtc);
        device->crtc = NULL;
    }
    if (device->connector) {
        drmModeFreeConnector(device->connector);
        device->connector = NULL;
    }
    if (device->encoder) {
        drmModeFreeEncoder(device->encoder);
        device->encoder = NULL;
    }
    if (device->res) {
        drmModeFreeResources(device->res);
        device->res = NULL;
    }
    if (device->fd >= 0) {
        close(device->fd);
        device->fd = -1;
    }
}


DrmDevice init_drm_device(const char **device_paths, int num_paths) {
    DrmDevice device = { .fd = -1, .res = NULL, .connector = NULL, .encoder = NULL, .crtc = NULL };
    for (int i = 0; i < num_paths; i++) {
        device.fd = open(device_paths[i], O_RDWR | O_CLOEXEC);
        if (device.fd < 0) {
            perror("Cannot open device");
            continue;
        }
        device.res = drmModeGetResources(device.fd);
        if (device.res) {
            printf("Using device: %s\n", device_paths[i]);
            return device;
        } else {
            perror("Cannot get DRM resources");
            close(device.fd);
            device.fd = -1;
        }
    }
    return device;
}


drmModeConnector* find_valid_connector(int fd, drmModeRes *res) {
    drmModeConnector *connector = NULL;
    for (int i = 0; i < res->count_connectors; i++) {
        connector = drmModeGetConnector(fd, res->connectors[i]);
        if (connector) {
            if (connector->connection == DRM_MODE_CONNECTED) {
                return connector;
            }
            drmModeFreeConnector(connector);
        }
    }
    return NULL;
}


drmModeEncoder* find_valid_encoder(int fd, drmModeRes *res, drmModeConnector *connector) {
    drmModeEncoder *encoder = NULL;
    for (int i = 0; i < res->count_encoders; i++) {
        encoder = drmModeGetEncoder(fd, res->encoders[i]);
        if (encoder) {
            if (encoder->encoder_id == connector->encoder_id) {
                return encoder;
            }
            drmModeFreeEncoder(encoder);
        }
    }
    return NULL;
}

DrmDevice open_drm_device(const char *device_paths[], int num_paths) {
    DrmDevice device = {0};

    device = init_drm_device(device_paths, num_paths);
    if (device.fd < 0 || !device.res) {
        fprintf(stderr, "Failed to find a valid DRM device\n");
        return device;
    }

    device.connector = find_valid_connector(device.fd, device.res);
    if (!device.connector) {
        fprintf(stderr, "No connected monitor found\n");
        cleanup_drm_device(&device);
        return device;
    }

    int connector_id = device.connector->connector_id;
    printf("Connected monitor found: connector ID %d\n", connector_id);

    device.encoder = find_valid_encoder(device.fd, device.res, device.connector);
    if (!device.encoder) {
        fprintf(stderr, "Failed to find a valid encoder for the connector\n");
        cleanup_drm_device(&device);
        return device;
    }

    int crtc_id = device.encoder->crtc_id;
    drmModeFreeEncoder(device.encoder);
    device.encoder = NULL; // Clear the encoder as it is freed

    device.crtc = drmModeGetCrtc(device.fd, crtc_id);
    if (!device.crtc) {
        fprintf(stderr, "Failed to get CRTC information\n");
        cleanup_drm_device(&device);
        return device;
    }

    return device;
}


void *create_framebuffer(int fd, int width, int height, int bpp, uint32_t *fb_id) {
    struct drm_mode_create_dumb create_req;
    struct drm_mode_map_dumb map_req;
    struct drm_mode_destroy_dumb destroy_req;
    void *map;
    // Create dumb buffer
    memset(&create_req, 0, sizeof(create_req));
    create_req.width = width;
    create_req.height = height;
    create_req.bpp = bpp;
    if (drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &create_req) < 0) {
        perror("Cannot create dumb buffer");
        return NULL;
    }
    // Create framebuffer
    if (drmModeAddFB(fd, create_req.width, create_req.height, 24, create_req.bpp, create_req.pitch, create_req.handle, fb_id)) {
        perror("Cannot create framebuffer");
        return NULL;
    }
    // Prepare buffer for memory mapping
    memset(&map_req, 0, sizeof(map_req));
    map_req.handle = create_req.handle;
    if (drmIoctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &map_req)) {
        perror("Cannot prepare dumb buffer for mapping");
        return NULL;
    }
    // Perform actual memory mapping
    map = mmap(0, create_req.size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, map_req.offset);
    if (map == MAP_FAILED) {
        perror("Cannot map dumb buffer");
        return NULL;
    }

    return map;
}


int main()
{
    const char *device_paths[] = {"/dev/dri/card0", "/dev/dri/card1"};
    DrmDevice device = open_drm_device(device_paths, 2);

    if (device.fd < 0 || !device.res || !device.connector || !device.crtc) {
        return 1;
    }

    int width = 1920;
    int height = 1080;
    int bpp = 32;

    uint32_t fb_ids[3];
    void *framebuffers[3];
    for (int i = 0; i < 3; i++) {
        framebuffers[i] = create_framebuffer(device.fd, width, height, bpp, &fb_ids[i]);
        if (!framebuffers[i]) {
            fprintf(stderr, "Failed to create framebuffer %d\n", i);
            cleanup_drm_device(&device);
            return 1;
        }
    }

    cleanup_drm_device(&device);
    return 0;
}