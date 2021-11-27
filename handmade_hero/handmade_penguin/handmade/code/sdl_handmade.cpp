#include <SDL.h>
#include <stdlib.h>
#include <stdint.h>

//------------DEFINES------------

#define internal static
#define local_persist static
#define global_variable static

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;


typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;


//------------GLOBALS------------

global_variable SDL_Texture *texture;
global_variable void *bitmap_memory;
global_variable int bitmap_width;
global_variable int bitmap_height;
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
render_weird_gradient(int blue_offset, int green_offset) {
  printf("render_weird_gradient\n");
  int width  = bitmap_width; 
  int height = bitmap_height;
  int pitch  = width * bytes_per_pixel;
  uint8 *row = (uint8 *)bitmap_memory;
  for (int y = 0; y < bitmap_height; ++y) {
    uint32 *pixel = (uint32 *)row;
    for (int x = 0; x < bitmap_width; ++x) {
      uint8 blue (x + blue_offset);
      uint8 green (x + green_offset);
      *pixel++ = ((green << 8) | blue);
    }
    row += pitch;
  }
}

internal void
resize_texture(SDL_Renderer *renderer, int width, int height) {
  printf("resize_texture\n");
  if (bitmap_memory) {
    free(bitmap_memory);
  }
  if (texture) {
    SDL_DestroyTexture(texture);
  }

  texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, width, height);
  bitmap_width  = width;
  bitmap_height = height;
  bitmap_memory = malloc(width * height * bytes_per_pixel);
}
internal void
update_window(SDL_Window *window, SDL_Renderer *renderer) {
  printf("update_window\n");
  SDL_UpdateTexture(texture,
      0,
      bitmap_memory,
      bitmap_width * bytes_per_pixel);

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
              printf("SDL_WINDOWEVENT_SIZE_CHANGED \n");
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
              printf("SDL_WINDOWEVENT_EXPOSED \n");
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

    int width, height;
    SDL_GetWindowSize(window, &width, &height);
    resize_texture(renderer, width, height); 

    int x_offset = 0;
    int y_offset = 0;

    bool running = true;
    while (running) {
      SDL_Event event;
      while(SDL_PollEvent(&event)) {
        printf("running\n");
        if (handle_event(&event))
          running = false;
      }
      render_weird_gradient(x_offset, y_offset);
      update_window(window, renderer);

      ++x_offset;
      y_offset +=2 ;
    }

  }
  SDL_Quit();
  return(0);
}


//------------MEMORY MANAGEMENT -> "LINUX ONLY"------------
//sys/mman.h
//pixesl = mmap(0, width* height * 4, PROT_READ | PROT_WRTE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
//munmap(pixles, width * height * 4);
