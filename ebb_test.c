#include "ebb.h"
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

void request_cb(ebb_request *request, void  *data)
{
  ebb_env_pair *pair;
  
  //g_message("Request");
  
  while((pair = g_queue_pop_head(request->env))) {
    tcp_client_write(request->client, pair->field, pair->flen);
    tcp_client_write(request->client, "\r\n", 2);
    tcp_client_write(request->client, pair->value, pair->vlen);
    tcp_client_write(request->client, "\r\n\r\n", 4);
    
    ebb_env_pair_free(pair);
  }
  tcp_client_write(request->client, "Hello.\r\n\r\n", 6);
  tcp_client_close(request->client);
}

int main(void)
{
  ebb_server *server;
  server = ebb_server_new();
  
  g_log_set_handler( TCP_LOG_DOMAIN
                   , G_LOG_LEVEL_MASK | G_LOG_FLAG_FATAL| G_LOG_FLAG_RECURSION
                   , unit_test_error
                   , NULL);
  g_log_set_handler( EBB_LOG_DOMAIN
                   , G_LOG_LEVEL_MASK | G_LOG_FLAG_FATAL| G_LOG_FLAG_RECURSION
                   , unit_test_error
                   , NULL);
  
  fprintf(stdout, "Starting server at 0.0.0.0 31337\n");
  
  ebb_server_start(server, "localhost", 31337, request_cb, NULL);
  
  
  return 0; // success
}
