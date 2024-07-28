#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

int main() {
    const char *device_paths[] = {"/dev/dri/card0", "/dev/dri/card1"};
    int fd = -1;
    drmModeRes *res = NULL;

    for (int i = 0; i < 2; i++) {
        fd = open(device_paths[i], O_RDWR | O_CLOEXEC);
        if (fd < 0) {
            perror("Cannot open device");
            continue;
        }

        res = drmModeGetResources(fd);
        if (res) {
            printf("Using device: %s\n", device_paths[i]);
            break;
        } else {
            perror("Cannot get DRM resources");
            close(fd);
            fd = -1;
        }
    }

    if (fd < 0 || !res) {
        fprintf(stderr, "Failed to find a valid DRM device\n");
        return 1;
    }

    printf("Found %d connectors\n", res->count_connectors);

    drmModeFreeResources(res);
    close(fd);
    return 0;
}