#include <stdio.h>
#define HEAP_SIZE  30
#define ROOTS_N 10

#define NULL_DATA 0
#define FWD_PTR 1
#define CONST_INT_DATA 2
#define CONST_STR_DATA 3
#define PTR_DATA 4
#define RANGE_DATA 5

typedef struct node{
	unsigned char tag;
	union {
		int number;
		int ptr;
		int fwd_ptr;
		char str[8];
	};
} Node;

Node heap[2*HEAP_SIZE] = {0};
int from_hp = 0, to_hp = HEAP_SIZE;

int roots[ROOTS_N];
int roots_i=0;

void gc();
int evacuate(int i);
void traverse_roots();
void scaveneging();

int add_node(char type, int value);
int add_int_data(int number);
void add_root(int index);

void print_heap();
void print_roots();

void test_case1();
void test_case2();
void test_case3();
void test_case4();


//------------------------------------------------------------------------------------------
// Garbage collector
//------------------------------------------------------------------------------------------

void gc()
{
	traverse_roots();
	scaveneging();
};

int evacuate(int i){
	if(heap[i].tag == FWD_PTR)	//check whether already evacuated
		return heap[i].fwd_ptr;
	else {
		//evacuate
		int old_to_hp = to_hp;
		heap[to_hp++] = heap[i];
		heap[i].tag = FWD_PTR;
		heap[i].fwd_ptr = old_to_hp;

		//data, which occupies more than one node
		if(heap[old_to_hp].tag == RANGE_DATA)
			heap[to_hp++] = heap[++i];

		return old_to_hp;
	}
};

void traverse_roots()
{
	int i;
	for(i=0; i<roots_i; i++)
		roots[i] = evacuate(roots[i]);
};

void scaveneging()
{
	int i=HEAP_SIZE;
	while(i<to_hp){
		if(heap[i].tag == PTR_DATA || heap[i].tag == RANGE_DATA)
			if(heap[i].fwd_ptr<HEAP_SIZE)
				heap[i].ptr = evacuate(heap[i].fwd_ptr);
		i++;
	}
};


//------------------------------------------------------------------------------------------
// Initializing data
//------------------------------------------------------------------------------------------


int add_int_data(int number)
{
	return add_node(CONST_INT_DATA, number);
}

int add_ptr_data(int ptr){
	return add_node(PTR_DATA, ptr);
}

int add_str_data(char *str){
	int i;
	Node node;
	node.tag = CONST_STR_DATA;
	for(i=0; i<8; i++)
		node.str[i] = str[i];
	heap[from_hp] = node;
	return from_hp++;
}

int add_range_data(int ptr1, int ptr2)
{
	int old_from_hp = from_hp;
	add_node(RANGE_DATA, ptr1);
	add_node(RANGE_DATA, ptr2);
	return old_from_hp;
}

int add_node(char type, int value){
	Node node = {type, value};
	heap[from_hp] = node;
	return from_hp++;
}

void add_root(int index){
	roots[roots_i] = index;
	roots_i++;
}


//------------------------------------------------------------------------------------------
// Main function
//------------------------------------------------------------------------------------------


int main(void)
{
	test_case4();
	printf("Before:\nfrom_space:\n");
	print_heap(0, from_hp);
	printf("roots:\n");
	print_roots();

	gc();

	printf("\n\nAfter:\n");
	printf("from_space:\n");
	print_heap(0, from_hp);
	printf("to_space:\n");
	print_heap(HEAP_SIZE, to_hp);
	printf("roots:\n");
	print_roots();
	return 1;
}


//------------------------------------------------------------------------------------------
// Debugging functions
//------------------------------------------------------------------------------------------


void print_heap(int i, int hn)
{
	while(i<hn){
		switch(heap[i].tag){
			case FWD_PTR:
				printf("Forward ptr: %d\n", heap[i].fwd_ptr);
				break;
			case CONST_INT_DATA:
				printf("INTEGER: %d\n", heap[i].number);
				break;
			case PTR_DATA:
				printf("POINTER: %d\n", heap[i].ptr);
				break;
			case CONST_STR_DATA:
				printf("String: %s\n", heap[i].str);
				break;
			case RANGE_DATA:
				printf("Range: %d %d\n", heap[i].ptr, heap[i+1].ptr);
				i++;
				break;
		}
		i++;
	}
}

void print_roots()
{
	int i;
	for(i=0; i<roots_i; i++)
		printf("Root -> %d\n", roots[i]);
}


//------------------------------------------------------------------------------------------
// Test cases
//------------------------------------------------------------------------------------------


void test_case1() //traversing roots
{
	add_root(add_int_data(10));
	add_root(add_int_data(30));
	add_int_data(20);
};

void test_case2() //scavenging
{
	int ptr = add_int_data(10);
	add_root(add_ptr_data(ptr));
	add_root(add_int_data(20));
	add_root(add_ptr_data(ptr));
};

void test_case3() //string data
{
	add_root(add_str_data("maciek"));
	add_str_data("kasia");
}

void test_case4() //range data
{
	int ptr1 = add_int_data(10);
	int ptr2 = add_int_data(20);
	add_root(add_range_data(ptr1, ptr2));
}