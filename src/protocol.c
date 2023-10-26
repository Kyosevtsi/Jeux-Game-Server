#include "protocol.h"
#include "debug.h"
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

int proto_send_packet(int fd, JEUX_PACKET_HEADER *hdr, void *data) {
    size_t hdr_size = sizeof(JEUX_PACKET_HEADER);
    size_t header_to_write = hdr_size;
    int header_write_bytes = 0;
    int total_header_written = 0;
    char* hdr_ptr = (char*)hdr;
    debug("write header loop started");
    while (header_to_write > 0) {  // old: total_header_written < hdr_size
        header_write_bytes = write(fd, hdr_ptr, header_to_write);
        debug("%i bytes written from header", header_write_bytes);
        if (header_write_bytes == 0) {
            return -1;
        }
        total_header_written += header_write_bytes;
        hdr_ptr += header_write_bytes;
        header_to_write -= header_write_bytes;
        debug("%i bytes total from header, %lu left", total_header_written, header_to_write);
    }
    debug("write header loop ended");
    debug("data: %s", (char*)data);
    if (data) {  // if a payload is being passed
        int payload_size = ntohs(hdr->size);
        int payload_to_write = payload_size;
        int payload_write_bytes = 0;
        int total_payload_write = 0;
        char* data_ptr = (char*)data;
        debug("write data loop started");
        while (total_payload_write < payload_size) {
            payload_write_bytes = write(fd, data_ptr, payload_to_write);
            debug("%i bytes written from payload", payload_write_bytes);
            if (payload_write_bytes == 0) {
                return -1;
            }
            total_payload_write += payload_write_bytes;
            data_ptr += payload_write_bytes;
            payload_to_write -= payload_write_bytes;
            debug("%i bytes total from payload, %i left", total_payload_write, payload_to_write);
        }
        debug("write data loop ended");
    }
    return 0;
}

int proto_recv_packet(int fd, JEUX_PACKET_HEADER *hdr, void **payloadp) {
    // read the header and allocate memory for the payload if it exists
    size_t hdr_size = sizeof(JEUX_PACKET_HEADER); // need to keep reading until hdr_size bytes are read
    size_t bytes_to_read = hdr_size;
    int read_bytes = 0;
    int total_read = 0;
    char* hdr_ptr = (char*)hdr;
    debug("read header loop started");
    while (bytes_to_read > 0) {  // old: total_read < hdr_size
        read_bytes = read(fd, hdr_ptr, bytes_to_read);
        debug("%i bytes read from header", read_bytes);
        if (read_bytes <= 0) {
            // errno set by read
            return -1;  // @return 0 in case of successful reception, -1 otherwise. In the latter case, errno is set to indicate the error.
        }
        total_read += read_bytes;
        hdr_ptr += read_bytes;
        bytes_to_read -= read_bytes;
        debug("%i bytes total from header, %lu left", total_read, bytes_to_read);
    }
    debug("read header loop ended");
    if (ntohs(hdr->size) > 0) {  // payload exists
        int payload_size = ntohs(hdr->size);
        debug("payload size = %i", payload_size);
        int payload_to_read = payload_size;
        int payload_read_bytes = 0;
        debug("check");
        int total_payload_read = 0;
        debug("mallocing");
        char* pp = (char*)malloc(payload_size);
        *payloadp = (void*)pp;
        debug("read payload loop started");
        while (payload_to_read > 0) {  // old: total_payload_read < payload_size
            payload_read_bytes = read(fd, pp, payload_to_read);
            debug("%i bytes read from payload", payload_read_bytes);
            if (payload_read_bytes <= 0) {
                return -1;
            }
            total_payload_read += payload_read_bytes;
            pp += payload_read_bytes;
            payload_to_read -= payload_read_bytes;
            debug("%i bytes total from payload, %i left", total_payload_read, payload_to_read);
        }
        debug("read payload loop ended");
    }
    return 0;
}
