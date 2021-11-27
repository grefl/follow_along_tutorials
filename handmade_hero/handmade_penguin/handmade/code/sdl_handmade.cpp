#include <SDL.h>
#include <stdlib.h>

//------------DEFINES------------

#define internal static
#define local_persist static
#define global_variable static


//------------GLOBALS------------

global_variable SDL_Texture *texture;
global_variable void *bit_map_memory;
global_variable int bit_map_width;
global_variable int bytes_per_pixel = 4;


//------------HELPERS------------

void scc(int code) {

  if (code < 0) {
    fprintf(stderr, "SDL ERROR: %s\n", SDL_GetError());
    exit(1);
  }
}

void *scp(void *ptr) {
  if (ptr == NULL ) {
    fprintf(stderr, "SDL ERROR: %s\n", SDL_GetError());
    exit(1);
  }
  return ptr;
}

//------------TEXTURE------------

internal void
resize_texture(SDL_Renderer *renderer, int width, int height) {
  if (bit_map_memory) {
    free(bit_map_memory);
  }
  if (texture) {
    SDL_DestroyTexture(texture);
  }

  SDL_Texture *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, width, height);
  bit_map_width = width;
  void *bit_map_memory = malloc(width * height * bytes_per_pixel);
}
internal void
update_window(SDL_Window *window, SDL_Renderer *renderer) {
  SDL_UpdateTexture(texture,
      0,
      bit_map_memory,
      bit_map_width * bytes_per_pixel);

  SDL_RenderCopy(renderer,
      texture,
      0,
      0);
  SDL_RenderPresent(renderer);
}
//------------INPUT------------

bool handle_event(SDL_Event *event) {

  bool should_quit = false;

  switch(event->type) {
    case SDL_QUIT:
      {
        printf("SDL_QUIT\n");
        should_quit = true;
      }
      break;
    case SDL_WINDOWEVENT:
      {
        switch(event->window.event) {
          case SDL_WINDOWEVENT_SIZE_CHANGED: 
            {
              SDL_Window *window = SDL_GetWindowFromID(event->window.windowID);
              SDL_Renderer *renderer = SDL_GetRenderer(window);
              // DEBUG
              printf("SDL_WINDOWEVENT_RESIZED (%d, %d)\n", event->window.data1, event->window.data2);
              resize_texture(renderer, event->window.data1, event->window.data2);
            } 
            break;
          case SDL_WINDOWEVENT_FOCUS_GAINED:
            {
              printf("SDL_WINDOWEVENT_FOCUS_GAINED \n");
            }
            break;
          case SDL_WINDOWEVENT_EXPOSED:
            {
              SDL_Window *window = SDL_GetWindowFromID(event->window.windowID);
              SDL_Renderer *renderer = SDL_GetRenderer(window);
              update_window(window, renderer);

            }
            break;
        }
      }
      break;
  }
  return(should_quit);
}


//------------RUN------------
int main(int argc, char *argv[]) {
  scc(SDL_Init(SDL_INIT_VIDEO) !=0);
  SDL_Window *window;
  window = SDL_CreateWindow("Handmade Hero",
      SDL_WINDOWPOS_UNDEFINED,
      SDL_WINDOWPOS_UNDEFINED,
      640,
      480,
      SDL_WINDOW_RESIZABLE);


  if (window) {
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, 0);
    scp(&renderer);
    for (;;) {
      SDL_Event event;
      SDL_WaitEvent(&event);
      if (handle_event(&event))
        break;
    }

  }
  SDL_Quit();
  return(0);
}
