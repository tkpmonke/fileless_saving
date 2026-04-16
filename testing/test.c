#if !defined(FLS_IMPLEMENTATION)
#define FLS_IMPLEMENTATION
#endif

#include "../fileless_saving.h"

#include <stdio.h>

typedef struct serializable_data_t {
	float a;
	int b;
} serializable_data_t;

serializable_data_t data = {
	.a = 53.43f,
	.b = 5
};

float array[] = {
	0.5f, 1.0f, 1.5f
};

int main(void) {
	printf("A = %f\n", data.a);
	printf("B = %d\n", data.b);
	printf("array = [%f, %f, %f]\n", array[0], array[1], array[2]);
	printf("Writing new values\n");
	
	data.a += 1.25;
	data.b += 1;

	array[0] += 0.5;
	array[1] += 0.75;
	array[2] += 1.5;

	printf("Serializing Data\n");

	fls_binary_t* binary = fls_initialize();
	fls_serialize_from_symbol_name(binary, "data", &data);
	fls_serialize_from_pointer(binary, &array, sizeof(array));
	fls_finish(binary);

	return 0;
}
