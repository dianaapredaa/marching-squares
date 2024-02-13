// Author: APD team, except where source was noted

#include "helpers.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

#define CONTOUR_CONFIG_COUNT    16
#define FILENAME_MAX_SIZE       50
#define STEP                    8
#define SIGMA                   200
#define RESCALE_X               2048
#define RESCALE_Y               2048

#define MIN(a, b) ((a) < (b) ? (a) : (b))

typedef struct {
    ppm_image *image;
    ppm_image *final_img;
    ppm_image **contour_map;
    pthread_barrier_t* barrier;
    int step;
    unsigned char **grid;
    int id;
    int num_of_threads;
} thread_data;


// Creates a map between the binary configuration (e.g. 0110_2) and the corresponding pixels
// that need to be set on the output image. An array is used for this map since the keys are
// binary numbers in 0-15. Contour images are located in the './contours' directory.
ppm_image **init_contour_map() {
    ppm_image **map = (ppm_image **)malloc(CONTOUR_CONFIG_COUNT * sizeof(ppm_image *));
    if (!map) {
        fprintf(stderr, "Unable to allocate memory\n");
        exit(1);
    }

    for (int i = 0; i < CONTOUR_CONFIG_COUNT; i++) {
        char filename[FILENAME_MAX_SIZE];
        sprintf(filename, "./contours/%d.ppm", i);
        map[i] = read_ppm(filename);
    }

    return map;
}

// Updates a particular section of an image with the corresponding contour pixels.
// Used to create the complete contour image.
void update_image(ppm_image *image, ppm_image *contour, int x, int y) {
    for (int i = 0; i < contour->x; i++) {
        for (int j = 0; j < contour->y; j++) {
            int contour_pixel_index = contour->x * i + j;
            int image_pixel_index = (x + i) * image->y + y + j;

            image->data[image_pixel_index].red = contour->data[contour_pixel_index].red;
            image->data[image_pixel_index].green = contour->data[contour_pixel_index].green;
            image->data[image_pixel_index].blue = contour->data[contour_pixel_index].blue;
        }
    }
}

void rescale_image(thread_data* data) { 
    // Calculate the start and end of the current thread area
    int start = data->id * data->final_img->x / data->num_of_threads;
    int end = MIN(data->final_img->x, (data->id + 1) * data->final_img->x / data->num_of_threads);

    if (data->image->x <= RESCALE_X && data->image->y <= RESCALE_Y) {
        // If the image is smaller than the final image, just copy it
        for (int i = start; i < end; i++) {
            for (int j = 0; j < data->image->y; j++) {
                data->final_img->data[i * data->final_img->y + j].red = data->image->data[i * data->image->y + j].red;
                data->final_img->data[i * data->final_img->y + j].green = data->image->data[i * data->image->y + j].green;
                data->final_img->data[i * data->final_img->y + j].blue = data->image->data[i * data->image->y + j].blue;
            }
        }
        return;
    }

    uint8_t sample[3];

    // Use bicubic interpolation for scaling
    for (int i = start; i < end; i++) {
        for (int j = 0; j < data->final_img->y; j++) {
            // Calculate the coordinates
            float u = (float)i / (float)(data->final_img->x - 1);
            float v = (float)j / (float)(data->final_img->y - 1);
            sample_bicubic(data->image, u, v, sample);

            // Set the pixels
            data->final_img->data[i * data->final_img->y + j].red = sample[0];
            data->final_img->data[i * data->final_img->y + j].green = sample[1];
            data->final_img->data[i * data->final_img->y + j].blue = sample[2];
        }
    }
}

void sample_grid(thread_data* data) {
    ppm_image* image = data->final_img;
    int step_x = STEP;
    int step_y = STEP;

    int p = image->x / step_x;
    int q = image->y / step_y;

    int start = data->id * p / data->num_of_threads;
    int end = MIN(p, (data->id + 1) * p / data->num_of_threads);
    
    // Sample the grid
    for (int i = start; i < end; i++) {
        for (int j = 0; j < q; j++) {
            // Calculate the average color of the current square
            ppm_pixel curr_pixel = image->data[i * step_x * image->y + j * step_y];

            unsigned char curr_color = (curr_pixel.red + curr_pixel.green + curr_pixel.blue) / 3;

            data->grid[i][j] = (curr_color > SIGMA) ? 0 : 1;
        }
    }

    // Update the first element
    data->grid[start][q] = 0;

    // Update the last column
    for (int i = start; i < end; i++) {
        // Calculate the average color of the current square
        ppm_pixel curr_pixel = image->data[i * step_x * image->y + image->x - 1];

        unsigned char curr_color = (curr_pixel.red + curr_pixel.green + curr_pixel.blue) / 3;

        data->grid[i][q] = (curr_color > SIGMA) ? 0 : 1;
    }

    // Update the last row
    for (int j = 0; j < q; j++) {
        // Calculate the average color of the current square
        ppm_pixel curr_pixel = image->data[(image->x - 1) * image->y + j * step_y];

        unsigned char curr_color = (curr_pixel.red + curr_pixel.green + curr_pixel.blue) / 3;

        data->grid[p][j] = (curr_color > SIGMA) ? 0 : 1;
    }
}

void march(thread_data* data) {
    ppm_image* image = data->final_img;
    unsigned char **grid = data->grid;
    ppm_image **contour_map = data->contour_map;
    
    int step_x = STEP;
    int step_y = STEP;

    int p = image->x / step_x;
    int q = image->y / step_y;

    int start = data->id * p / data->num_of_threads;
    int end = MIN(p, (data->id + 1) * p / data->num_of_threads);

    // March the squares
    for (int i = start; i < end; i++) {
        for (int j = 0; j < q; j++) {
            unsigned char k;
            k = (grid[i][j] << 3) + (grid[i][j + 1] << 2) + (grid[i + 1][j + 1] << 1) + grid[i + 1][j];
            update_image(data->final_img, contour_map[k], i * step_x, j * step_y);
        }
    }
}

void* main_thread_function(thread_data* data) {
    // 1. Rescale the image
    rescale_image(data);

    // Wait for threads
    pthread_barrier_wait(data->barrier);

    // 2. Sample the grid
    sample_grid(data);

    // Wait for threads
    pthread_barrier_wait(data->barrier);

    // 3. March the squares
    march(data);

    // Wait for threads
    pthread_barrier_wait(data->barrier);

    return NULL;
}

// Calls `free` method on the utilized resources.
void free_resources(ppm_image *image, ppm_image **contour_map, unsigned char **grid, int step_x) {
    for (int i = 0; i < CONTOUR_CONFIG_COUNT; i++) {
        free(contour_map[i]->data);
        free(contour_map[i]);
    }

    free(contour_map);

    for (int i = 0; i <= image->x / step_x; i++) {
        free(grid[i]);
    }
    
    free(grid);

    free(image->data);
    free(image);
}


int main(int argc, char *argv[]) {
    // Check arguments
    if (argc < 4) {
        fprintf(stderr, "Usage: ./tema1 <in_file> <out_file> <P>\n");
        return 1;
    }

    // Read input
    ppm_image *image = read_ppm(argv[1]);

    // 0. Initialize contour map
    ppm_image **contour_map = init_contour_map();

    // Start threads
    int n = atoi(argv[3]);
    pthread_t tid[n];
    thread_data data[n];
    pthread_barrier_t barrier;
    pthread_barrier_init(&barrier, NULL, n);

    // Get the X dimension of the final image
    int xx = MIN(image->x, RESCALE_X);
    int yy = MIN(image->y, RESCALE_Y);

    int step = STEP;
    
    int p = xx / step;
    int q = yy / step;

    // Malloc grid image
    unsigned char **grid = (unsigned char **)malloc((p + 1) * sizeof(unsigned char*));
    if (!grid) {
        fprintf(stderr, "Unable to allocate memory\n");
        exit(1);
    }

    for (int i = 0; i <= p; i++) {
        grid[i] = (unsigned char *)malloc((q + 1) * sizeof(unsigned char));
        if (!grid[i]) {
            fprintf(stderr, "Unable to allocate memory\n");
            exit(1);
        }
    }

    // Malloc final image
    ppm_image *final_img = (ppm_image *)malloc(sizeof(ppm_image));
    if (!final_img) {
        fprintf(stderr, "Unable to allocate memory\n");
        exit(1);
    }

    final_img->x = xx;
    final_img->y = yy;

    final_img->data = (ppm_pixel*)malloc(final_img->x * final_img->y * sizeof(ppm_pixel));

    if (!final_img) {
        fprintf(stderr, "Unable to allocate memory\n");
        exit(1);
    }

    // Start threads
    for (int i = 0; i < n; i++) {
        data[i].image = image;
        data[i].contour_map = contour_map;
        data[i].grid = grid;
        data[i].final_img = final_img;
        data[i].barrier = &barrier;
        data[i].id = i;
        data[i].step = xx / n;
        data[i].num_of_threads = n;
        pthread_create(&(tid[i]), NULL, (void *)main_thread_function, &(data[i]));
    }

    // Wait for threads
    for (int i = 0; i < n; i++) {
        pthread_join(tid[i], NULL);
    }

    // Write output
    write_ppm(final_img, argv[2]);

    free_resources(final_img, contour_map, grid, STEP);
    pthread_barrier_destroy(&barrier);
    return 0;
}
