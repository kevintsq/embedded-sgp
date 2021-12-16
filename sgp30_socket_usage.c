#include "sgp30.h"

#include <inttypes.h>  // PRIu64
#include <stdio.h>     // printf
#include <string.h>
#include <unistd.h>    // sleep
#include <sys/socket.h>
#include <arpa/inet.h>

#define MAX_BUFFER_SIZE 1024

int main(int argc, char *argv[]) {
    uint64_t i = 0;
    int16_t err;
    uint16_t tvoc_ppb, co2_eq_ppm;
    uint32_t iaq_baseline;
    uint16_t ethanol_raw_signal, h2_raw_signal;

    if (argc != 3) {
        fprintf(stderr, "usage: %s <IP_ADDRESS> <PORT>\n", argv[0]);
    }

    const char* driver_version = sgp30_get_driver_version();
    if (driver_version) {
        printf("SGP30 driver version %s\n", driver_version);
    } else {
        fprintf(stderr, "fatal: Getting driver version failed\n");
        return EXIT_FAILURE;
    }

    /* Initialize I2C bus */
    sensirion_i2c_init();

    /* Busy loop for initialization. The main loop does not work without
     * a sensor. */
    int16_t probe;
    while (1) {
        probe = sgp30_probe();

        if (probe == STATUS_OK)
            break;

        if (probe == SGP30_ERR_UNSUPPORTED_FEATURE_SET)
            fprintf(stderr,
                "Your sensor needs at least feature set version 1.0 (0x20)\n");

        fprintf(stderr, "SGP sensor probing failed\n");
        sensirion_sleep_usec(1000000);
    }

    printf("SGP sensor probing successful\n");

    uint16_t feature_set_version;
    uint8_t product_type;
    err = sgp30_get_feature_set_version(&feature_set_version, &product_type);
    if (err == STATUS_OK) {
        printf("Feature set version: %u\n", feature_set_version);
        printf("Product type: %u\n", product_type);
    } else {
        fprintf(stderr, "sgp30_get_feature_set_version failed\n");
    }
    uint64_t serial_id;
    err = sgp30_get_serial_id(&serial_id);
    if (err == STATUS_OK) {
        printf("SerialID: %" PRIu64 "\n", serial_id);
    } else {
        fprintf(stderr, "sgp30_get_serial_id failed\n");
    }

    /* Read gas raw signals */
    err = sgp30_measure_raw_blocking_read(&ethanol_raw_signal, &h2_raw_signal);
    if (err == STATUS_OK) {
        /* Print ethanol raw signal and h2 raw signal */
        printf("Ethanol raw signal: %u\n", ethanol_raw_signal);
        printf("H2 raw signal: %u\n", h2_raw_signal);
    } else {
        fprintf(stderr, "error reading raw signals\n");
    }

    /* Consider the two cases (A) and (B):
     * (A) If no baseline is available or the most recent baseline is more than
     *     one week old, it must discarded. A new baseline is found with
     *     sgp30_iaq_init() */
    err = sgp30_iaq_init();
    if (err == STATUS_OK) {
        printf("sgp30_iaq_init done\n");
    } else {
        fprintf(stderr, "sgp30_iaq_init failed!\n");
    }
    /* (B) If a recent baseline is available, set it after sgp30_iaq_init() for
     * faster start-up */
    /* IMPLEMENT: retrieve iaq_baseline from presistent storage;
     * err = sgp30_set_iaq_baseline(iaq_baseline);
     */

    int socket_fd;
    struct sockaddr_in socket_address;
    socklen_t socket_address_size = sizeof(socket_address);
    char buffer[MAX_BUFFER_SIZE] = {0};

    if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }
    
    socket_address.sin_family = AF_INET;
    socket_address.sin_addr.s_addr = inet_addr(argv[1]);
    socket_address.sin_port = htons(atoi(argv[2]));

    if (connect(socket_fd, (struct sockaddr *) &socket_address, socket_address_size) < 0) {
        perror("connect");
        exit(EXIT_FAILURE);
    }

    /* Run periodic IAQ measurements at defined intervals */
    for (i = 0; ; i++) {
        /*
         * IMPLEMENT: get absolute humidity to enable humidity compensation
         * uint32_t ah = get_absolute_humidity(); // absolute humidity in mg/m^3
         * sgp30_set_absolute_humidity(ah);
         */

        sprintf(buffer, "{\n\t\"type\": \"concentration\",\n\t\"timeStamp\": %lld", i);
        /* Read gas raw signals */
        err = sgp30_measure_raw_blocking_read(&ethanol_raw_signal, &h2_raw_signal);
        if (err == STATUS_OK) {
            sprintf(buffer, "%s,\n\t\"Ethanol\": %u", buffer, ethanol_raw_signal);
            sprintf(buffer, "%s,\n\t\"H2\": %u", buffer, h2_raw_signal);
        } else {
            fprintf(stderr, "error reading raw signals\n");
        }

        err = sgp30_measure_iaq_blocking_read(&tvoc_ppb, &co2_eq_ppm);
        if (err == STATUS_OK) {
            sprintf(buffer, "%s,\n\t\"tVOC\": %u", buffer, tvoc_ppb);
            sprintf(buffer, "%s,\n\t\"CO2\": %u", buffer, co2_eq_ppm);
        } else {
            fprintf(stderr, "error reading IAQ values\n");
        }

        sprintf(buffer, "%s\n}\r\n", buffer);
        if (send(socket_fd, buffer, strlen(buffer), 0) < 0) {
            printf("connection closed\n");
            break;
        }

        /* Persist the current baseline every hour */
        if (i % 3600 == 3599) {
            err = sgp30_get_iaq_baseline(&iaq_baseline);
            if (err == STATUS_OK) {
                /* IMPLEMENT: store baseline to presistent storage */
            }
        }

        /* The IAQ measurement must be triggered exactly once per second (SGP30)
         * to get accurate values.
         */
        sleep(1);  // SGP30
    }
    
    sensirion_i2c_release();
    return EXIT_SUCCESS;
}
