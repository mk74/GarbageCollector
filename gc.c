#include <stdio.h>
#define HEAP_SIZE  30
#define ROOTS_N 10

#define CNULL 0
#define CFWD_PTR 1
#define CCONST_INT 2
#define CCONST_STR 3
#define CPTR 4
#define CRANGE 5
#define CDATA_HEAD 6
#define CDATA_ND 7

typedef struct node{
	unsigned char tag;
	union {
		int number;
		int ptr;
		int fwd_ptr;
		char str[8];
		int constructor;
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
int add_int(int number);
int add_ptr(int ptr);
int add_str(char[]);
int add_range(int ptr1, int ptr2);
void add_root(int index);

void print_heap();
void print_roots();

void test_case1();
void test_case2();
void test_case3();
void test_case4();
void test_case5();


//------------------------------------------------------------------------------------------
// Garbage collector
//------------------------------------------------------------------------------------------

void gc()
{
	traverse_roots();
	scaveneging();
};

int evacuate(int i){
	if(heap[i].tag == CFWD_PTR)	//check whether already evacuated
		return heap[i].fwd_ptr;
	else {
		//evacuate
		int old_to_hp = to_hp;
		heap[to_hp++] = heap[i];
		heap[i].tag = CFWD_PTR;
		heap[i].fwd_ptr = old_to_hp;

		//data, which occupies more than one node
		//ranges
		if(heap[old_to_hp].tag == CRANGE)
			heap[to_hp++] = heap[++i];

		//data
		if(heap[old_to_hp].tag == CDATA_HEAD)
			while(heap[++i].tag == CDATA_ND)
				heap[to_hp++] = heap[i];

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
		if(heap[i].tag == CPTR || heap[i].tag == CRANGE || heap[i].tag == CDATA_ND)
			if(heap[i].fwd_ptr<HEAP_SIZE)
				heap[i].ptr = evacuate(heap[i].fwd_ptr);
		i++;
	}
};


//------------------------------------------------------------------------------------------
// Initializing data
//------------------------------------------------------------------------------------------


int add_int(int number)
{
	return add_node(CCONST_INT, number);
}

int add_ptr(int ptr)
{
	return add_node(CPTR, ptr);
}

int add_str(char *str)
{
	int i;
	Node node;
	node.tag = CCONST_STR;
	for(i=0; i<8; i++)
		node.str[i] = str[i];
	heap[from_hp] = node;
	return from_hp++;
}

int add_range(int ptr1, int ptr2)
{
	int old_from_hp = from_hp;
	add_node(CRANGE, ptr1);
	add_node(CRANGE, ptr2);
	return old_from_hp;
}

int add_data(int c, int n, int *ptrs)
{
	int old_from_hp = from_hp;
	int i;
	add_node(CDATA_HEAD, c);
	for(i=0; i<n; i++)
		add_node(CDATA_ND, ptrs[i]);
	return old_from_hp;
}

int add_node(char type, int value)
{
	Node node = {type, value};
	heap[from_hp] = node;
	return from_hp++;
}

void add_root(int index)
{
	roots[roots_i] = index;
	roots_i++;
}


//------------------------------------------------------------------------------------------
// Main function
//------------------------------------------------------------------------------------------


int main(void)
{
	test_case5();
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
		printf("%d:", i);
		switch(heap[i].tag){
			case CFWD_PTR:
				printf("Forward ptr: %d\n", heap[i].fwd_ptr);
				break;
			case CCONST_INT:
				printf("INTEGER: %d\n", heap[i].number);
				break;
			case CPTR:
				printf("POINTER: %d\n", heap[i].ptr);
				break;
			case CCONST_STR:
				printf("String: %s\n", heap[i].str);
				break;
			case CRANGE:
				printf("Range: %d %d\n", heap[i].ptr, heap[i+1].ptr);
				i++;
				break;
			case CDATA_HEAD:
				printf("Data constructor: %d, ", heap[i].constructor);
				while(heap[++i].tag == CDATA_ND)
					printf("node: %d, ", heap[i].ptr);
				printf("\n");
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
	add_root(add_int(10));
	add_root(add_int(30));
	add_int(20);
};

void test_case2() //scavenging
{
	int ptr = add_int(10);
	add_root(add_ptr(ptr));
	add_root(add_int(20));
	add_root(add_ptr(ptr));
};

void test_case3() //string data
{
	add_root(add_str("maciek"));
	add_str("kasia");
}

void test_case4() //range data
{
	int ptr1 = add_int(10);
	int ptr2 = add_int(20);
	add_root(add_range(ptr1, ptr2));
}

void test_case5() //data
{
	int ptrs[5], i;
	for(i=0; i<5; i++)
		ptrs[i] = add_int(10 +i);
	add_root(add_data(7, 5, ptrs));
}