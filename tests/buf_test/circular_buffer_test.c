#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <assert.h>

#include "circular_buffer.h"

#define EXAMPLE_BUFFER_SIZE 10

void print_buffer_status(cbuf_handle_t cbuf);

int main(void)
{
	uint8_t * buffer  = malloc(EXAMPLE_BUFFER_SIZE * sizeof(uint8_t));

	printf("\n=== C Circular Buffer Check ===\n");

	cbuf_handle_t cbuf = circular_buf_init(buffer, EXAMPLE_BUFFER_SIZE);

	printf("Buffer initialized. ");
	print_buffer_status(cbuf);

	printf("\n******\nAdding %d values\n", EXAMPLE_BUFFER_SIZE - 1);
	for(uint8_t i = 0; i < (EXAMPLE_BUFFER_SIZE - 1); i++)
	{
		circular_buf_put(cbuf, i);
		printf("Added %u, Size now: %zu\n", i, circular_buf_size(cbuf));
	}

	print_buffer_status(cbuf);

	printf("\n******\nAdding %d values\n", EXAMPLE_BUFFER_SIZE);
	for(uint8_t i = 0; i < EXAMPLE_BUFFER_SIZE; i++)
	{
		circular_buf_put(cbuf, i);
		printf("Added %u, Size now: %zu\n", i, circular_buf_size(cbuf));
	}

	print_buffer_status(cbuf);

	printf("\n******\nReading back values: ");
	while(!circular_buf_empty(cbuf))
	{
		uint8_t data;
		circular_buf_get(cbuf, &data);
		printf("%u ", data);
	}
	printf("\n");

	print_buffer_status(cbuf);

	printf("\n******\nAdding %d values\n", EXAMPLE_BUFFER_SIZE + 5);
	for(uint8_t i = 0; i < EXAMPLE_BUFFER_SIZE + 5; i++)
	{
		circular_buf_put(cbuf, i);
		printf("Added %u, Size now: %zu\n", i, circular_buf_size(cbuf));
	}

	print_buffer_status(cbuf);

	printf("\n******\nReading back values: ");
	while(!circular_buf_empty(cbuf))
	{
		uint8_t data;
		circular_buf_get(cbuf, &data);
		printf("%u ", data);
	}
	printf("\n");

	printf("\n******\nAdding %d values using non-overwrite version\n", EXAMPLE_BUFFER_SIZE + 5);
	for(uint8_t i = 0; i < EXAMPLE_BUFFER_SIZE + 5; i++)
	{
		circular_buf_put2(cbuf, i);
	}

	print_buffer_status(cbuf);

	printf("\n******\nReading back values: ");
	while(!circular_buf_empty(cbuf))
	{
		uint8_t data;
		circular_buf_get(cbuf, &data);
		printf("%u ", data);
	}
	printf("\n");

	printf("\n******\n1111Adding buffer\n");
        circular_buf_put_range(cbuf, "123456789", 10);

        print_buffer_status(cbuf);
	printf("\n******\nReading back values: \n");

        uint8_t data[10];
        circular_buf_get_range(cbuf, data, 10);

        printf("output: %s\n", data);
        print_buffer_status(cbuf);

	printf("\n******\n222Adding buffer\n");
        circular_buf_put_range(cbuf, "1234", 4);
        print_buffer_status(cbuf);
        circular_buf_put_range(cbuf, "1234", 4);
        print_buffer_status(cbuf);
	printf("\n******\nReading back values: ");

        printf("bufsize = %d\n", circular_buf_size(cbuf));
        circular_buf_get_range(cbuf, data, 8);
        print_buffer_status(cbuf);
        printf("output: %s\n", data);



	while(!circular_buf_empty(cbuf))
	{
		uint8_t data;
		circular_buf_get(cbuf, &data);
		printf("%u ", data);
	}
	printf("\n");


	free(buffer);
	circular_buf_free(cbuf);


	return 0;
}

void print_buffer_status(cbuf_handle_t cbuf)
{
	printf("Full: %d, empty: %d, size: %zu\n",
		circular_buf_full(cbuf),
		circular_buf_empty(cbuf),
		circular_buf_size(cbuf));
}
