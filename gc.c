#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
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
	void *value;
} Node;

Node heap[2*HEAP_SIZE] = {0};
int from_hp = 0, to_hp = HEAP_SIZE;

Node *roots[ROOTS_N], *other_ptrs[ROOTS_N], *finilized_ptrs[ROOTS_N];
int roots_i=0, other_ptr_i=0, finilized_ptr_i=0;

void gc();
Node* evacuate(Node *node);
void traverse_roots();
int traverse_other_ptrs();
void scaveneging();
void look_back();

Node* add_node(char type, void* value);
Node* add_bool(bool value);
Node* add_int(intptr_t number);
Node* add_ptr(Node *ptr);
Node* add_weak_ptr(Node *node);
Node* add_str(char[]);
Node* add_range(Node* node1, Node* node2);
Node* add_data(intptr_t c, int n, Node **nodes);
Node* add_lambda(intptr_t id, intptr_t n, Node **nodes);

void add_root(Node *ptr);
void finilize_ptr(Node *node);

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

Node* evacuate(Node *node)
{
	if(node->tag == CFWD_PTR)	//check whether already evacuated
		return node->value;
	else {
		//evacuate
		heap[to_hp].tag = node->tag;
		heap[to_hp].value = node->value;
		node->tag = CFWD_PTR;
		node->value = &(heap[to_hp++]);

		Node *head_node = node->value;

		//data, which occupies more than one node

		//ranges
		if(head_node->tag == CRANGE){
			Node *next_node = (node + 1);
			heap[to_hp].tag = next_node->tag;
			heap[to_hp++].value = next_node->value;
		}

		//data
		if(head_node->tag == CDATA_HEAD){
			Node *next_node = (node + 1);
			while(next_node->tag == CDATA_ND){
				heap[to_hp].tag = next_node->tag;
				heap[to_hp++].value = next_node->value;
				next_node = (next_node + 1);
			}
		}

		//lambda
		if(head_node->tag == CLAMBDA_ID){
			Node *next_node = (node + 1);
			int end = (intptr_t)next_node->value, i;
			heap[to_hp].tag = next_node->tag;
			heap[to_hp++].value = next_node->value;
			for(i=0; i<end; i++){
				next_node = (next_node + 1);
				heap[to_hp].tag = next_node->tag;
				heap[to_hp++].value = next_node->value;
			}
		}

		return head_node;
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
	for(i=0; i<other_ptr_i; i++){
		Node *node = (Node*)(other_ptrs[i]->value);
		if(node->tag == CFWD_PTR) //something else was referring to that node
			other_ptrs[i]->value = node->value;
		else{
			int left_mem = 2 * HEAP_SIZE - to_hp;
			if(other_ptrs[i]->tag == CSOFT_PTR && left_mem > LOW_MEM_THRESHOLD){
				other_ptrs[i]->value = evacuate(node);
			}else
				other_ptrs[i]-> value = CNULL;
		}
	}
	return old_to_hp;
};

void scaveneging(int i)
{
	while(i<to_hp){
		if(heap[i].tag == CPTR || heap[i].tag == CRANGE || heap[i].tag == CDATA_ND 
		   || heap[i].tag == CPHANTOM_PTR_NF || heap[i].tag == CLAMBDA_ARG)
			heap[i].value = evacuate(heap[i].value);

		//weak/soft pointers
		if(heap[i].tag == CWEAK_PTR || heap[i].tag == CSOFT_PTR){
			if(((Node*)heap[i].value)->tag == CFWD_PTR)
				heap[i].value = evacuate(heap[i].value);
			else
				other_ptrs[other_ptr_i++] = &(heap[i]);
		}

		//finilized phantom pointers
		if(heap[i].tag == CPHANTOM_PTR_F){
			heap[i].value = CNULL;
			finilized_ptrs[finilized_ptr_i++] = &(heap[i]);
		}
		i++;
	}
};

void look_back()
{
	int i = 0;
	while(i<from_hp){
		if(heap[i].tag == CCONST_STR){
			free(heap[i].value);
			heap[i].value = CNULL;
		}
		i++;
	}
};


//------------------------------------------------------------------------------------------
// Initializing data
//------------------------------------------------------------------------------------------


Node* add_int(intptr_t number)
{
	return add_node(CCONST_INT, (void *)number);
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

Node* add_phantom_ptr(Node *node){
	return add_node(CPHANTOM_PTR_NF, node);
}

Node* add_str(char *str)
{
	return add_node(CCONST_STR, (void *)strdup(str));	
}

Node* add_range(Node* node1, Node* node2)
{
	Node *head_node = add_node(CRANGE, node1);
	add_node(CRANGE, node2);
	return head_node;
}

Node* add_data(intptr_t c, int n, Node **nodes)
{
	int i;
	Node *head_node = add_node(CDATA_HEAD, (void*) c);
	for(i=0; i<n; i++)
		add_node(CDATA_ND, nodes[i]);
	return head_node;
}

Node* add_lambda(intptr_t id, intptr_t n, Node **nodes)
{
	int i;
	Node *head_node=add_node(CLAMBDA_ID, (void*)id);
	add_node(CLAMBDA_N, (void *)n);
	for(i=0; i<n; i++)
		add_node(CLAMBDA_ARG, nodes[i]);
	return head_node;
}

Node* add_bool(bool value)
{
	return add_node(CCONST_BOOL, (void *)value);	
}

Node* add_node(char type, void *value)
{
	Node node = {type, value};
	heap[from_hp] = node;
	return &(heap[from_hp++]);
}

void add_root(Node* ptr)
{
	roots[roots_i] = ptr;
	roots_i++;
}

void finilize_ptr(Node* node)
{
	node->tag = CPHANTOM_PTR_F;
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
		printf("%d:%p:", i, &(heap[i]));
		switch(heap[i].tag){
			case CFWD_PTR:
				printf("Forward ptr: %p\n", heap[i].value);
				break;
			case CCONST_INT:
				printf("INTEGER: %ld\n", (intptr_t)heap[i].value);
				break;
			case CPTR:
				printf("POINTER: %p\n", heap[i].value);
				break;
			case CCONST_STR:
				printf("String: %s\n", (char *)heap[i].value);
				break;
			case CRANGE:
				printf("Range: %p %p\n", heap[i].value, heap[i+1].value);
				i++;
				break;
			case CDATA_HEAD:
				printf("Data constructor: %ld, ", (intptr_t)heap[i].value);
				while(heap[++i].tag == CDATA_ND)
					printf("node: %p, ", heap[i].value);
				i--;
				printf("\n");
				break;
			case CCONST_BOOL:
				if((bool)(heap[i].value))
					printf("Boolean: true\n");
				else
					printf("Boolean: false\n");
				break;
			case CLAMBDA_ID:
				printf("Lambda function id: %ld, ", (intptr_t)heap[i].value);
				printf("n: %ld, ", (intptr_t)heap[++i].value);
				int end = i + (intptr_t)heap[i].value;
				i++;
				for(; i<=end; i++)
					printf("arg ptr: %p, ", heap[i].value);
				i--;
				printf("\n");
				break;
			case CWEAK_PTR:
				printf("Weak pointer: %p\n", heap[i].value);
				break;
			case CSOFT_PTR:
				printf("Soft pointer: %p\n", heap[i].value);
				break;	
			case CPHANTOM_PTR_NF:
				printf("Phantom ptr(not-finlized): %p\n", heap[i].value);
				break;	
			case CPHANTOM_PTR_F:
				printf("Phantom ptr(finilized): %p\n", heap[i].value);
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
		printf("Root -> %p\n", roots[i]);
}

void print_other_ptrs()
{
	int i;
	for(i=0; i<other_ptr_i; i++){
		if(other_ptrs[i]->tag == CWEAK_PTR)
			printf("Weak ptr -> %p\n", other_ptrs[i]);
		if(other_ptrs[i]->tag == CSOFT_PTR)
			printf("Soft ptr -> %p\n", other_ptrs[i]);
	}
}

void print_finilized_ptrs()
{
	int i;
	for(i=0; i<finilized_ptr_i; i++)
		printf("Finilized ptr -> %p\n", finilized_ptrs[i]);
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
	Node *ptr = add_int(10);
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
	add_root(add_range(add_int(10), add_int(20)));
}

void test_case5() //data
{
	int i;
	Node* nodes[5];
	for(i=0; i<5; i++)
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
	int i;
	Node* nodes[5];
	for(i=0; i<5; i++)
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
	int i;
	for(i=0; i<7; i++){
		add_root(add_soft_ptr(add_int(10 +i)));
	}
}

void test_case10() //phantom pointers
{
	Node *node1 = add_int(10);
	Node *node2 = add_int(11);
	Node *phantom_node = add_phantom_ptr(node1);
	finilize_ptr(phantom_node);
	add_root(phantom_node);
	add_root(add_phantom_ptr(node2));
	
}