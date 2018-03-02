// the lock server implementation

#include "lock_server.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>

lock_server::lock_server():
  nacquire (0)
{
  pthread_mutex_init(&mutex, NULL);
  pthread_cond_init(&cond, NULL);
}

lock_protocol::status
lock_server::stat(int clt, lock_protocol::lockid_t lid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
  printf("stat request from clt %d\n", clt);
  r = nacquire;
  return ret;
}

lock_protocol::status
lock_server::acquire(int clt, lock_protocol::lockid_t lid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
	// Your lab4 code goes here
  //lock the locks map;
  pthread_mutex_lock(&mutex);
  std::map<lock_protocol::lockid_t, bool>::iterator it;
  it = lock_map.find(lid);
  if(it == lock_map.end()){
    //not found and create a new lock
    lock_map.insert(std::pair<lock_protocol::lockid_t, bool>(lid, true));
  }
  else if(it->second == false){
    //grant the free lock
    it->second = true;
  }
  else{
    //acquired lock is locked
    while(it->second == true){
      //wait for releasing any lock then test again
      pthread_cond_wait(&cond, &mutex);
    }
    it->second = true;
  }
  nacquire++;
  pthread_mutex_unlock(&mutex);
  return ret;
}

lock_protocol::status
lock_server::release(int clt, lock_protocol::lockid_t lid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
	// Your lab4 code goes here
  pthread_mutex_lock(&mutex);
  std::map<lock_protocol::lockid_t, bool>::iterator it;
  it = lock_map.find(lid);
  if(it == lock_map.end()){
    //not found
    ret = lock_protocol::NOENT;
  }
  else if(it->second == false){
    //target lock has been released
    ret = lock_protocol::RETRY;
  }
  else{
    it->second = false;
    nacquire++;
  }
  pthread_mutex_unlock(&mutex);
  //send a singal to all the waiting threads
  pthread_cond_signal(&cond);
  return ret;
}
