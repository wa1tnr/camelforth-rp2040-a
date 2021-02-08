#define stack_buffer_length 64

#if CONF_USBD_HS_SP
#define CDCD_ECHO_BUF_SIZCF CONF_USB_CDCD_ACM_DATA_BULKIN_MAXPKSZ_HS
#else
// #define CDCD_ECHO_BUF_SIZCF CONF_USB_CDCD_ACM_DATA_BULKIN_MAXPKSZ
#define TEST_CBC77 1
#endif

char print_string[stack_buffer_length];
// extern char usbd_cdc_in_buffer[CDCD_ECHO_BUF_SIZCF / 4];
