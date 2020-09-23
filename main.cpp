#include"server.hpp"
#include"client.hpp"

void srv_start()
{
 MyServer server;
  server.ServerStart(9000);
}

int main()
{
  std::thread thr(srv_start);
  thr.detach();

  MyClient cli(9000);
  cli.ClientStart();
  
  return 0;
}

