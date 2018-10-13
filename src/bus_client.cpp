#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <systemd/sd-bus.h>

#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/interprocess/mapped_region.hpp>

namespace ipc = boost::interprocess;

ipc::shared_memory_object shmem_obj;
ipc::mapped_region mapped_obj;
ipc::permissions allow_all;
int* shobj;

class SdbusShmemMappable
{
public:
    ipc::mapping_handle_t get_mapping_handle() const
    {
    }
    ipc::file_handle_t handle;
    bool is_shm;

};

int main(int argc, char *argv[]) {
        sd_bus_error error = SD_BUS_ERROR_NULL;
        sd_bus_message *m = NULL;
        sd_bus *bus = NULL;
        int r;

        allow_all.set_unrestricted();

        /* Connect to the system bus */
        r = sd_bus_open_user(&bus);
        if (r < 0) {
                fprintf(stderr, "Failed to connect to user bus: %s\n", strerror(-r));
                goto finish;
        }


        fprintf(stderr, "Can send file handles: %i\n", sd_bus_can_send(bus, 'h'));

        /* Issue the method call and store the respons message in m */
        r = sd_bus_call_method(bus,
                               "org.sdbus_shmem.sdbus_shmem",   /* service to contact */
                               "/org/sdbus_shmem/sdbus_shmem",   /* object path */
                               "org.sdbus_shmem.sdbus_shmem",   /* interface name */
                               "GetFile",                    /* method name */
                               &error,                        /* object to return error in */
                               &m,
                               "");                    /* second argument */
        if (r < 0) {
                fprintf(stderr, "Failed to issue method call: %s\n", error.message);
                goto finish;
        }

        /* Parse the response message */
        int fd;
        r = sd_bus_message_read(m, "h", &fd);
        if (r < 0) {
                fprintf(stderr, "Failed to parse response message: %s\n", strerror(-r));
                goto finish;
        }

        {
            SdbusShmemMappable mappable({fd, true});

            try
            {
                mapped_obj = ipc::mapped_region(
                        mappable,
                        ipc::read_write
                        );
            } catch (ipc::interprocess_exception& e)
            {
                fprintf(stderr, "Cannot map shmem: %s\n", e.what());
                goto finish;
            }
        }

finish:
        sd_bus_error_free(&error);
        sd_bus_message_unref(m);
        sd_bus_unref(bus);

        return r < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
