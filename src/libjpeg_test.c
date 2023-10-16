#include <stdio.h>
#include <stdlib.h>
#include <jpeglib.h>
#include <time.h>

int main(int argc, char** argv) {
	if (argc != 2)
		bail(1, "Usage: %s <input.jpg>\n", argv[0]);

	const char* input_file = argv[1];

	const int num_repeats=5;

	for(int i=0;i<num_repeats;i++){
		// Open the input JPEG file
		FILE* infile = fopen(input_file, "rb");
		if (!infile)
			bail(1, "Error opening input file: %s\n", input_file);

		// Initialize libjpeg-turbo decompression object
		struct jpeg_decompress_struct cinfo;
		struct jpeg_error_mgr jerr;

		cinfo.err = jpeg_std_error(&jerr);
		jpeg_create_decompress(&cinfo);

		// Specify the input file
		jpeg_stdio_src(&cinfo, infile);

		// Read the JPEG header
		jpeg_read_header(&cinfo, TRUE);

		// Start the decompression process
		clock_t start_time = clock();
		jpeg_start_decompress(&cinfo);

		// Calculate dimensions and size of the image
		int width = cinfo.output_width;
		int height = cinfo.output_height;
		int num_channels = cinfo.output_components;
		int row_stride = width * num_channels;
		int image_size = row_stride * height;

		// Allocate memory for the image buffer
		unsigned char* image_buffer = (unsigned char*)malloc(image_size);

		// Decompress the image
		while (cinfo.output_scanline < height) {
			unsigned char* row_pointer = &image_buffer[cinfo.output_scanline * row_stride];
			jpeg_read_scanlines(&cinfo, &row_pointer, 1);
		}

		// Finish the decompression process
		jpeg_finish_decompress(&cinfo);

		// Cleanup
		jpeg_destroy_decompress(&cinfo);
		fclose(infile);

		clock_t end_time = clock();
		double time_taken = ((double)(end_time - start_time)) / CLOCKS_PER_SEC;

		printf("libjpeg decoded %s: %.3fms\n", argv[1], time_taken*1000);
	}

	return 0;
}

