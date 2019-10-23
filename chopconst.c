#include <stdio.h>
#include <unistd.h>

#include "chopconst.h"
#include "chopdebug.h"

int init_pipe_struct(struct pipe_t **target) {
  // check valid argument
  if (target == NULL) {
    return 1;
  }

  // allocate structure
  struct pipe_t *new = malloc(sizeof(struct pipe_t));
  if (new == NULL) {
    DEBUG_PRINT("malloc");
    return 1;
  }

  // create pipe
  int pipefd[2];
  if (pipe(pipefd) < 0) {
    DEBUG_PRINT("pipe");
    return 1;
  }

  // initialize structure fields
  new->read = pipefd[0];
  new->write = pipefd[1];

  // copy finished structure into given location
  *target = new;

  return 0;
}

int init_buffer_struct(struct buffer **target, const int size) {
  // check valid argument
  if (target == NULL || size < 0) {
    return 1;
  }

  // allocate structure
  struct buffer *new = malloc(sizeof(struct buffer));
  if (new == NULL) {
    DEBUG_PRINT("malloc, structure");
    return 1;
  }

  // allocate buffer memory
  char *mem = malloc(sizeof(char) * size);
  if (mem == NULL) {
    DEBUG_PRINT("malloc, memory");
    return 1;
  }

  // initialize structure fields
  new->buf = mem;
  new->inbuf = 0;
  new->bufsize = size;

  // copy finished structure into given location
  *target = new;

  return 0;
}

int init_packet_struct(struct packet **target) {
  // check valid argument
  if (target == NULL) {
    return 1;
  }

  // allocate structure
  struct packet *new = malloc(sizeof(struct packet));
  if (new == NULL) {
    DEBUG_PRINT("malloc");
    return 1;
  }

  // initialize structure fields
  new->head = -1;
  new->status = -1;
  new->control1 = -1;
  new->control2 = -1;

  new->data = NULL;
  new->datalen = 0;

  // copy finished structure into given location
  *target = new;

  return 0;
}

int init_server_struct(struct server **target, const int max_conns) {
  // check valid argument
  if (target == NULL) {
    return 1;
  }

  // allocate structure
  struct server *new = malloc(sizeof(struct server));
  if (new == NULL) {
    DEBUG_PRINT("malloc, structure");
    return 1;
  }

  // allocate server client array
  struct client **mem = malloc(sizeof(struct client *) * max_conns);
  if (mem == NULL) {
    DEBUG_PRINT("malloc, memory");
    return 1;
  }

  // initialize structure fields
  new->server_fd = -1;
  new->clients = mem;
  new->max_connections = max_conns;

  // copy finished structure into given location
  *target = new;

  return 0;
}

int init_client_struct(struct client **target, const int size) {
  // check valid argument
  if (target == NULL) {
    return 1;
  }

  // allocate structure
  struct client *new = malloc(sizeof(struct client));
  if (new == NULL) {
    DEBUG_PRINT("malloc, structure");
    return 1;
  }

  // allocate buffer memory
  char *mem = malloc(sizeof(char) * size);
  if (mem == NULL) {
    DEBUG_PRINT("malloc, memory");
    return 1;
  }

  // initialize structure fields
  new->socket_fd = -1;
  new->server_fd = -1;
  new->inc_flag = -1;
  new->out_flag = -1;

  new->buf.buf = mem;
  new->buf.inbuf = 0;
  new->buf.bufsize = size;

  // copy finished structure into given location
  *target = new;

  return 0;
}

int destroy_pipe_struct(struct pipe_t **target) {
  // check valid argument
  if (target == NULL) {
    return 1;
  }

  // direct reference to structure
  struct pipe_t *old = *target;

  // close pipe ends, no error checking in case it was already closed
  close(old->read);
  close(old->write);

  // deallocate structure
  free(old);

  // dereference holder
  *target = NULL;

  return 0;
}

int destroy_buffer_struct(struct buffer **target) {
  // check valid argument
  if (target == NULL) {
    return 1;
  }

  // direct reference to structure
  struct buffer *old = *target;

  // deallocate data section
  free(old->buf);

  // deallocate structure
  free(old);

  // dereference holder
  *target = NULL;

  return 0;
}

int destroy_packet_struct(struct packet **target) {
  // check valid argument
  if (target == NULL) {
    return 1;
  }

  // direct reference to structure
  struct packet *old = *target;

  // deallocate data section
  free(old->data);

  // deallocate structure
  free(old);

  // dereference holder
  *target = NULL;

  return 0;
}

int destroy_server_struct(struct server **target) {
  // check valid argument
  if (target == NULL) {
    return 1;
  }

  // direct reference to structure
  struct server *old = *target;

  // deallocate remaining clients
  for (int i = 0; i < old->max_connections; i++) {
    if (old->clients + i != NULL) {
      destroy_client_struct(old->clients + i); // TODO: make sure the array of client pointers can be iterated along as such
      //destroy_client_struct(&(old->clients[i])); // alternative option
    }
  }

  // deallocate clients section
  free(old->clients);

  // deallocate structure
  free(old);

  // dereference holder
  *target = NULL;

  return 0;
}

int destroy_client_struct(struct client **target) {
  // check valid argument
  if (target == NULL) {
    return 1;
  }

  // direct reference to structure
  struct client *old = *target;

  // deallocate data section
  free(old->buf.buf);

  // deallocate structure
  free(old);

  // dereference holder
  *target = NULL;

  return 0;
}
