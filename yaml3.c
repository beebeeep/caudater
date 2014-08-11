#include <stdio.h>
#include <yaml.h>

#define MAX_ELEM_COUNT 1024

/* struct for keeping tree elements parent stack */
typedef struct elem_t {
    void *value;
    struct elem_t *parent;
} elem;

typedef struct kv_elem_t {
    char *key;
    char *value;
    struct kv_elem_t *parent;
} kv_elem;

/* takes element to push and stack head poiner, return new stack head */
elem *push(void *m, elem *stack) 
{
    elem *new_elem = (elem *)malloc(sizeof(elem));
    new_elem->parent = stack;
    new_elem->value = m;
    return new_elem;
}

/* returns value stored on stack head, changes stack head to prev elem */
void *pop(elem **stack)
{
    elem *m = *stack;
    *stack = m->parent;
    void *value = m->value;
    free(m);
    return value;
}

void get_elem_by_parent_value(elem *tree[], size_t count, char *value)
{
    size_t i;
    for (i = 0; i < count; i++) {
        if(tree[i]->parent == NULL) continue;
        if(!strcmp(value, tree[i]->parent->value)) {
            printf("Found elem %s for parent %s\n", tree[i]->value, value);
        }
    }
}


int main(void)
{
    FILE *fh = fopen("cfg.yaml", "r");
    yaml_parser_t parser;
    yaml_event_t  event;   /* New variable */

    /* Initialize parser */
    if(!yaml_parser_initialize(&parser))
        fputs("Failed to initialize parser!\n", stderr);
    if(fh == NULL)
        fputs("Failed to open file!\n", stderr);

    /* Set input file */
    yaml_parser_set_input_file(&parser, fh);

    kv_elem *top_elem = (kv_elem *)malloc(sizeof(elem));
    top_elem->key = "HEAD"; top_elem->value = NULL; top_elem->parent = NULL; 
    kv_elem *curr_elem = top_elem;

    elem *parent_stack = push(top_elem, NULL);

    kv_elem *tree[MAX_ELEM_COUNT];
    tree[0] = top_elem;
    unsigned elem_count = 1;

    unsigned kv_parity = 0;

    /* START new code */
    do {
        if (!yaml_parser_parse(&parser, &event)) {
            printf("Parser error %d\n", parser.error);
            exit(EXIT_FAILURE);
        }

        switch(event.type)
        { 
            case YAML_NO_EVENT:
            case YAML_STREAM_START_EVENT:
            case YAML_STREAM_END_EVENT:
            case YAML_DOCUMENT_START_EVENT:
            case YAML_DOCUMENT_END_EVENT:
            case YAML_SEQUENCE_START_EVENT: 
            case YAML_SEQUENCE_END_EVENT:   
            case YAML_ALIAS_EVENT:  
                break;
            case YAML_MAPPING_START_EVENT:  
                                            kv_parity = 0;
                                            parent_stack = push(curr_elem, parent_stack);
                                            break;
            case YAML_MAPPING_END_EVENT:    
                                            kv_parity = 0;
                                            curr_elem = pop(&parent_stack);
                                            break;
            case YAML_SCALAR_EVENT: 
                                            if(kv_parity++ % 2 == 0) {      /* this is a key */
                                                kv_elem *new_elem = (kv_elem *)malloc(sizeof(kv_elem));
                                                new_elem->key = malloc(event.data.scalar.length);
                                                new_elem->value = NULL;
                                                memcpy(new_elem->key, event.data.scalar.value, event.data.scalar.length+1);
                                                new_elem->parent = parent_stack->value;
                                                tree[elem_count++] = new_elem; 
                                                curr_elem = new_elem; 
                                            } else {    /* this is a value */
                                                curr_elem->value = malloc(event.data.scalar.length);
                                                memcpy(curr_elem->value, event.data.scalar.value, event.data.scalar.length+1);
                                            }


                                            break;
        }
        if(event.type != YAML_STREAM_END_EVENT)
            yaml_event_delete(&event);
    } while(event.type != YAML_STREAM_END_EVENT);
    yaml_event_delete(&event);
    /* END new code */

    /* Cleanup */
    yaml_parser_delete(&parser);
    fclose(fh);

    size_t i;
    for (i = 0; i < elem_count; i++) { 
        kv_elem *c = tree[i];
        printf("Elem %p:\tkey=%s, value=%s, parent = %s\n", c, c->key, (c->value != NULL)?c->value:"NULL", (c->parent != NULL)?c->parent->key:"NULL");
    }
    return 0;
}

