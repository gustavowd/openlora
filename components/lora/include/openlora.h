
/*
    <OpenLoRa is a protocol stack aimed at long-range, low-power radio-based communication networks.>
    Copyright (C) 2024  Gustavo Weber Denardin

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#define NUMBER_OF_NET_IF_DESCRIPTORS        16
#define MIN_NUMBER_OF_NET_IF_DESCRIPTORS    (NUMBER_OF_NET_IF_DESCRIPTORS / 2)
#define MAX_NET_IF_DESCRIPTORS_WAIT_TIME_MS 100
#define RELEASE_NET_IF_BUFFER_TIMEOUT       10

#define OL_BORDER_ROUTER_ADDR               0
#define OL_BROADCAST_ADDR                   0xFF

#define LINK_ACK_TIMEOUT                    300
#define LINK_RETRIES                        3

/* The maximum number of retries for the example code. */
#define BACKOFF_RETRY_MAX_ATTEMPTS      LINK_RETRIES + 1
/* The base back-off delay (in milliseconds) for retry configuration. */
#define RETRY_BACKOFF_BASE_MS           ( ((256U*10U*8U)/125U) )
/* The maximum back-off delay (in milliseconds) for between retries. */
#define RETRY_MAX_BACKOFF_DELAY_MS      ( RETRY_BACKOFF_BASE_MS*4U )

#define OL_TRANSPORT_CLIENT_PORT_INIT         0x80
#define OL_TRANSPORT_MAX_PAYLOAD_SIZE         216 // excludes the protocol headers and trailers
//#define OL_APP_MAX_PAYLOAD_SIZE               216-sizeof(transport_layer_header_t) // excludes the protocol headers and trailers

typedef struct openlora_t_ {
    uint8_t  nwk_id;
    uint8_t  node_addr;
    uint8_t  mac_seq_number;
    uint8_t  pad;
    uint8_t  neigh_seq_number[256];
    uint32_t backoff_base_ms;
    uint32_t max_backoof_delay_ms;
    uint32_t symbol_time;
}openlora_t;


typedef struct
{
	uint8_t         *puc_link_buffer; 	    /* Pointer to the start of the link frame. */
    QueueHandle_t   packet_ack;
	uint8_t         data_length; 			/* Holds the total frame length */
    uint8_t         dst_addr;
    uint8_t         pad[2];
} net_if_buffer_descriptor_t;

typedef enum __attribute__((packed)) {
    DATA_FRAME = 1,
    //RTS_FRAME,
    //CTS_FRAME,
    ACK_FRAME
}link_frame_type_t;

typedef struct __attribute__((packed)) {
    link_frame_type_t   frame_type;
    uint8_t             network_id;
    uint8_t             seq_number;
    uint8_t             dst_addr;
    uint8_t             src_addr;
    uint8_t             payload_size;
}link_layer_header_t;

typedef struct __attribute__((packed)) {
    uint16_t            crc;
}link_layer_trailer_t;

typedef enum __attribute__((packed)) {
    NEIGHBOR_REQ = 1,
    NEIGHBOR_ADV,
    ROUTER_ADV,
    DATA_PACKET,
}network_packet_type_t;

typedef struct __attribute__((packed)) {
    network_packet_type_t   packet_type;
    uint8_t                 hops;
    uint16_t                dst_addr;
    uint16_t                src_addr;
    uint8_t                 payload_size;
}network_layer_header_t;

struct transp_layer_s{
    uint8_t                 dst_port;
    uint8_t                 src_port;
    uint8_t                 dst_addr;
    uint8_t                 src_addr;
    QueueHandle_t           transp_wakeup;
    QueueHandle_t           transp_ack;
    uint8_t                 sender_port;
    uint8_t                 sender_addr;
    uint8_t                 payload_size;
    uint8_t                 protocol;
    struct transp_layer_s   *next;
    struct transp_layer_s   *previous;
};

typedef struct transp_layer_s transport_layer_t;

typedef enum __attribute__((packed)) {
    TRANSP_DATAGRAM = 1,
    TRANSP_STREAM
}transp_protocol_type_t;

#define FIRST_SEQ   0x80
#define LAST_SEQ    0x40

typedef struct __attribute__((packed, aligned(1))) {
    uint8_t                 dst_port;
    uint8_t                 src_port;
    transp_protocol_type_t  protocol;
    uint8_t                 seq;
    uint8_t                 payload_size;
}transport_layer_header_t;

#define FTP_PORT             21
#define FTP_BUFFER_SIZE      1024

typedef enum __attribute__((packed)) {
    FILE_START_PACKET = 1,
    FILE_START_COMPRESS_PACKET,
    FILE_DATA_PACKET,
    FILE_END_PACKET,
    FILE_END_COMPRESS_PACKET
}file_data_packet_type_t;

typedef struct __attribute__((packed, aligned(1))) {
    file_data_packet_type_t app_packet_type;
    uint16_t                seq_number;
    uint16_t                payload_size;
}app_file_data_layer_header_t;

#define FTP_MAX_PAYLOAD_SIZE FTP_BUFFER_SIZE-sizeof(app_file_data_layer_header_t)

typedef struct {
    transport_layer_t   transp_handler;
}file_server_client_t;

// Init openLoRa network
BaseType_t ol_init(uint8_t nwk_id, uint8_t addr);

// Transport layer functions
int ol_transp_open(transport_layer_t *client_server);
int ol_transp_close(transport_layer_t *server_client);
int ol_transp_recv(transport_layer_t *server_client, uint8_t *buffer, TickType_t timeout);
int ol_transp_send(transport_layer_t *server_client, const uint8_t *buffer, uint16_t length, TickType_t timeout);

file_server_client_t *ol_create_file_client(void);
file_server_client_t *ol_create_file_server(void);
uint32_t ol_send_file_buffer(file_server_client_t *server_client, uint8_t dst_addr, char *filename, uint8_t *file, uint32_t file_size, bool compress, uint32_t segment_timeout);
uint32_t ol_receive_file_buffer(file_server_client_t *server_client, char *filename, uint8_t *file, uint32_t *file_size, uint32_t segment_timeout);
uint32_t ol_send_file(file_server_client_t *server_client, uint8_t dst_addr, char *sd_path, char *filename, uint32_t segment_timeout);
uint32_t ol_receive_file(file_server_client_t *server_client, char *sd_path, char *filename, uint32_t *file_size, uint32_t segment_timeout);
