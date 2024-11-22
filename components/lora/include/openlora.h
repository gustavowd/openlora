
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

typedef struct __attribute__((packed, aligned(1))) {
    uint8_t                 dst_port;
    uint8_t                 src_port;
    transp_protocol_type_t  protocol;
    uint8_t                 payload_size;
}transport_layer_header_t;

BaseType_t ol_init(uint8_t nwk_id, uint8_t addr);

net_if_buffer_descriptor_t *ol_get_net_if_buffer(uint8_t size, uint32_t timeout);
BaseType_t ol_release_net_if_buffer(net_if_buffer_descriptor_t *buffer);
BaseType_t ol_get_number_of_free_net_if_buffer(void);

BaseType_t ol_to_link_layer(net_if_buffer_descriptor_t *buffer, TickType_t timeout);
BaseType_t ol_from_link_layer(net_if_buffer_descriptor_t **buffer, TickType_t timeout);

BaseType_t ol_to_transport_layer(net_if_buffer_descriptor_t *buffer, TickType_t timeout);
BaseType_t ol_from_transport_layer(net_if_buffer_descriptor_t **buffer, TickType_t timeout);
int ol_transp_open(transport_layer_t *client_server);
int ol_transp_close(transport_layer_t *server_client);
int ol_transp_recv(transport_layer_t *server_client, uint8_t *buffer, TickType_t timeout);
int ol_transp_send(transport_layer_t *server_client, const uint8_t *buffer, uint16_t length, TickType_t timeout);
