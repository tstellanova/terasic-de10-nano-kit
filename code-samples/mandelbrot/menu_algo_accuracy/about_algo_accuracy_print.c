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
#include <stdint.h>

#include "menu_algo_accuracy/menu_algo_accuracy.h"
#include "do_fluid_movement/movement_functions.h"
#include "console_support/console_support.h"
#include "gpio_support/gpio_support.h"
#include "font_support/font_def_public.h"
#include "gt_timer/gt_timer.h"
#include "mandelbrot.h"
#include "de10_init/de10_init.h"

void about_algo_accuracy_print(void) {

	struct abc_font_struct *original_font;
	int i;
	const char *display_strings[] = {
"The first menu allows you to compare the accuracy between the",
"integer and floating point algorithms for a full frame",
"Mandelbrot fractal calculation.  The single thread standard C",
"algorithm implmentation is used for both the integer and",
"floating point base image calculations.  Due to the different",
"precision characteristics of floating point and integer",
"arithmetic we expect there to be some differences between the",
"two full frame computations.",
" ",
"The second and third menus allow you to compare the accuracy",
"between the various floating point algorithm implementations",
"and the various integer algorithm implmentations",
"respectively.  The single thread standard C implementation is",
"used as the base starting reference frame that is compared to",
"the NEON vector intrisic implmentations, as well as the",
"multithreaded implementations.  The integer comparisons also",
"include the FPGA hardware algorithm.  Since the comparisons",
"are all made in the same computational precision we will",
"expect there to be no differences between them.",
" ",
"Press KEY0 or KEY1 to continue...",
NULL
};

	//
	// Font metrics for 640 x 480 display:
	//	cousine_bold_16: 62 characters x 26 rows
	//	cousine_bold_18: 57 characters x 23 rows
	//	cousine_bold_20: 52 characters x 20 rows
	//	cousine_bold_30: 34 characters x 14 rows
	//
	original_font = get_console_font();
	set_console_font(&cousine_bold_16);

	console_clear();

	i = 0;
	while(display_strings[i] != NULL)
		console_print((char *)(display_strings[i++]));

	get_input_key();

	set_console_font(original_font);
}

