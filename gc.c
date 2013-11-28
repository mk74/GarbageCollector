#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define HEAP_SIZE  30
#define LOW_MEM_THRESHOLD 20
#define ROOTS_N 10

#define CNULL 0
#define CFWD_PTR 1
#define CCONST_INT 2
#define CCONST_STR 3
#define CPTR 4
#define CRANGE 5
#define CDATA_HEAD 6
#define CDATA_ND 7
#define CCONST_BOOL 8
#define CLAMBDA_ID 9
#define CLAMBDA_N 10
#define CLAMBDA_ARG 11
#define CWEAK_PTR 12
#define CSOFT_PTR 13
#define CPHANTOM_PTR_NF 14
#define CPHANTOM_PTR_F 15

typedef struct node{
	unsigned char tag;
	union {
		int number;
		int ptr;
		int fwd_ptr;
		char *str;
		int constructor;
		bool bool_value;
	};
} Node;

Node heap[2*HEAP_SIZE] = {0};
int from_hp = 0, to_hp = HEAP_SIZE;

int roots[ROOTS_N], other_ptrs[ROOTS_N], finilized_ptrs[ROOTS_N];
int roots_i=0, soft_ptr_i=0, finilized_ptr_i=0;

void gc();
int evacuate(int i);
void traverse_roots();
int traverse_other_ptrs();
void scaveneging();
void look_back();

int add_node(char type, int value);
int add_bool(bool value);
int add_int(int number);
int add_ptr(int ptr);
int add_str(char[]);
int add_range(int ptr1, int ptr2);
int add_data(int c, int n, int *ptrs);
int add_lambda(int id, int n, int *ptrs);

void add_root(int index);
void finilize_ptr(int ptr);

void print_heap();
void print_roots();
void print_other_ptrs();
void print_finilized_ptrs();

void test_case1();
void test_case2();
void test_case3();
void test_case4();
void test_case5();
void test_case6();
void test_case7();
void test_case8();
void test_case9();
void test_case10();

//------------------------------------------------------------------------------------------
// Garbage collector
//------------------------------------------------------------------------------------------

void gc()
{
	traverse_roots();
	scaveneging(HEAP_SIZE);
	int old_to_hp = traverse_other_ptrs();
	if(old_to_hp != to_hp)
		scaveneging(old_to_hp);

	look_back();
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

		//lambda
		if(heap[old_to_hp].tag == CLAMBDA_ID){
			int end = heap[++i].number;
			heap[to_hp++] = heap[i];
			for(; i<end; i++)
				heap[to_hp++] = heap[i];
		}

		return old_to_hp;
	}
};

void traverse_roots()
{
	int i;
	for(i=0; i<roots_i; i++)
		roots[i] = evacuate(roots[i]);
};

int traverse_other_ptrs()
{
	int i, old_to_hp= to_hp;
	for(i=0; i<soft_ptr_i; i++){
		int ptr = heap[other_ptrs[i]].ptr;
		if(heap[ptr].tag == CFWD_PTR) //something else was referring to that node
			heap[other_ptrs[i]].ptr = heap[ptr].ptr;
		else{
			int left_mem = 2 * HEAP_SIZE - to_hp;
			if(heap[other_ptrs[i]].tag == CSOFT_PTR && left_mem > LOW_MEM_THRESHOLD){
				heap[other_ptrs[i]].ptr = evacuate(ptr);
			}else
				heap[other_ptrs[i]].ptr = CNULL;
		}
	}
	return old_to_hp;
};

void scaveneging(int i)
{
	while(i<to_hp){
		if(heap[i].tag == CPTR || heap[i].tag == CRANGE || heap[i].tag == CDATA_ND || heap[i].tag == CPHANTOM_PTR_NF)
			heap[i].ptr = evacuate(heap[i].fwd_ptr);

		//weak/soft pointers
		if(heap[i].tag == CWEAK_PTR || heap[i].tag == CSOFT_PTR){
			if(heap[heap[i].ptr].tag == CFWD_PTR)
				heap[i].ptr = evacuate(heap[i].fwd_ptr);
			else
				other_ptrs[soft_ptr_i++]=i;
		}

		//finilized phantom pointers
		if(heap[i].tag == CPHANTOM_PTR_F){
			heap[i].ptr = 0;
			finilized_ptrs[finilized_ptr_i++]=i;
		}
		i++;
	}
};

void look_back(){
	int i = 0;
	while(i<from_hp){
		if(heap[i].tag == CCONST_STR){
			free(heap[i].str);
			heap[i].str = NULL;
		}
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

int add_soft_ptr(int ptr){
	return add_node(CSOFT_PTR, ptr);
}

int add_weak_ptr(int ptr){
	return add_node(CWEAK_PTR, ptr);
}

int add_phantom_ptr(int ptr){
	return add_node(CPHANTOM_PTR_NF, ptr);
}

int add_str(char *str)
{
	int i;
	Node node;
	node.tag = CCONST_STR;
	node.str = strdup(str);
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
	int i, old_from_hp = from_hp;
	add_node(CDATA_HEAD, c);
	for(i=0; i<n; i++)
		add_node(CDATA_ND, ptrs[i]);
	return old_from_hp;
}

int add_lambda(int id, int n, int *ptrs)
{
	int i, old_from_hp = from_hp;
	add_node(CLAMBDA_ID, id);
	add_node(CLAMBDA_N, n);
	for(i=0; i<n; i++)
		add_node(CLAMBDA_ARG, ptrs[i]);
	return old_from_hp;
}

int add_bool(bool value)
{
	Node node = {CCONST_BOOL, value};
	heap[from_hp] = node;
	return from_hp++;
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

void finilize_ptr(int ptr)
{
	heap[ptr].tag = CPHANTOM_PTR_F;
}


//------------------------------------------------------------------------------------------
// Main function
//------------------------------------------------------------------------------------------


int main(void)
{
	test_case10();
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
	printf("soft pointers:\n");
	print_other_ptrs();
	printf("\nfinialized pointers:\n");
	print_finilized_ptrs();
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
			case CCONST_BOOL:
				if(heap[i].bool_value)
					printf("Boolean: true\n");
				else
					printf("Boolean: false\n");
			case CLAMBDA_ID:
				printf("Lambda function id: %d, ", heap[i].number);
				printf("n: %d, ", heap[++i].number);
				int end = i + heap[i].number;
				for(; i<end; i++)
					printf("arg ptr: %d, ", heap[i].ptr);
				printf("\n");
				break;
			case CWEAK_PTR:
				printf("Weak pointer: %d\n", heap[i].ptr);
				break;
			case CSOFT_PTR:
				printf("Soft pointer: %d\n", heap[i].ptr);
				break;	
			case CPHANTOM_PTR_NF:
				printf("Phantom ptr(not-finlized): %d\n", heap[i].ptr);
				break;	
			case CPHANTOM_PTR_F:
				printf("Phantom ptr(finilized): %d\n", heap[i].ptr);
				break;	
			default:
				printf("\n");
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

void print_other_ptrs()
{
	int i;
	for(i=0; i<soft_ptr_i; i++){
		if(heap[other_ptrs[i]].tag == CWEAK_PTR)
			printf("Weak ptr -> %d\n", other_ptrs[i]);
		if(heap[other_ptrs[i]].tag == CSOFT_PTR)
			printf("Soft ptr -> %d\n", other_ptrs[i]);
	}
}

void print_finilized_ptrs()
{
	int i;
	for(i=0; i<finilized_ptr_i; i++)
		printf("Finilized ptr -> %d\n", finilized_ptrs[i]);
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

void test_case6() //bool
{
	add_root(add_bool(true));
}

void test_case7() //lambda expression
{
	int ptrs[5], i;
	for(i=0; i<5; i++)
		ptrs[i] = add_int(10 +i);
	add_root(add_lambda(7, 5, ptrs));
}

void test_case8() //weak pointers
{
	int ptr1 = add_int(10);
	int ptr2 = add_int(11);
	int ptr3 = add_int(12);
	add_root(add_weak_ptr(ptr1));
	add_root(ptr1);
	add_root(add_weak_ptr(ptr3));
}

void test_case9() //soft pointers
{
	int i;
	for(i=0; i<7; i++){
		add_root(add_soft_ptr(add_int(10 +i)));
	}
}

void test_case10() //phantom pointers
{
	int ptr1 = add_int(10);
	int ptr2 = add_int(11);
	int phantom_ptr = add_phantom_ptr(ptr1);
	finilize_ptr(phantom_ptr);
	add_root(phantom_ptr);
	add_root(add_phantom_ptr(ptr2));
	
}