#include <stdio.h>
#define HEAP_SIZE  32
//512
#define ROOTS_N 10

#define NULL_DATA 0
#define FWD_PTR 1
#define CONST_INT_DATA 2

int heap[2*HEAP_SIZE] = {0};
int hp = 0;
int to_space = 0;
//evacuation(int *node)
//scavenging

int roots[ROOTS_N];
int roots_i=0;

void trigger_gc();
void copy_objs();

int add_data(int number);
void add_root(int index);

void print_heap();
void test_case1();


//------------------------------------------------------------------------------------------
// Garbage collector
//------------------------------------------------------------------------------------------

int evacuate(int from_i){
	int to_evacuate = from_i;
	int fwd_ptr_value = hp;

	heap[hp++] = heap[from_i++];
	heap[hp++] = heap[from_i++];

	heap[to_evacuate]=FWD_PTR;
	heap[to_evacuate+1]=fwd_ptr_value;
	
	return fwd_ptr_value;
};

void trigger_gc()
{
	copy_objs();
}

void copy_objs()
{
	int from_space = to_space;
	to_space ^= 1;
	hp = HEAP_SIZE * to_space;

	int from_i = HEAP_SIZE * from_space;
	int from_n = HEAP_SIZE + (HEAP_SIZE * from_space);


	while(from_i< from_n && heap[from_i]!= NULL_DATA){
		evacuate(from_i);
		from_i+=2;
	}
}

//------------------------------------------------------------------------------------------
// Initializing data
//------------------------------------------------------------------------------------------
int add_data(int number)
{
	int index = hp;
	heap[hp++] = CONST_INT_DATA;
	heap[hp++] = number;
	return index;
}

void add_root(int index){
	roots[roots_i]=index;
	roots_i++;
}


//------------------------------------------------------------------------------------------
// Main function
//------------------------------------------------------------------------------------------


int main(void)
{
	test_case1();
	trigger_gc();
	print_heap();
	to_space^=1;
	print_heap();
	return 1;
}


//------------------------------------------------------------------------------------------
// Debugging functions
//------------------------------------------------------------------------------------------


void print_heap()
{
	int i=HEAP_SIZE * to_space;
	int space_n = HEAP_SIZE + (HEAP_SIZE * to_space);
	if(to_space)
		printf("from %d on:\n", HEAP_SIZE);
	else
		printf("form 0 on:\n");
	while(i<space_n && heap[i]!= NULL_DATA){
		switch(heap[i]){
			case FWD_PTR:
				printf("Forward ptr: %d\n", heap[i+1]);
				break;
			case CONST_INT_DATA:
				printf("INTEGER: %d\n", heap[i+1]);
				break;
		}
		i+=2;
	}
}


//------------------------------------------------------------------------------------------
// Test cases
//------------------------------------------------------------------------------------------


void test_case1()
{
	add_root(add_data(10));
	add_root(add_data(30));
	add_root(add_data(20));
}