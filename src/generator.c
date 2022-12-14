#include <vslc.h>

void generate_stringtable ( void );
void generate_global_variables ( void );
void generate_function ( symbol_t *function );

static void generate_node ( node_t *node );
void generate_main ( symbol_t *first );
static void generate_function_call ( node_t *call );

#define MIN(a,b) (((a)<(b)) ? (a):(b))

static const char *record[6] = {
    "%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9"
};

static symbol_t *current_function = NULL;
int counter_if = 0;
int counter_while = 0;


void
generate_program ( void )
{

    size_t n_globals = tlhash_size(global_names);
    symbol_t *global_list[n_globals];
    tlhash_values ( global_names, (void **)&global_list );

    symbol_t *first_function;
    for ( size_t i=0; i<tlhash_size(global_names); i++ )
        if ( global_list[i]->type == SYM_FUNCTION )
        {
            // Allows the use of main as name to override entry point
            if (!strcmp(global_list[i]->name, "main")) {
                first_function = global_list[i];
                break;
            }
            else if (global_list[i]->seq == 0) {
                first_function = global_list[i];
            }
        }

    generate_stringtable();
    generate_global_variables();
    generate_main ( first_function );
    for ( size_t i=0; i<tlhash_size(global_names); i++ )
        if ( global_list[i]->type == SYM_FUNCTION )
            generate_function ( global_list[i] );

}


void
generate_stringtable ( void )
{
    puts ( ".section .rodata" );
    puts ( ".intout: .string \"\%ld \"" );
    puts ( ".strout: .string \"\%s \"" );
    puts ( ".errout: .string \"Wrong number of arguments\"" );
    for ( size_t s=0; s<stringc; s++ )
        printf ( ".STR%zu: .string %s\n", s, string_list[s] );
}


void
generate_global_variables ( void )
{
    puts ( ".section .data" );
    size_t nsyms = tlhash_size ( global_names );
    symbol_t *syms[nsyms];
    tlhash_values ( global_names, (void **)&syms );
    for ( size_t n=0; n<nsyms; n++ )
    {
        if ( syms[n]->type == SYM_GLOBAL_VAR )
            printf ( "._%s: .zero 8\n", syms[n]->name );
    }
}


void
generate_main ( symbol_t *first )
{
    puts ( ".globl main" );
    puts ( ".section .text" );
    puts ( "main:" );
    puts ( "\tpushq   %rbp" );
    puts ( "\tmovq    %rsp, %rbp" );

    printf ( "\tsubq\t$1,%%rdi\n" );
    printf ( "\tcmpq\t$%zu,%%rdi\n", first->nparms );
    printf ( "\tjne\tABORT\n" );
    printf ( "\tcmpq\t$0,%%rdi\n" );
    printf ( "\tjz\tSKIP_ARGS\n" );

    printf ( "\tmovq\t%%rdi,%%rcx\n" );
    printf ( "\taddq $%zu, %%rsi\n", 8*first->nparms );
    printf ( "PARSE_ARGV:\n" );
    printf ( "\tpushq %%rcx\n" );
    printf ( "\tpushq %%rsi\n" );

    printf ( "\tmovq\t(%%rsi),%%rdi\n" );
    printf ( "\tmovq\t$0,%%rsi\n" );
    printf ( "\tmovq\t$10,%%rdx\n" );
    printf ( "\tcall\tstrtol\n" );

    /*  Now a new argument is an integer in rax */

    printf ( "\tpopq %%rsi\n" );
    printf ( "\tpopq %%rcx\n" );
    printf ( "\tpushq %%rax\n" );
    printf ( "\tsubq $8, %%rsi\n" );
    printf ( "\tloop PARSE_ARGV\n" );

    /* Now the arguments are in order on stack */
    for ( int arg=0; arg<MIN(6,first->nparms); arg++ )
        printf ( "\tpopq\t%s\n", record[arg] );

    printf ( "SKIP_ARGS:\n" );
    printf ( "\tcall\t_%s\n", first->name );
    printf ( "\tjmp\tEND\n" );
    printf ( "ABORT:\n" );
    printf ( "\tmovq\t$.errout, %%rdi\n" );
    printf ( "\tcall puts\n" );

    printf ( "END:\n" );
    puts ( "\tmovq    %rax, %rdi" );
    puts ( "\tcall    exit" );

}


static void
generate_identifier ( node_t *ident )
{
    symbol_t *symbol = ident->entry;
    int64_t argument_offset;
    switch ( symbol->type )
    {
        case SYM_GLOBAL_VAR:
            /* Global variables called by name */
            printf ( "._%s", symbol->name );
            break;
        case SYM_PARAMETER:
            if ( symbol->seq > 5 )
                /* Extra parameters pushed in decreasing order */
                printf ( "%ld(%%rbp)", 8+8*(symbol->seq-5) );
            else
                /* First six parameters directly after base poiter */
                printf ( "%ld(%%rbp)", -8*(symbol->seq+1) );
            break;
        case SYM_LOCAL_VAR:
            /* Local variables places after parameters in stack */
            argument_offset = -8*MIN(6,current_function->nparms);
            printf ( "%ld(%%rbp)", -8*(symbol->seq+1) + argument_offset );
            break;
    }
}

static void 
generate_identifier_with_str( node_t *ident, char str[])
{
    symbol_t *symbol = ident->entry;
    int64_t argument_offset;
    char append[64];
    switch ( symbol->type )
    {
        case SYM_GLOBAL_VAR:
            /* Global variables called by name */
            strcat(str, "._");
            strcat(str, symbol->name);
            break;
        case SYM_PARAMETER:
            if ( symbol->seq > 5 ){
                /* Extra parameters pushed in decreasing order */
                int v = 8 + 8 * (symbol->seq-5);
                sprintf(append, "%d", v);
                strcat(str, append);
                strcat(str, "(%rbp)");

            }else{
                /* First six parameters directly after base poiter */
                int v = -8 * (symbol->seq+1);
                sprintf(append, "%d", v);
                strcat(str, append);
                strcat(str, "(%rbp)");
            }
            break;
        case SYM_LOCAL_VAR:
            /* Local variables places after parameters in stack */
            argument_offset = -8*MIN(6,current_function->nparms);
            int v = -8 *(symbol->seq+1) + argument_offset;
            sprintf(append, "%d", v);
            strcat(str, append);
            strcat(str, "(%rbp)");
            break;
    }
}


static void
generate_expression ( node_t *expr )
{
    if ( expr->type == IDENTIFIER_DATA )
    {
        printf ( "\tmovq\t" );
        generate_identifier ( expr );
        printf ( ", %%rax\n" );
    }
    else if ( expr->type == NUMBER_DATA )
    {
        printf ( "\tmovq\t$%ld, %%rax\n", *(int64_t *)expr->data );
    }
    else if ( expr->n_children == 1 )
    {
        switch ( *((char*)(expr->data)) )
        {
            case '-':
                generate_expression ( expr->children[0] );
                printf ( "\tnegq\t%%rax\n" );
                break;
            case '~':
                generate_expression ( expr->children[0] );
                printf ( "\tnotq\t%%rax\n" );
                break;
        }
    }
    else if ( expr->n_children == 2 )
    {
        if ( expr->data != NULL )
        {
            switch ( *((char *)expr->data) )
            {
                case '+':
                    generate_expression ( expr->children[0] );
                    printf ( "\tpushq\t%%rax\n" );
                    generate_expression ( expr->children[1] );
                    printf ( "\taddq\t%%rax, (%%rsp)\n" );
                    printf ( "\tpopq\t%%rax\n" );
                    break;
                case '-':
                    generate_expression ( expr->children[0] );
                    printf ( "\tpushq\t%%rax\n" );
                    generate_expression ( expr->children[1] );
                    printf ( "\tsubq\t%%rax, (%%rsp)\n" );
                    printf ( "\tpopq\t%%rax\n" );
                    break;
                case '*':
                    printf ( "\tpushq\t%%rdx\n" );
                    generate_expression ( expr->children[1] );
                    printf ( "\tpushq\t%%rax\n" );
                    generate_expression ( expr->children[0] );
                    printf ( "\tmulq\t(%%rsp)\n" );
                    printf ( "\tpopq\t%%rdx\n" );
                    printf ( "\tpopq\t%%rdx\n" );
                    break;
                case '/':
                    printf ( "\tpushq\t%%rdx\n" );
                    generate_expression ( expr->children[1] );
                    printf ( "\tpushq\t%%rax\n" );
                    generate_expression ( expr->children[0] );
                    printf ( "\tcqo\n" );
                    printf ( "\tidivq\t(%%rsp)\n" );
                    printf ( "\tpopq\t%%rdx\n" );
                    printf ( "\tpopq\t%%rdx\n" );
                    break;
                case '|':
                    generate_expression ( expr->children[0] );
                    printf ( "\tpushq\t%%rax\n" );
                    generate_expression ( expr->children[1] );
                    printf ( "\torq\t%%rax, (%%rsp)\n" );
                    printf ( "\tpopq\t%%rax\n" );
                    break;
                case '^':
                    generate_expression ( expr->children[0] );
                    printf ( "\tpushq\t%%rax\n" );
                    generate_expression ( expr->children[1] );
                    printf ( "\txorq\t%%rax, (%%rsp)\n" );
                    printf ( "\tpopq\t%%rax\n" );
                    break;
                case '&':
                    generate_expression ( expr->children[0] );
                    printf ( "\tpushq\t%%rax\n" );
                    generate_expression ( expr->children[1] );
                    printf ( "\tandq\t%%rax, (%%rsp)\n" );
                    printf ( "\tpopq\t%%rax\n" );
                    break;
            }
        } else {
            generate_function_call ( expr );
        }
    }
}


static void
generate_function_call ( node_t *call )
{
    /* Check function call */
    size_t n_arguments = 0;
    if ( call->children[1] != NULL )
        n_arguments = call->children[1]->n_children;
    symbol_t *function = call->children[0]->entry;
    if ( n_arguments != function->nparms )
    {
        fprintf ( stderr,
            "Function %s has %zu parameters, called with %zu arguments\n",
            (char *) call->children[0]->data,
            (size_t) call->children[0]->entry->nparms,
            n_arguments
        );
        exit ( EXIT_FAILURE );
    }

    /* Generate function call: */

    /* Push all the arguments */
    node_t *arglist = call->children[1];
    if ( arglist != NULL )
    {
        for ( size_t p=arglist->n_children; p>0; p-- )
        {
            generate_expression ( arglist->children[(p-1)] );
            if ( (p-1)>5 )
                printf ( "\tpushq\t%%rax\n" );
            else
                printf ( "\tmovq\t%%rax, %s\n", record[(p-1)] );
        }
    }
    /* Call the function */
    printf ( "\tcall _%s\n", (char *)call->children[0]->data );
}


static void
generate_assignment_statement ( node_t *statement )
{
    switch ( statement->type )
    {
        case ASSIGNMENT_STATEMENT:
            generate_expression ( statement->children[1] );
            printf ( "\tmovq\t%%rax, " );
            generate_identifier ( statement->children[0] );
            printf ( "\n" );
            break;
        case ADD_STATEMENT:
            generate_expression ( statement->children[1] );
            printf ( "\taddq\t%%rax, " );
            generate_identifier ( statement->children[0] );
            printf ( "\n" );
            break;
        case SUBTRACT_STATEMENT:
            generate_expression ( statement->children[1] );
            printf ( "\tsubq\t%%rax, " );
            generate_identifier ( statement->children[0] );
            printf ( "\n" );
            break;
        case MULTIPLY_STATEMENT:
            generate_expression ( statement->children[1] );
            printf ( "\tmulq\t " );
            generate_identifier ( statement->children[0] );
            printf ( "\n" );
            printf ( "\tmovq\t%%rax, " );
            generate_identifier ( statement->children[0] );
            printf ( "\n" );
            break;
        case DIVIDE_STATEMENT:
            generate_expression ( statement->children[1] );
            printf ( "\txchgq\t%%rax, " );
            generate_identifier ( statement->children[0] );
            printf ( "\n" );
            printf ( "\tcqo\n" );
            printf ( "\tidivq\t" );
            generate_identifier ( statement->children[0] );
            printf ( "\n" );
            printf ( "\txchgq\t%%rax, " );
            generate_identifier ( statement->children[0] );
            printf ( "\n" );
            break;
    }
}


static void
generate_print_statement ( node_t *statement )
{
    for ( size_t i=0; i<statement->n_children; i++ )
    {
        node_t *item = statement->children[i];
        switch ( item->type )
        {
            case STRING_DATA:
                printf ( "\tmovq\t$.STR%zu, %%rsi\n", *((size_t *)item->data) );
                printf ( "\tmovq\t$.strout, %%rdi\n" );
                break;
            case NUMBER_DATA:
                printf ("\tmovq\t$%ld, %%rsi\n", *((int64_t *)item->data) );
                printf ( "\tmovq\t$.intout, %%rdi\n" );
                break;
            case IDENTIFIER_DATA:
                printf ( "\tmovq\t" );
                generate_identifier ( item );
                printf ( ", %%rsi\n" );
                printf ( "\tmovq\t$.intout, %%rdi\n" );
                break;
            case EXPRESSION:
                generate_expression ( item );
                printf ( "\tmovq\t%%rax, %%rsi\n" );
                printf ( "\tmovq\t$.intout, %%rdi\n" );
                break;
        }
        puts (   "\tmovq\t$0, %rax\n"       // Clear rax to indicate not to use SSE instructions
                    "\tcall\tprintf" );
    }
    printf ( "\tmovq\t$0x0A, %%rdi\n" );    // Finish statement by inserting a newline
    puts ( "\tcall\tputchar" );
}

static void 
generate_relation( node_t *relation, uint16_t i ){
    if(relation != NULL){
        
        if(i == 0){ //if i == 0 then if statement
            char* rel = relation->data;
            if(counter_if == 0){
                if( *rel == '='){
                    puts("\tjne .ELSE");
                }
                else if( *rel == '>'){
                    puts("\tjle .ELSE");
                }
                else if(*rel == '<'){
                    puts("\tjge .ELSE");
                }
            
            }else{
                if( *rel == '='){
                    printf("\tjne .ELSE%d\n", counter_if);
                }
                else if( *rel == '>'){
                    printf("\tjle .ELSE%d\n", counter_if);
                }
                else if(*rel == '<'){
                    printf("\tjge .ELSE%d\n", counter_if);
                }
            }
        }

        if(i == 1){ //if i == 1 then while statement
            char* rel = relation->data;
            if( *rel == '='){
                printf("\tjne .ENDWHILE%d\n", counter_while);
            }
            else if( *rel == '>'){
                printf("\tjle .ENDWHILE%d\n", counter_while);
            }
            else if(*rel == '<'){
                printf("\tjge .ENDWHILE%d\n", counter_while);
            }
        }
    }
}

generate_relation_statement(node_t *item, char str[]){
    char newstr[64] = "";
    for(int i = 0; i < item->n_children; i++){
        if(item->children[i]->type == IDENTIFIER_DATA){
            //created new method to generate identifier with a string array
            generate_identifier_with_str(item->children[i], newstr);
        }
        else if(item->children[i]->type == EXPRESSION){
            generate_expression(item->children[i]); //generate expression 
            strcat(newstr, "%rax");
        }
        else if(item->children[i]->type == NUMBER_DATA){
            strcat(str, "$");
            char append[64];
            int v = *((int64_t *)item->children[i]->data);
            sprintf(append, "%d", v); //convert int to string
            strcat(str, append);
        }
    }
    strcat(str, ",");
    strcat(str, newstr);
}


static void
generate_if_statement ( node_t *statement )
{
    for(size_t i = 0; i < statement->n_children; i++){
        node_t *item = statement->children[i];
        
        if(item->type == RELATION){ //if relation generate relation statement
            char str[64] = "\tcmpq\t";
            generate_relation_statement(item, str);
            puts(str);
            generate_relation(item, 0);
        }

        if(i == 1){ //first block
            if(item->type == BLOCK){ //if block go through statement list 
                node_t *statement_list = item->children[0];
                for(int j = 0; j < statement_list->n_children; j++){
                    generate_node(statement_list->children[j]); //generate new nodes in the block
                }
                
                if(counter_if == 0){
                    puts( "\tjmp .ENDIF" );
                    puts( "\t.ELSE:" );
                    counter_if += 1;
                }else{
                    printf("\tjmp .ENDIF%d\n", counter_if);
                    printf( "\t.ELSE%d:\n", counter_if);
                    counter_if += 1;
                }
            
            }else{
                generate_node(item); //if not block generate node
                if(counter_if == 0){
                    puts( "\tjmp .ENDIF" );
                    puts( "\t.ELSE:" );
                    counter_if += 1;
                }else{
                    printf("\tjmp .ENDIF%d\n", counter_if);
                    printf("\t.ELSE%d:\n", counter_if);
                    counter_if += 1;
                }
                
            }
        }

        if(i == 2){ //second block
            if(item->type == BLOCK){ //generate node if there is a block
                node_t *statement_list = item->children[0];
                for(int j = 0; j < statement_list->n_children; j++){
                    generate_node(statement_list->children[j]);
                }
            }
            generate_node(item);
        }
    }
    
    if(counter_if - 1 == 0){
        puts( "\t.ENDIF:" );
    }else{
        printf("\t.ENDIF%d:\n", counter_if - 1);
    }
} 

//fix while counter_if
static void
generate_while_statement ( node_t *statement )
{
    for(size_t i = 0; i < statement->n_children; i++){
        node_t *item = statement->children[i];

        if(item->type == RELATION){ //if relation generate relation statement
            printf("\t.WHILE%d:\n", counter_while);
            char str[64] = "\tcmpq\t";
            generate_relation_statement(item, str);
            puts(str);
            generate_relation(
                item, 1); 
        }
        if(i == 1){ //if first is block then generate node in block
            if(item->type == BLOCK){
                node_t *statement_list = item->children[0];
                for(int j = 0; j < statement_list->n_children; j++){
                    generate_node(statement_list->children[j]);
                }
                
            }else{
                generate_node(item);
            }
            printf("\tjmp .WHILE%d\n", counter_while);
        }

    }
    printf("\t.ENDWHILE%d:\n", counter_while);
    counter_while += 1;
}

static void
generate_node ( node_t *node )
{
    switch (node->type)
    {
        case PRINT_STATEMENT:
            generate_print_statement ( node );
            break;
        case ASSIGNMENT_STATEMENT:
        case ADD_STATEMENT:
        case SUBTRACT_STATEMENT:
        case MULTIPLY_STATEMENT:
        case DIVIDE_STATEMENT:
            generate_assignment_statement ( node );
            break;
        case RETURN_STATEMENT:
            generate_expression ( node->children[0] );
            printf ( "\tleave\n" );
            printf ( "\tret\n" );
            break;
        case IF_STATEMENT:
            generate_if_statement(node);
            break;
        case WHILE_STATEMENT:
            generate_while_statement(node); 
            break;
        case NULL_STATEMENT: //fix jump while
            printf("\tjmp .WHILE%d\n", counter_while);
            break;
        default:
            for ( size_t i=0; i<node->n_children; i++ )
                generate_node ( node->children[i] );
            break;
    }
}


void
generate_function ( symbol_t *function )
{
    current_function = function;
    printf ( "_%s:\n", function->name );
    puts ( "\tpushq   %rbp" );
    puts ( "\tmovq    %rsp, %rbp" );

    /* Save arguments in local stack frame */
    for ( size_t arg=1; arg<=MIN(6,function->nparms); arg++ )
            printf ( "\tpushq\t%s\n", record[arg-1] );
    /* Make space for locals in local stack frame */
    size_t local_vars = tlhash_size(function->locals) - function->nparms;
    if ( local_vars > 0 ) 
        printf ( "\tsubq $%zu, %%rsp\n", 8*local_vars );
    if ( (tlhash_size(function->locals)&1) == 1 )
        puts ( "\tpushq\t$0 /* Stack padding for 16-byte alignment */" );
    generate_node ( function->node );
    printf( "\tmovq\t%%rbp, %%rsp\n"							// 		movq	%rbp, %rsp	// restore stack pointer
            "\tmovq\t$0, %%rax\n"   							//      movq    $0, %rax    // return 0 if nothing else
            "\tpopq\t%%rbp\n"									//      popq	%rbp  		// restore base pointer
            "\tret\n");               							//      ret
    current_function = NULL;
}
