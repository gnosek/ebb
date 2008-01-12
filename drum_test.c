#include "drum.h"
#include <stdio.h>
#include <stdlib.h>

void unit_test_error( const gchar *log_domain
                    , GLogLevelFlags log_level
                    , const gchar *message
                    , gpointer user_data
                    )
{
  printf("ERROR(%d) %s\n", log_level, message);
  if(log_level < G_LOG_LEVEL_ERROR) { exit(1); }
}

void request_cb(drum_request *request, void  *data)
{
  fprintf(stdout, "hello world :P\n");
}

int main(void)
{
  drum_server *server;
  server = drum_server_new();
  
  g_log_set_handler( TCP_SERVER_LOG_DOMAIN
                   , G_LOG_LEVEL_MASK | G_LOG_FLAG_FATAL| G_LOG_FLAG_RECURSION
                   , unit_test_error
                   , NULL);
  g_log_set_handler( DRUM_LOG_DOMAIN
                   , G_LOG_LEVEL_MASK | G_LOG_FLAG_FATAL| G_LOG_FLAG_RECURSION
                   , unit_test_error
                   , NULL);
  
  fprintf(stdout, "Starting server at 0.0.0.0 31337\n");
  
  drum_server_start(server, "localhost", 31337, request_cb, NULL);
  
  
  return 0; // success
}
