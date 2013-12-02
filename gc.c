#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#define GENERATIONAL_GC_FREQ 5

#define HEAP_SIZE  100000
#define LOW_MEM_THRESHOLD 20
#define ROOTS_N 500000

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
#define CBDATA_PTR 16
#define CBDATA_HEAD_N 17
#define CBDATA_HEAD_C 18
#define CBDATA_ND 19

typedef struct node{
	unsigned char tag;
	void *value;
} Node;

//from_space and to_space
Node heap[2*HEAP_SIZE] = {0};
Node *from_start = &heap[0], *from_hp= &heap[0], *to_start = &heap[HEAP_SIZE], *to_hp = &heap[HEAP_SIZE], *to_mem_end = &heap[2*HEAP_SIZE];

//roots, soft/weak pointers, phantom pointers, big data
Node *roots[ROOTS_N], *other_ptrs[ROOTS_N], *finalized_ptrs[ROOTS_N], *big_data[ROOTS_N];
int roots_i=0, other_ptr_i=0, finalized_ptr_i=0, big_data_i=0;

int collector_counter = 0;
bool generational_gc = false;

//variables for time complexity analysis
long int scaveneging_counter = 0, copy_counter = 0;

//mutator
void mutator_start();
void mutator_continue();

//main GC funcitons
void collector();
void change_spaces();
void minor_collection();
void major_collection();
void swap_spaces();
void collection(Node *old_to_hp);
Node* evacuate(Node *node);
void traverse_roots();
void traverse_other_ptrs(int type);
void scavenege_new(Node *to_new);
void scavenege_big_data(Node *head_node);
void scaveneging(Node *node, Node *end_node);
void traverse_free();

//initialize data
Node* add_int(intptr_t number);
Node* add_bool(bool value);
Node* add_str(char *str);
Node* add_ptr(Node *ptr);
Node* add_soft_ptr(Node *node);
Node* add_weak_ptr(Node *node);
Node* add_phantom_ptr(Node *node);
Node* add_range(Node* node1, Node* node2);
Node* add_data(intptr_t c, int n, Node **nodes);
Node* add_big_data(intptr_t n, intptr_t c, Node **nodes);
Node* add_lambda(intptr_t id, intptr_t n, Node **nodes);
Node* add_node(char type, void* value);
void copy_node(Node *dst, Node*src);
void add_root(Node *ptr);
void finalize_ptr(Node *node);

//debugging functions
void print_nodes(Node *node, Node *end_node);
void print_roots();
void print_other_ptrs();
void print_finalized_ptrs();
void print_big_data();
void print_mem_state();
void print_collector_state();

//test units
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
void test_case11();

void test_heap1_start();
void test_heap1_continue();
void test_heap2_start();
void test_heap2_continue();
void test_heap3_start();
void test_heap3_continue();

//Mutator test cases:
void (*mutator_starts[])(void) = {test_heap1_start, test_heap2_start, test_heap3_start, test_case1, test_case2, test_case3, test_case4,
								  test_case5, test_case6, test_case7, test_case8, test_case9, test_case10, test_case11};
int mutator_starts_n = 14;							

void (*mutator_continues[])(void) = {test_heap1_continue, test_heap2_continue, test_heap3_continue};
int mutator_continues_n = 3;


//------------------------------------------------------------------------------------------
// Garbage collector
//------------------------------------------------------------------------------------------


void collector()
{
	//init
	collector_counter++;
	finalized_ptr_i = 0;
	big_data_i = 0;

	if(generational_gc){
		//generational copying garbage collector
		minor_collection();
		if(collector_counter == GENERATIONAL_GC_FREQ){
			major_collection();
			collector_counter = 0;
		}
		from_hp = from_start;  //forget about any old data put in from_space
	}else{
		//copying garbage collector
		collection(to_start);
		swap_spaces();
	}
	other_ptr_i = 0;
}

void minor_collection(){
	collection(to_hp);
}

void major_collection(){
	swap_spaces();

	//init collection and collect
	big_data_i =0; other_ptr_i = 0;
	collection(to_start);

	//update positions of finailized ptrs
	for(int i=0; i<finalized_ptr_i; i++)
		if(finalized_ptrs[i]->tag == CFWD_PTR)
			finalized_ptrs[i] = finalized_ptrs[i]->value;
}

void swap_spaces(){
	//switch from_space and to_space: swap from_start with to_start, set from_hp and to_hp
	Node *tmp = from_start;
	from_start = to_start;
	to_start = tmp;
	from_hp = to_hp;
	to_hp = to_start;
	to_mem_end = to_start + HEAP_SIZE;
}

void collection(Node *old_to_hp)
{
	//traverse and scavenege roots
	traverse_roots();
	scavenege_new(old_to_hp);

	// scavenege big data
	old_to_hp = to_hp;
	for(int i=0; i<big_data_i; i++){
		scavenege_big_data(big_data[i]->value);
	}
	scavenege_new(old_to_hp);

	//traverse and scavenege weak/soft pointers
	old_to_hp = to_hp;
	traverse_other_ptrs(CSOFT_PTR);
	traverse_other_ptrs(CWEAK_PTR);
	scavenege_new(old_to_hp);

	//traverse and free unused objects(strings and bigdata)
	traverse_free(from_start, from_hp);
}

Node* evacuate(Node *node)
{
	if(node>=to_start && node<to_hp)
		return node;	//don't evacuate nodes from to_space (intergenerational pointers)

	if(node->tag == CFWD_PTR)	//check whether already evacuated
		return node->value;
	else {
		//evacuate
		Node *head_node = to_hp;
		copy_node(to_hp, node);
		node->tag = CFWD_PTR;
		node->value = to_hp++;

		//data, which occupies more than one node:

		//ranges
		if(head_node->tag == CRANGE){
			Node *next_node = (node + 1);
			copy_node(to_hp++, next_node);
		}

		//data
		if(head_node->tag == CDATA_HEAD){
			Node *next_node = (node + 1);
			while(next_node->tag == CDATA_ND){
				copy_node(to_hp++, next_node);
				next_node = (next_node + 1);
			}
		}

		//lambda
		if(head_node->tag == CLAMBDA_ID){
			Node *next_node = (node + 1);
			int end = (intptr_t)next_node->value;
			copy_node(to_hp++, next_node);
			for(int i=0; i<end; i++){
				next_node = (next_node + 1);
				copy_node(to_hp++, next_node);
			}
		}

		return head_node;
	}
}

void traverse_roots()
{
	for(int i=0; i<roots_i; i++)
		roots[i] = evacuate(roots[i]);
}

void traverse_other_ptrs(int type)
{
	for(int i=0; i<other_ptr_i; i++){
		if(other_ptrs[i]->tag != type)
			continue;
		Node *node = (Node*)(other_ptrs[i]->value);
		if(node->tag == CFWD_PTR) //something else was already referring to that node
			other_ptrs[i]->value = node->value;
		else{
			int left_mem = to_mem_end - to_hp;
			if(other_ptrs[i]->tag == CSOFT_PTR && left_mem > LOW_MEM_THRESHOLD){
				other_ptrs[i]->value = evacuate(node);
			}else
				other_ptrs[i]->value = CNULL;
		}
	}
}

void scavenege_new(Node *to_new)
{
	Node *to_end = to_hp;
	while(to_new!=to_end){ //kept scavenging as long as new things are added to the end of to_space area
		scaveneging(to_new, to_end);
		to_new = to_end;
		to_end = to_hp;
	}
}

void scavenege_big_data(Node *head_node)
{
	Node *end_node = head_node + (intptr_t)(head_node->value) + 2;
	head_node+=2;
	scaveneging(head_node, end_node);
}

void scaveneging(Node *node, Node *end_node)
{
	scaveneging_counter += (end_node - node);

	while(node<end_node){
		if(node->tag == CPTR || node->tag == CRANGE || node->tag == CDATA_ND 
		   || node->tag == CPHANTOM_PTR_NF || node->tag == CLAMBDA_ARG || node->tag == CBDATA_ND)
			node->value = evacuate(node->value);

		//weak/soft pointers
		if(node->tag == CWEAK_PTR || node->tag == CSOFT_PTR){
			if(node->value!=CNULL){
				if(((Node*)node->value)->tag == CFWD_PTR)
					node->value = evacuate(node->value);
				else
					other_ptrs[other_ptr_i++] = node;
			}
		}

		//finalized phantom pointers
		if(node->tag == CPHANTOM_PTR_F && node->value != CNULL){
			node->value = CNULL;
			finalized_ptrs[finalized_ptr_i++] = node;
		}

		//big data
		if(node->tag == CBDATA_PTR)
			big_data[big_data_i++] = node;
		node++;
	}
}

void traverse_free(Node *node, Node *end_node)
{
	while(node<end_node){

		if(node->tag ==CBDATA_PTR){ //recursive traverse_free bigdata objects
			Node *head_node = node->value;
			Node *end_node = head_node + (intptr_t)head_node->value + 2;
			head_node+=2;
			traverse_free(head_node, end_node);
		}

		if(node->tag == CCONST_STR || node->tag == CBDATA_PTR){
			free(node->value);
			node->value = CNULL;
		}

		node++;
	}
}


//------------------------------------------------------------------------------------------
// Initializing data
//------------------------------------------------------------------------------------------


Node* add_int(intptr_t number)
{
	return add_node(CCONST_INT, (void *)number);
}

Node* add_bool(bool value)
{
	return add_node(CCONST_BOOL, (void *)value);	
}

Node* add_str(char *str)
{
	return add_node(CCONST_STR, (void *)strdup(str));	
}

Node* add_ptr(Node *node)
{
	return add_node(CPTR, node);
}

Node* add_soft_ptr(Node *node)
{
	return add_node(CSOFT_PTR, node);
}

Node* add_weak_ptr(Node *node)
{
	return add_node(CWEAK_PTR, node);
}

Node* add_phantom_ptr(Node *node)
{
	return add_node(CPHANTOM_PTR_NF, node);
}

Node* add_range(Node* node1, Node* node2)
{
	Node *head_node = add_node(CRANGE, node1);
	add_node(CRANGE, node2);
	return head_node;
}

Node* add_data(intptr_t c, int n, Node **nodes)
{
	Node *head_node = add_node(CDATA_HEAD, (void*) c);
	for(int i=0; i<n; i++)
		add_node(CDATA_ND, nodes[i]);
	return head_node;
}

Node* add_big_data(intptr_t n, intptr_t c, Node **nodes)
{
	//construct big data outside of the heap
	Node *big_data = malloc(sizeof(Node) * (n+2) );
	big_data[0]= (Node){CBDATA_HEAD_N, (void *)n};
	big_data[1]= (Node){CBDATA_HEAD_C, (void *)c};
	for(int i=0; i<n; i++)
		big_data[i+2] = (Node){CBDATA_ND, nodes[i]};

	//add reference to that memory area
	return add_node(CBDATA_PTR, (void *) big_data);
}

Node* add_lambda(intptr_t id, intptr_t n, Node **nodes)
{
	Node *head_node=add_node(CLAMBDA_ID, (void*)id);
	add_node(CLAMBDA_N, (void *)n);
	for(int i=0; i<n; i++)
		add_node(CLAMBDA_ARG, nodes[i]);
	return head_node;
}

Node* add_node(char type, void *value)
{
	if(from_hp - from_start+1 >= HEAP_SIZE){
		printf("You are trying to use too much memory\n");
		exit(0);
	}
	from_hp->tag = type;
	from_hp->value = value;
	return from_hp++;
}

void copy_node(Node *dst, Node *src)
{
	copy_counter++;

	dst->tag = src->tag;
	dst->value = src->value;
}

void add_root(Node* ptr)
{
	roots[roots_i] = ptr;
	roots_i++;
}

void finalize_ptr(Node* node)
{
	node->tag = CPHANTOM_PTR_F;
}


//------------------------------------------------------------------------------------------
// Main function + mutator
//------------------------------------------------------------------------------------------


int main(int argc, char *argv[])
{
	int collections_n = 0;
	int heap_case = 0;

	//user's input
	if(argc>1)
		heap_case = atoi(argv[1]);
	if(argc>2)
		generational_gc = (atoi(argv[2]) == 1)	? true : false;
	if(argc>3)
		collections_n = atoi(argv[3]);

	mutator_start(heap_case);
	printf("[DEBUG]After data initialization:\n");
	print_mem_state();
	collector();

	for(int i=0; i<collections_n; i++){
		mutator_continue(heap_case);
		collector();
	}

	printf("\n\n[DEBUG]After all collections:\n");
	print_mem_state();

	printf("\n\nTime complexity report:\n");
	printf("\tScaveneging:\t\t%ld\n\tCopying:\t\t%ld\n", scaveneging_counter, copy_counter);
	return 1;
}

void mutator_start(int heap_case){
	//the heap initialization
	if(heap_case<mutator_starts_n)
		(*mutator_starts[heap_case])();
}

void mutator_continue(int heap_case){
	//finalized could be passed to this funciton, instead of being kept as global variable
	print_finalized_ptrs();	

	if(heap_case<mutator_continues_n)
		(*mutator_continues[heap_case])();

}


//------------------------------------------------------------------------------------------
// Debugging functions
//------------------------------------------------------------------------------------------


void print_mem_state()
{
	printf("from_space[%ld]: %p-%p\n", (from_hp-from_start), from_start, from_hp);
	print_nodes(from_start, from_hp);
	printf("to_space[%ld]: %p-%p END: %p\n", (to_hp-to_start), to_start, to_hp, to_mem_end);
	print_nodes(to_start, to_hp);
	print_roots();
	print_big_data();
}

void print_nodes(Node *node, Node *end_node)
{
	while(node < end_node){
		printf("%p:", node);
		switch(node->tag){
			case CNULL:
				printf("Null ptr: %p\n", node->value);
				break;
			case CFWD_PTR:
				printf("Forward ptr: %p\n", node->value);
				break;
			case CCONST_INT:
				printf("Integer: %ld\n", (intptr_t)node->value);
				break;
			case CPTR:
				printf("Pointer: %p\n", node->value);
				break;
			case CCONST_STR:
				printf("String: %s\n", (char *)node->value);
				break;
			case CRANGE:
				printf("Range: %p\n", node->value);
				break;

			case CDATA_HEAD:
				printf("Data constructor: %ld\n", (intptr_t)node->value);
				break;
			case CDATA_ND:
				printf("Data node ptr: %p\n", node->value);
				break;

			case CCONST_BOOL:
				if((bool)(node->value))
					printf("Boolean: true\n");
				else
					printf("Boolean: false\n");
				break;

			//lambda
			case CLAMBDA_ID:
				printf("Lambda function id: %ld\n", (intptr_t)node->value);
				break;
			case CLAMBDA_N:
				printf("Lambda function n: %ld\n", (intptr_t)node->value);
				break;
			case CLAMBDA_ARG:
				printf("Lambda arg ptr: %p\n", node->value);
				break;

			//pointers
			case CWEAK_PTR:
				printf("Weak pointer: %p\n", node->value);
				break;
			case CSOFT_PTR:
				printf("Soft pointer: %p\n", node->value);
				break;	
			case CPHANTOM_PTR_NF:
				printf("Phantom ptr(not-finlized): %p\n", node->value);
				break;	
			case CPHANTOM_PTR_F:
				printf("Phantom ptr(finalized): %p\n", node->value);
				break;	

			//big data
			case CBDATA_PTR:
				printf("Big data ptr: %p\n", node->value);
				break;	
			case CBDATA_HEAD_N:
				printf("Big data n: %ld\n", (intptr_t)node->value);
				break;	
			case CBDATA_HEAD_C:
				printf("Big data c: %ld\n", (intptr_t)node->value);
				break;	
			case CBDATA_ND:
				printf("Big data node ptr: %p\n", node->value);
				break;	

			default:
				printf("\n");
		}
		node++;
	}
}

void print_roots()
{
	if(roots_i==0)
		return;
	printf("roots[%d]:\n", roots_i);
	for(int i=0; i<roots_i; i++)
		printf("Root -> %p\n", roots[i]);
}

void print_other_ptrs()
{
	if(other_ptr_i==0)
		return;
	printf("soft/weak pointers to check[%d]:\n", other_ptr_i);
	for(int i=0; i<other_ptr_i; i++){
		if(other_ptrs[i]->tag == CWEAK_PTR)
			printf("Weak ptr -> %p\n", other_ptrs[i]);
		if(other_ptrs[i]->tag == CSOFT_PTR)
			printf("Soft ptr -> %p\n", other_ptrs[i]);
	}
}

void print_finalized_ptrs()
{
	if(finalized_ptr_i==0)
		return;
	printf("\nMutator:\nfinialized pointers[%d]:\n", finalized_ptr_i);
	for(int i=0; i<finalized_ptr_i; i++)
		printf("Finilized ptr -> %p\n", finalized_ptrs[i]);
}

void print_big_data()
{
	if(big_data_i==0)
		return;
	printf("big data:\n");
	for(int i=0; i<big_data_i; i++){
		printf("Big data -> %p\n", big_data[i]);
		Node *head_node = big_data[i]->value;
		Node *end_node = head_node + (intptr_t)(head_node->value) + 2;
		print_nodes(head_node, end_node);
	}
}


//------------------------------------------------------------------------------------------
// Test cases
//------------------------------------------------------------------------------------------


void test_case1() //traversing roots
{
	add_root(add_int(10));
	add_root(add_int(30));
	add_int(20);
}

void test_case2() //scavenging
{
	Node *ptr = add_int(10);
	add_root(add_ptr(ptr));
	add_root(add_int(20));
	add_root(add_ptr(ptr));
}

void test_case3() //string data
{
	add_root(add_str("foo"));
	add_str("bar");
}

void test_case4() //range data
{
	add_root(add_range(add_int(10), add_int(20)));
}

void test_case5() //data
{
	Node* nodes[5];
	for(int i=0; i<5; i++)
		nodes[i] = add_int(10 +i);
	add_root(add_data(7, 5, nodes));
}

void test_case6() //bool
{
	add_bool(false);
	add_root(add_bool(true));
}

void test_case7() //lambda expression
{
	Node* nodes[5];
	for(int i=0; i<5; i++)
		nodes[i] = add_int(10 +i);
	add_root(add_lambda(7, 5, nodes));
}

void test_case8() //weak pointers
{
	Node *node1 = add_int(10);
	Node *node2 = add_int(11);
	Node *node3 = add_int(12);
	add_root(add_weak_ptr(node1));
	add_root(node1);
	add_root(add_weak_ptr(node3));
}

void test_case9() //soft pointers
{
	int soft_ptrs_n = (HEAP_SIZE-LOW_MEM_THRESHOLD)/2 + 1;
	for(int i=0; i<soft_ptrs_n; i++){
		add_root(add_soft_ptr(add_int(i)));
	}
}

void test_case10() //phantom pointers
{
	Node *node1 = add_int(10);
	Node *node2 = add_int(11);
	Node *phantom_node = add_phantom_ptr(node1);
	finalize_ptr(phantom_node);
	add_root(phantom_node);
	add_root(add_phantom_ptr(node2));
}

void test_case11() // big data
{
	Node* nodes[5];
	for(int i=0; i<5; i++)
		nodes[i] = add_int(10 +i);
	add_root(add_big_data(5, 7, nodes));
	add_big_data(5, 7, nodes);
}


//------------------------------------------------------------------------------------------
// Test heaps
//------------------------------------------------------------------------------------------


//adding element at the beginning of linked list
void test_heap1_start()
{
	add_str("foo");
	add_str("bar");
	add_bool(true);
	Node* nodes[] = {add_int(10), add_node(CNULL, NULL)};
	add_root(add_data(1, 2, nodes));
}
void test_heap1_continue()
{
	add_str("foo");
	add_str("bar");
	add_bool(true);
	Node* nodes[] = {add_int(10), add_ptr(roots[roots_i-1])};
	roots[roots_i-1] = add_data(1, 2, nodes);
}

// calculating something in loop(using range)
// building lambda function
// keeping result of this lambda function and arguments as big data
// phantom pointer to that big data
void test_heap2_start()
{
	add_range(add_int(0), add_int(5));
	Node* nodes[6];
	for(int i=0; i<5; i++)
		nodes[i] = add_int(10 +i);
	add_lambda(7, 5, nodes);
	nodes[5] = add_int(100);
	Node *big_data = add_big_data(6, 5, nodes);
	add_root(add_phantom_ptr(big_data));
}
void test_heap2_continue()
{
	finalize_ptr(roots[roots_i-1]);
	test_heap2_start();
}

//caching results if there is space available
void test_heap3_start()
{
	Node *ptr = add_str("Cached data");
	add_root(add_soft_ptr(ptr));
	add_root(add_weak_ptr(ptr));
}
void test_heap3_continue()
{
	test_heap3_start();
}
