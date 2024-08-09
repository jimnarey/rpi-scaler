#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
// #include <fcntl-linux.h>


int main()
{
    const char *device_paths[] = {"/dev/dri/card0", "/dev/dri/card1"};
    int fd = -1;
    drmModeRes *res = NULL;

    for (int i = 0; i < 2; i++)
    {
        fd = open(device_paths[i], O_RDWR | O_CLOEXEC);
        if (fd < 0)
        {
            perror("Cannot open device");
            continue;
        }

        res = drmModeGetResources(fd);
        if (res)
        {
            printf("Using device: %s\n", device_paths[i]);
            break;
        }
        else
        {
            perror("Cannot get DRM resources");
            close(fd);
            fd = -1;
        }
    }

    if (fd < 0 || !res)
    {
        fprintf(stderr, "Failed to find a valid DRM device\n");
        return 1;
    }

    int connector_id = -1;
    drmModeConnector *connector = NULL;

    for (int i = 0; i < res->count_connectors; i++)
    {
        connector = drmModeGetConnector(fd, res->connectors[i]);
        if (connector)
        {
            if (connector->connection == DRM_MODE_CONNECTED)
            {
                connector_id = connector->connector_id;
                break;
            }
            drmModeFreeConnector(connector);
        }
    }

    printf("Found %d connectors\n", res->count_connectors);
    printf("Connected monitor found: connector ID %d\n", connector_id);

    if (connector_id == -1)
    {
        fprintf(stderr, "No connected monitor found\n");
        drmModeFreeResources(res);
        close(fd);
        return 1;
    }

    int crtc_id = -1;
    drmModeEncoder *encoder = NULL;
    drmModeCrtc *crtc = NULL;

    for (int i = 0; i < res->count_encoders; i++)
    {
        encoder = drmModeGetEncoder(fd, res->encoders[i]);
        if (encoder)
        {
            if (encoder->encoder_id == connector->encoder_id)
            {
                crtc_id = encoder->crtc_id;
                break;
            }
            drmModeFreeEncoder(encoder);
        }
    }

    if (crtc_id == -1)
    {
        fprintf(stderr, "Failed to find CRTC for the connector\n");
        drmModeFreeConnector(connector);
        drmModeFreeResources(res);
        close(fd);
        return 1;
    }

    crtc = drmModeGetCrtc(fd, crtc_id);
    if (!crtc)
    {
        fprintf(stderr, "Failed to get CRTC information\n");
        drmModeFreeConnector(connector);
        drmModeFreeResources(res);
        close(fd);
        return 1;
    }

    // Use the CRTC information as needed

    drmModeFreeCrtc(crtc);
    drmModeFreeConnector(connector);
    drmModeFreeResources(res);
    close(fd);
    return 0;
}
