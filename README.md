### Marching Squares 

    Student: Diana Preda  
    Group: 334CA

### Description
    Marching squares is a grid-based algorithm used in computer graphics. It identifies and represents contours or boundaries between regions of different values in a binary grid, creating smooth shapes by connecting grid points based on a predefined lookup table. The algorithm is used in a variety of applications, including medical imaging, meteorology, and cartography.

### Implementation
    The implementation of Marching Squares is done in C using multi-threading. The code utilizes the marching squares algorithm to generate contour lines for an input image. It involves rescaling the image, sampling a grid, and performing the marching squares algorithm in parallel using pthreads.

### Code Overview
    The program is written in C and utilizes multi-threading with pthreads.
    It starts by initializing a contour map using pre-existing images stored in the './contours' directory.
    The input image is rescaled, and bicubic interpolation is applied for scaling.
    A grid is sampled from the final image using a specified step size.
    The marching squares algorithm is then employed to generate contour lines based on the sampled grid.
    The entire process is parallelized, with each thread handling a specific portion of the image.
    Barriers are used to synchronize the execution of multiple threads at specific points in the processing pipeline. 

### Code Structure
    Thread Data Structure: The thread_data structure encapsulates information for each thread, including the input image, contour map, grid, and other necessary parameters.
    Image Rescaling: The rescale_image function handles the resizing of the input image, applying bicubic interpolation when needed.
    Grid Sampling: The sample_grid function extracts a grid from the final image, determining whether each grid point is above or below a specified threshold (SIGMA).
    Marching Squares: The march function executes the marching squares algorithm on the sampled grid, updating the final image with contour lines.
    Main Thread Function: The main_thread_function orchestrates the overall process, including rescaling, grid sampling, and marching squares.

### Improvements
    ~ The path to contour images is hard-coded ("./contours/"). If the directory structure changes, the code may break.
    ~ The code is not very modular, and the functions are not very reusable.

### Conclusion
    The Marching Squares algorithm serves as a valuable resource for extracting contour lines from input images. To enhance its efficiency, the algorithm can be parallelized, leveraging multi-threading to boost performance. Additionally, optimizations in the code can be implemented to minimize memory usage and enhance modularity, further refining the algorithm's capabilities.