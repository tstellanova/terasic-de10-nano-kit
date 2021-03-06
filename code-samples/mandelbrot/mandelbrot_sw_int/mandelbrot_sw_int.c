//
// Copyright (c) 2017 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
//

#define _GNU_SOURCE
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <arm_neon.h>
#include <assert.h>

#include "mandelbrot_sw_support/mandelbrot_sw_support.h"

#define THREAD_POOL_CNT	(10)

// calculate the mandelbrot algorithm with integer math
static uint32_t int_mandelbrot(int64_t cr, int64_t ci,
						uint32_t max_iter) {

	int64_t xsqr = 0x0;
	int64_t ysqr = 0x0;
	int64_t x = 0x0;
	int64_t y = 0x0;
	uint32_t iter = 0;

	while( ((xsqr + ysqr) < 0x0400000000000000LL) && (iter < max_iter) ) {
		xsqr = (x * x);
		ysqr = (y * y);
		y = (2 * x * y) + ci;
		x = xsqr - ysqr + cr;

		//xsqr should always be positive
		//ysqr should always be positive

		//x = x  / 0x10000000;
		//x may be positive or negative so handle the sign
		if( x & 0x8000000000000000LL ) {
			x = x >> 28;
			x |= 0xfffffff000000000LL;
		} else {
			x = x >> 28;
		}

		//y = y  / 0x10000000;
		//y may be positive or negative so handle the sign
		if( y & 0x8000000000000000LL ) {
			y = y >> 28;
			y |= 0xfffffff000000000LL;
		} else {
			y = y >> 28;
		}

		iter++;
	}
	return(iter);
}

// loop overhead for the integer mandelbrot algorithm
void draw_int_mandelbrot(float in_x, float in_y, float in_x_dim,
			uint32_t in_max_iter, void *pixel_buf_ptr,
			uint32_t pixmap_width, uint32_t pixmap_height) {

	uint32_t max_iter;
	uint32_t iters;
	uint32_t i;
	uint32_t j;
	int64_t leftmost_x;
	int64_t current_x;
	int64_t current_y;
	int64_t x_dim;
	int64_t center_x;
	int64_t center_y;
	int64_t step_dim;

	// set the initial center point, x dimension, and maximum iterations
	center_x = (int64_t)(in_x * 0x0100000000000000LL);
	center_y = (int64_t)(in_y * 0x0100000000000000LL);
	x_dim = (int64_t)(in_x_dim * 0x0100000000000000LL);
	max_iter = in_max_iter;

	// calculate the step dimension between each pixel
	step_dim = x_dim / pixmap_width;
	step_dim &= 0xFFFFFFFFF0000000;

	// calculate the top left pixel value to start with
	current_x = center_x - (x_dim / 2);
	current_x &= 0xFFFFFFFFF0000000;
	leftmost_x = current_x;
	current_y = center_y + ((pixmap_height / 2) * step_dim);
	current_y &= 0xFFFFFFFFF0000000;

	//process the image

	// i counts the rows in the image
	for(i = 0 ; i < pixmap_height ; i++) {
		// j counts the columns in the row
		for(j = 0 ; j < pixmap_width ; j++) {

			// allow thread to cancel here
			pthread_testcancel();
			// evaluate this coordinate
			iters = int_mandelbrot(current_x & 0xFFFFFFFFF0000000,
				current_y & 0xFFFFFFFFF0000000, max_iter);

			if(iters == max_iter) {
				// it's in the set
				set_pixel_value(max_iter, pixel_buf_ptr,
						pixmap_width, i, j, max_iter);
			} else {
				// it's out of the set
				set_pixel_value(max_iter, pixel_buf_ptr,
						pixmap_width, i, j, iters);
			}

			// increment coordinate to next column
			current_x += step_dim;
		}
		// reset coordinate to first column of image
		current_x = leftmost_x;
		// increment coordinate to next row in image
		current_y -= step_dim;
	}
}

struct thread_s {
	uint32_t pixmap_width;
	void *pixel_buf_ptr;
	uint32_t i;
	uint32_t max_iter;
	int64_t current_x;
	int64_t current_y;
	int64_t step_dim;
};

static void *int_mandelbrot_thread(void *arg) {

	struct thread_s *thread_s_ptr = (struct thread_s *)arg;
	uint32_t pixmap_width = thread_s_ptr->pixmap_width;
	void *pixel_buf_ptr = thread_s_ptr->pixel_buf_ptr;
	uint32_t i = thread_s_ptr->i;
	uint32_t max_iter = thread_s_ptr->max_iter;
	int64_t current_x = thread_s_ptr->current_x;
	int64_t current_y = thread_s_ptr->current_y;
	int64_t step_dim = thread_s_ptr->step_dim;

	uint32_t j;
	uint32_t iters;

	// j counts the columns in the row
	for(j = 0 ; j < pixmap_width ; j++) {

		// allow thread to cancel here
		pthread_testcancel();
		// evaluate this coordinate
		iters = int_mandelbrot(current_x & 0xFFFFFFFFF0000000,
				current_y & 0xFFFFFFFFF0000000, max_iter);

		if(iters == max_iter) {
			// it's in the set
			set_pixel_value(max_iter, pixel_buf_ptr, pixmap_width,
								i, j, max_iter);
		} else {
			// it's out of the set
			set_pixel_value(max_iter, pixel_buf_ptr, pixmap_width,
								i, j, iters);
		}

		// increment coordinate to next column
		current_x += step_dim;
	}

	return(NULL);
}

static void thread_cleanup_thread_pool(void *arg) {

	pthread_t *thread_pool = (pthread_t *)arg;
	int pool_index;
	int result;

	for(pool_index = 0 ; pool_index < THREAD_POOL_CNT ; pool_index++) {

		if(thread_pool[pool_index] != 0) {

			result = pthread_cancel(thread_pool[pool_index]);
			assert((result == 0) || (result == ESRCH));

			result = pthread_join( thread_pool[pool_index], NULL);
			assert(result == 0);

			thread_pool[pool_index] = 0;
		}
	}
}

// mt loop overhead for the intrinsic integer mandelbrot algorithm
void draw_int_mandelbrot_mt(float in_x, float in_y, float in_x_dim,
			uint32_t in_max_iter, void *pixel_buf_ptr,
			uint32_t pixmap_width, uint32_t pixmap_height) {

	uint32_t max_iter;
	uint32_t i;
	int64_t leftmost_x;
	int64_t current_x;
	int64_t current_y;
	int64_t x_dim;
	int64_t center_x;
	int64_t center_y;
	int64_t step_dim;
	struct thread_s the_thread_s[THREAD_POOL_CNT];
	struct thread_s *next_struct_alloc;
	pthread_t thread_pool[THREAD_POOL_CNT] = {0};
	pthread_t *next_thread_alloc;
	int pool_index;
	int result;
	int threads_joined;

	// verify pixmap_width is modulo 4
	assert((pixmap_width % 4) == 0);

	pthread_cleanup_push(thread_cleanup_thread_pool, &thread_pool[0]);

	// set the initial center point, x dimension, and maximum iterations
	center_x = (int64_t)(in_x * 0x0100000000000000LL);
	center_y = (int64_t)(in_y * 0x0100000000000000LL);
	x_dim = (int64_t)(in_x_dim * 0x0100000000000000LL);
	max_iter = in_max_iter;

	// calculate the step dimension between each pixel
	step_dim = x_dim / pixmap_width;
	step_dim &= 0xFFFFFFFFF0000000;

	// calculate the top left pixel value to start with
	current_x = center_x - (x_dim / 2);
	current_x &= 0xFFFFFFFFF0000000;
	leftmost_x = current_x;
	current_y = center_y + ((pixmap_height / 2) * step_dim);
	current_y &= 0xFFFFFFFFF0000000;

	//process the image

	// i counts the rows in the image
	for(i = 0 ; i < pixmap_height ; i++) {

		pthread_testcancel();

		next_thread_alloc = NULL;
		while(next_thread_alloc == NULL) {
			for(pool_index = 0 ; pool_index < THREAD_POOL_CNT ;
								pool_index++) {

				if(thread_pool[pool_index] == 0) {

					next_thread_alloc =
						&thread_pool[pool_index];

					next_struct_alloc =
						&the_thread_s[pool_index];

					the_thread_s[pool_index].pixmap_width =
								pixmap_width;
					the_thread_s[pool_index].pixel_buf_ptr =
								pixel_buf_ptr;
					the_thread_s[pool_index].i = i;
					the_thread_s[pool_index].max_iter =
								max_iter;
					the_thread_s[pool_index].current_x =
								current_x;
					the_thread_s[pool_index].current_y =
								current_y;
					the_thread_s[pool_index].step_dim =
								step_dim;

					break;
				}
			}

			if(next_thread_alloc != NULL)
				break;

			threads_joined = 0;
			for(pool_index = 0 ; pool_index < THREAD_POOL_CNT ;
								pool_index++) {
				result = pthread_tryjoin_np(
						thread_pool[pool_index], NULL);
				if(result == 0) {
					thread_pool[pool_index] = 0;
					threads_joined++;
				}
			}

			pthread_testcancel();
			if(threads_joined == 0)
				pthread_yield();
		}

		result = pthread_create(next_thread_alloc, NULL,
				int_mandelbrot_thread, next_struct_alloc);
		assert(result == 0);

		// reset coordinate to first column of image
		current_x = leftmost_x;
		// increment coordinate to next row in image
		current_y -= step_dim;
	}

	for(pool_index = 0 ; pool_index < THREAD_POOL_CNT ; pool_index++) {

		if(thread_pool[pool_index] != 0) {

			result = pthread_join( thread_pool[pool_index], NULL);
			assert(result == 0);

			thread_pool[pool_index] = 0;
		}
	}

	pthread_cleanup_pop(0);
}

