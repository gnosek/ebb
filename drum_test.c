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
  drum_env_pair *pair;
  
  //g_message("Request");
  
  while((pair = g_queue_pop_head(request->env))) {
    
    tcp_client_write(request->client, pair->field, pair->flen);
    tcp_client_write(request->client, "\r\n", 2);
    tcp_client_write(request->client, pair->value, pair->vlen);
    tcp_client_write(request->client, "\r\n\r\n", 4);
    
    drum_env_pair_free(pair);
  }
  
  tcp_client_write(request->client, "Hello.\r\n\r\n", 6);
  tcp_client_close(request->client);
}

int main(void)
{
  drum_server *server;
  server = drum_server_new();
  
  g_log_set_handler( TCP_LOG_DOMAIN
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
