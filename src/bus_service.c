#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <systemd/sd-bus.h>

int fd;
int running = 1;

static int method_multiply(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
        int64_t x, y;
        int r;

        /* Read the parameters */
        r = sd_bus_message_read(m, "xx", &x, &y);
        if (r < 0) {
                fprintf(stderr, "Failed to parse parameters: %s\n", strerror(-r));
                return r;
        }

        /* Reply with the response */
        return sd_bus_reply_method_return(m, "x", x * y);
}

static int method_divide(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
        int64_t x, y;
        int r;

        /* Read the parameters */
        r = sd_bus_message_read(m, "xx", &x, &y);
        if (r < 0) {
                fprintf(stderr, "Failed to parse parameters: %s\n", strerror(-r));
                return r;
        }

        /* Return an error on division by zero */
        if (y == 0) {
                sd_bus_error_set_const(ret_error, "net.poettering.DivisionByZero", "Sorry, can't allow division by zero.");
                return -EINVAL;
        }

        return sd_bus_reply_method_return(m, "x", x / y);
}

static int method_getfile(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
        return sd_bus_reply_method_return(m, "h", fd);
}

static int method_exit(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
        return running = 0;
        return sd_bus_reply_method_return(m, "");
}

/* The vtable of our little object, implements the net.poettering.Calculator interface */
static const sd_bus_vtable calculator_vtable[] = {
        SD_BUS_VTABLE_START(0),
        SD_BUS_METHOD("Multiply", "xx", "x", method_multiply, SD_BUS_VTABLE_UNPRIVILEGED),
        SD_BUS_METHOD("Divide",   "xx", "x", method_divide,   SD_BUS_VTABLE_UNPRIVILEGED),
        SD_BUS_METHOD("GetFile",  "", "h", method_getfile,  SD_BUS_VTABLE_UNPRIVILEGED),
        SD_BUS_METHOD("Exit",  "", "", method_exit,  SD_BUS_VTABLE_UNPRIVILEGED),
        SD_BUS_VTABLE_END
};

int main(int argc, char *argv[]) {
        sd_bus_slot *slot = NULL;
        sd_bus *bus = NULL;
        int r;

        fd = open("MyFile.txt", O_RDWR | O_CREAT, 0644);

        if (fd < 0) {
                fprintf(stderr, "Failed to open file desc: %s\n", strerror(errno));
                goto finish;
        }

        write(fd, "hello world\n", 12);

        /* Connect to the user bus this time */
        r = sd_bus_open_user(&bus);
        if (r < 0) {
                fprintf(stderr, "Failed to connect to system bus: %s\n", strerror(-r));
                goto finish;
        }

        fprintf(stderr, "Can send file handles: %i\n", sd_bus_can_send(bus, 'h'));

        /* Install the object */
        r = sd_bus_add_object_vtable(bus,
                                     &slot,
                                     "/net/poettering/Calculator",  /* object path */
                                     "net.poettering.Calculator",   /* interface name */
                                     calculator_vtable,
                                     NULL);
        if (r < 0) {
                fprintf(stderr, "Failed to issue method call: %s\n", strerror(-r));
                goto finish;
        }

        /* Take a well-known service name so that clients can find us */
        r = sd_bus_request_name(bus, "net.poettering.Calculator", 0);
        if (r < 0) {
                fprintf(stderr, "Failed to acquire service name: %s\n", strerror(-r));
                goto finish;
        }

        while (1) {
                /* Process requests */
                r = sd_bus_process(bus, NULL);
                if (r < 0) {
                        fprintf(stderr, "Failed to process bus: %s\n", strerror(-r));
                        goto finish;
                }
                fprintf(stderr, "processed request: %i\n", r);
                if (r > 0) /* we processed a request, try to process another one, right-away */
                        continue;

                if (!running)
                {
                    fprintf(stderr, "Exit requested\n");
                    goto finish;
                }

                /* Wait for the next request to process */
                r = sd_bus_wait(bus, (uint64_t) -1);
                if (r < 0) {
                        fprintf(stderr, "Failed to wait on bus: %s\n", strerror(-r));
                        goto finish;
                }
        }

finish:
        sd_bus_slot_unref(slot);
        sd_bus_unref(bus);

        close(fd);

        return r < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
