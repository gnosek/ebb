#ifndef error_callback_h
#define error_callback_h

#define ERROR_CB_WARNING 0 /* just a warning, tell the user */
#define ERROR_CB_ERROR   1 /* an error, the operation cannot complete */
#define ERROR_CB_FATAL   2 /* an error, the operation must be aborted */
typedef void (*error_cb_t) (int severity, char *message);

#endif error_callback_h