#include <iot/iot.h>
#include "provision.h"


#define PROVISION_ADDRESS "https://gateway.iot.hotray.cn"
#define DEFAULT_DNS4_URL "udp://119.29.29.29:53"
#define HANDLER_LUA_SCRIPT "/www/iot/handler/iot-provision.lua"
#define DNS4_TIMEOUT 6
#define HTTP_TIMEOUT 30
#define SALT "cb2ba14fa0e14ca9dc14c86b8973d98a"


static void usage(const char *prog) {
    fprintf(stderr,
            "IoT-SDK v.%s\n"
            "Usage: %s OPTIONS\n"
            "  -s ADDR   - provision server address, default: '%s'\n"
            "  -n SN     - device sn, default: null\n"
            "  -u USER   - product key, default: null\n"
            "  -p PASS   - product secret, default: null\n"
            "  -d ADDR   - dns server address, default: '%s'\n"
            "  -x PATH   - provision message callback script, default: '%s'\n"
            "  -S SALT   - sign salt, default: '%s'\n"
            "  -t n      - dns server timeout, default: %d\n"
            "  -T n      - http client timeout, default: %d\n"
            "  -v LEVEL  - debug level, from 0 to 4, default: %d\n",
            MG_VERSION, prog, PROVISION_ADDRESS, DEFAULT_DNS4_URL, HANDLER_LUA_SCRIPT, SALT, DNS4_TIMEOUT, HTTP_TIMEOUT, MG_LL_INFO);

    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {

    struct provision_option opts = {
        .provision_address = PROVISION_ADDRESS,
        .sn = NULL,
        .product_key = NULL,
        .product_secret = NULL,
        .dns4_url = DEFAULT_DNS4_URL,
        .callback_lua = HANDLER_LUA_SCRIPT,
        .salt = SALT,
        .dns4_timeout = DNS4_TIMEOUT,
        .http_timeout = HTTP_TIMEOUT,
        .debug_level = MG_LL_INFO
    };

    // Parse command-line flags
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-s") == 0) {
            opts.provision_address = argv[++i];
        } if (strcmp(argv[i], "-S") == 0) {
            opts.salt = argv[++i];
        } else if (strcmp(argv[i], "-n") == 0) {
            opts.sn = argv[++i];
        } else if (strcmp(argv[i], "-u") == 0) {
            opts.product_key = argv[++i];
        } else if (strcmp(argv[i], "-p") == 0) {
            opts.product_secret = argv[++i];
        } else if (strcmp(argv[i], "-x") == 0) {
            opts.callback_lua = argv[++i];
        } else if (strcmp(argv[i], "-d") == 0) {
            opts.dns4_url = argv[++i];
        } else if (strcmp(argv[i], "-t") == 0) {
            opts.dns4_timeout = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-T") == 0) {
            opts.http_timeout = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-v") == 0) {
            opts.debug_level = atoi(argv[++i]);
        } else {
            MG_ERROR(("unknown option: %s", argv[i]));
            usage(argv[0]);
        }
    }
    if (!opts.sn || !opts.product_key || !opts.product_secret) {
        usage(argv[0]);
    }

    MG_INFO(("IoT-SDK version  : v%s", MG_VERSION));
    provision_main(&opts);

    return 0;
}