#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <systemd/sd-bus.h>

#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/interprocess/mapped_region.hpp>

namespace ipc = boost::interprocess;

int fd;
ipc::shared_memory_object shmem_obj;
ipc::mapped_region mapped_obj;
ipc::permissions allow_all;
int* shobj;

static int method_getfile(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
        return sd_bus_reply_method_return(m, "h", fd);
}

static const sd_bus_vtable calculator_vtable[] = {
        SD_BUS_VTABLE_START(0),
        SD_BUS_METHOD("GetFile",  "", "h", method_getfile,  SD_BUS_VTABLE_UNPRIVILEGED),
        SD_BUS_VTABLE_END
};

int main(int argc, char *argv[]) {
        sd_bus_slot *slot = NULL;
        sd_bus *bus = NULL;
        int r;

        allow_all.set_unrestricted();

        try {
            shmem_obj = ipc::shared_memory_object(
                    ipc::open_or_create
                  , "MyShmem"
                  , ipc::read_write
                  , allow_all
                  );
        } catch (ipc::interprocess_exception&)
        {
            fprintf(stderr, "Cannot open shmem\n");
            goto finish;
        }

        shmem_obj.truncate(sizeof(int));

        try
        {
            mapped_obj = ipc::mapped_region(
                    shmem_obj,
                    ipc::read_write
                    );
        } catch (ipc::interprocess_exception&)
        {
            fprintf(stderr, "Cannot map shmem\n");
            goto finish;
        }

        shobj = static_cast<int*>(mapped_obj.get_address());
        *shobj = 157;
        fd = shmem_obj.get_mapping_handle().handle;

        /* Connect to the user bus */
        r = sd_bus_open_user(&bus);
        if (r < 0) {
                fprintf(stderr, "Failed to connect to system bus: %s\n", strerror(-r));
                goto finish;
        }

        fprintf(stderr, "Can send file handles: %i\n", sd_bus_can_send(bus, 'h'));

        /* Install the object */
        r = sd_bus_add_object_vtable(bus,
                                     &slot,
                                     "/org/sdbus_shmem/sdbus_shmem",
                                     "org.sdbus_shmem.sdbus_shmem",
                                     calculator_vtable,
                                     NULL);
        if (r < 0) {
                fprintf(stderr, "Failed to issue method call: %s\n", strerror(-r));
                goto finish;
        }

        /* Take a well-known service name so that clients can find us */
        r = sd_bus_request_name(bus, "org.sdbus_shmem.sdbus_shmem", 0);
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
