#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>


#include "qdbmp.h"
#include "timer.h"

const int NUM_THREADS = 4;

typedef struct {
	char * input_file;
	char * output_file;
	int radius;
	int timer;
} commands_t;

BMP* bmp;
BMP* swap; 
commands_t commands;

int parse_cmd_line(int argc, char ** argv, commands_t * out_commands) 
{
	int i;
	int inputset = 0;
	int radiusset = 0;
	int outputset = 0;
	(*out_commands).timer = 0;

	for (i = 1; i < argc; ++i) 
	{
		if (strcmp(argv[i], "-o") == 0 && i + 1  < argc) 
		{
			(*out_commands).output_file = argv[i+1];
			++i;
			outputset = 1;
		}
		else if (strcmp(argv[i], "-r") == 0 && i + 1  < argc) 
		{
			(*out_commands).radius = strtol(argv[i+1], NULL, 10);
			++i;
			radiusset = 1;
		}
		else if (strcmp(argv[i], "-t") == 0)
		{
			(*out_commands).timer = 1;
		}
		else 
		{
			(*out_commands).input_file = argv[i];
			inputset = 1;
		}
	}
	if (inputset && outputset && radiusset) return 1;
	return 0;
}

void* horizontal_blur(void* tnum) 
{
   	UINT width, height;
	UINT r, g, b;
	UCHAR* pixel;

	int thread_num = (int)tnum;
	int radius = commands.radius;
	int blur_width = 2*radius+1;
    int i, x, y;
	int next, prev;
	float blur_width_divisor = 1.0 / blur_width;

	int start, end;

	UINT bytes_per_row;
	UCHAR bytes_per_pixel;
	UCHAR* data;
	UCHAR* outdata;

	bytes_per_pixel = BMP_GetBytesPerPixel(bmp);
	bytes_per_row = BMP_GetBytesPerRow(bmp);
	data = BMP_GetData(bmp);
	outdata = BMP_GetData(swap);

	//Get dimensions
	width = BMP_GetWidth(bmp); 
	height = BMP_GetHeight(bmp);

	start = (height / NUM_THREADS) * thread_num;
	if (thread_num == NUM_THREADS - 1)
		end = height;
	else
		end = (height / NUM_THREADS)*(thread_num + 1);

    for (y = start; y < end; ++y) 
    {
		pixel = data + ((height - y - 1) * bytes_per_row);
		r = (*( pixel + 2 ))*radius;
		g = (*( pixel + 1 ))*radius;
		b = (*( pixel + 0 ))*radius;

        for (i = 0; i <= radius; ++i)
        {
        	//Calculate the sum of all pixels' red, green and blue values
        	//for the blur width.
        	pixel = data + ((height - y - 1) * bytes_per_row + ((i<0) ? 0 : i) * bytes_per_pixel);
			r += *( pixel + 2 );
			g += *( pixel + 1 );
			b += *( pixel + 0 );
        }

        for (x = 0; x < width; ++x) 
        {
        	// //Set the current pixels value to the sum of calulated pixels divided by blur width.
			pixel = outdata + ((height - y - 1) * bytes_per_row + x * bytes_per_pixel);			
			/* Note: colors are stored in BGR order */
			*( pixel + 2 ) = r * blur_width_divisor;
			*( pixel + 1 ) = g * blur_width_divisor;
			*( pixel + 0 ) = b * blur_width_divisor;

         	//Check if edges of blur width are out of bounds.
           	next = (x+radius+1 > width - 1) ? width - 1 : x+radius+1; //If at the end of array (out of bounds)
         	prev = (x-radius < 0) ?  0 : x-radius; //Again, check if out of bounds.

            //Add the next pixel to the pixel sums.
		 	pixel = data + ((height - y - 1) * bytes_per_row + next * bytes_per_pixel);
			r += *( pixel + 2 );
			g += *( pixel + 1 );
			b += *( pixel + 0 );

            //... And remove the previous one.
			pixel = data + ((height - y - 1) * bytes_per_row + prev * bytes_per_pixel);
			r -= *( pixel + 2 );
			g -= *( pixel + 1 );
			b -= *( pixel + 0 );
		}
    }
    return NULL;
}

//This function is MUCH faster than the simple way, since this function
//will not cause nearly as many cache misses. 
void* vertical_blur(void* tnum) 
{
	UINT width, height;
	UINT *r, *g, *b;
	UCHAR* pixel;
	
	int thread_num = (int)tnum;
	int radius = commands.radius;
	int blur_width = 2*radius+1;
	int i, x, y;
	int next, prev;
	int start, end;
	float blur_width_divisor = 1.0 / blur_width;

	UINT bytes_per_row;
	UCHAR bytes_per_pixel;
	UCHAR* data;
	UCHAR* outdata;

	bytes_per_pixel = BMP_GetBytesPerPixel(swap);
	bytes_per_row = BMP_GetBytesPerRow(swap);
	data = BMP_GetData(swap);
	outdata = BMP_GetData(bmp);

	//Get dimensions
	width = BMP_GetWidth(swap); 
	height = BMP_GetHeight(swap); 

	r = malloc(width*sizeof(UINT*));
	g = malloc(width*sizeof(UINT*));
	b = malloc(width*sizeof(UINT*));

	start = (width / NUM_THREADS) * thread_num;
	if (thread_num == NUM_THREADS - 1)
		end = width;
	else
		end = (width / NUM_THREADS)*(thread_num + 1);

    for (x = start; x < end; ++x) 
    {
    	pixel = data + ((height - 1) * bytes_per_row + x * bytes_per_pixel);
		r[x] = (*( pixel + 2 ))*radius;
		g[x] = (*( pixel + 1 ))*radius;
		b[x] = (*( pixel + 0 ))*radius;
    }

    for (i = 0; i <= radius; ++i)
    {
    	for (x = start; x < end; ++x) 
    	{
			//Calculate the sum of all pixels' red, green and blue values
			//for the blur width.	
	    	pixel = data + ((height - i - 1) * bytes_per_row + x * bytes_per_pixel);
			r[x] += *( pixel + 2 );
			g[x] += *( pixel + 1 );
			b[x] += *( pixel + 0 );
    	}
    }

    for (y = 0; y < height; ++y) 
    {    
    	//Check if edges of blur width are out of bounds.
        next = (y+radius+1 > height - 1) ? height - 1 : y+radius+1; //If at the end of array (out of bounds)
        prev = (y-radius < 0) ?  0 : y-radius; //Again, check if out of bounds.

    	for (x = start; x < end; ++x) 
    	{
	       	//Set the current pixels value to the sum of calulated pixels divided by blur width.
			pixel = outdata + ((height - y - 1) * bytes_per_row + x * bytes_per_pixel);

			/* Note: colors are stored in BGR order */
			*( pixel + 2 ) = r[x] * blur_width_divisor;
			*( pixel + 1 ) = g[x] * blur_width_divisor;
			*( pixel + 0 ) = b[x] * blur_width_divisor;

	        //Add the next pixel to the pixel sums.
			pixel = data + ((height - next - 1) * bytes_per_row + x * bytes_per_pixel);
			r[x] += *( pixel + 2 );
			g[x] += *( pixel + 1 );
			b[x] += *( pixel + 0 );

	        //... And remove the previous one.
			pixel = data + ((height - prev - 1) * bytes_per_row + x * bytes_per_pixel);
			r[x] -= *( pixel + 2 );
			g[x] -= *( pixel + 1 );
			b[x] -= *( pixel + 0 );
		}
    }
    free(r);
    free(g);
    free(b);
    return NULL;
}

int main(int argc, char ** argv) 
{	
	UINT width, height; 
	USHORT depth; 
	int i, time;

	pthread_t thread[NUM_THREADS];
	pthread_attr_t attr;

	if (!parse_cmd_line(argc, argv, &commands)) 
	{
		printf("Example usage: %s -r 3 -o output input.\n", argv[0]);
		return -1;
	}
	/* Read an image file */ 
	bmp = BMP_ReadFile(commands.input_file); 
	BMP_CHECK_ERROR(stderr, -2); /* If an error has occurred, notify and exit */ 

	width = BMP_GetWidth(bmp); 
	height = BMP_GetHeight(bmp); 
	depth = BMP_GetDepth(bmp);

	if (commands.timer) time = timer();
	
	swap = BMP_Create(width, height, depth);
	BMP_CHECK_ERROR(stderr, 0);

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

	/* Create and run the threads */
	for (i=0; i<NUM_THREADS; i++)
		pthread_create(&thread[i], &attr, horizontal_blur, (void *) i);

	/* Join the threads - barrier*/
	for (i=0; i<NUM_THREADS; i++)
		pthread_join(thread[i], NULL);

	for (i=0; i<NUM_THREADS; i++)
		pthread_create(&thread[i], &attr, vertical_blur, (void *) i);

	for (i=0; i<NUM_THREADS; i++)
		pthread_join(thread[i], NULL);

	if (commands.timer) print_time(timer() - time);

	/* Save result */ 
	BMP_WriteFile(bmp, commands.output_file); 
	BMP_CHECK_ERROR(stderr, -3); 

	/* Free all memory allocated for the image */ 
	BMP_Free(bmp);
	BMP_Free(swap);
	
	return 0;
}