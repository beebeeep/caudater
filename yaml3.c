#include <stdio.h>
#include <yaml.h>

typedef struct elem_t {
    void *value;
    struct elem_t *parent;
} elem;

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

    elem *top_elem = (elem *)malloc(sizeof(elem));
    top_elem->value = NULL; top_elem->parent = NULL; 
    elem *curr_elem = top_elem;

    elem *parent_stack = push(top_elem, NULL);

    /* START new code */
    do {
        if (!yaml_parser_parse(&parser, &event)) {
            printf("Parser error %d\n", parser.error);
            exit(EXIT_FAILURE);
        }

        switch(event.type)
        { 
            case YAML_NO_EVENT: puts("No event!"); break;
                                /* Stream start/end */
            case YAML_STREAM_START_EVENT: puts("STREAM START"); break;
            case YAML_STREAM_END_EVENT:   puts("STREAM END");   break;
                                          /* Block delimeters */
            case YAML_DOCUMENT_START_EVENT: puts("<b>Start Document</b>"); break;
            case YAML_DOCUMENT_END_EVENT:   puts("<b>End Document</b>");   break;
            case YAML_SEQUENCE_START_EVENT: 
                                            /* we don't need arrays */
                                            puts("<b>Start Sequence</b>"); 
                                            break;
            case YAML_SEQUENCE_END_EVENT:   
                                            /* we don't need arrays */
                                            puts("<b>End Sequence</b>");
                                            break;
            case YAML_MAPPING_START_EVENT:  
                                            printf("Current elem %p pushed to %p\n", curr_elem, parent_stack);
                                            parent_stack = push(curr_elem, parent_stack);
                                            puts("<b>Start Mapping</b>");
                                            break;
            case YAML_MAPPING_END_EVENT:    
                                            curr_elem = pop(&parent_stack);
                                            printf("Current elem %p was popped from %p\n", curr_elem, parent_stack);
                                            puts("<b>End Mapping</b>");    break;
                                            /* Data */
            case YAML_ALIAS_EVENT:  
                                            /* i don't know what is this shit */
                                            printf("Got alias (anchor %s)\n", event.data.alias.anchor); break;
            case YAML_SCALAR_EVENT: 
                                            elem *new = (elem *)malloc(sizeof(elem));
                                            new->value = malloc(event.data.scalar.length);
                                            memcpy(new->value, event.data.scalar.value, event.data.scalar.length);
                                            new->parent = curr_elem;
                                            curr_elem = new; 

                                            printf("Got scalar (value %s)\n", event.data.scalar.value); 
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
    return 0;
}

