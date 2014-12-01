#include "umsections.h"
#include "table.h"
#include "uarray.h"
#include "mem.h"

#include <stdlib.h>
#include <stdbool.h>

struct Umsections_T 
{
        Table_T table;       
        char* current;
        int (*error)(void*, const char*);
        void* errstate;
};


/*------------helpers---------*/
unsigned hash(unsigned char *str)
{
        unsigned long hash = 1039;
        int c;

        while ((c = *str++))
                hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

        return hash;    
}

bool section_exists(Umsections_T asm, const char* name)
{
        return (Table_get(asm->table, name) != NULL);
}

void write_section(UArray_T sections, FILE* output)
{
        for (int i = 0; i < UArray_length(sections); i++)
                fputc(*(int*)UArray_at(sections, i), output);
}

typedef void Apply(const char *name, void *cl);

struct table_cl {
        Apply *apply;
        void *cl;
};

static void table_apply(const char* key, void ** val, void * cl)
{
        struct table_cl *tcl = cl;
        (void)val;
        tcl->apply(key, tcl->cl);
}



/* -----------real stuff--------------*/

Umsections_T Umsections_new(const char *section, 
            int (*error)(void *errstate, const char *message),
            void *errstate)
{
        Umsections_T to_return = malloc(sizeof(struct Umsections_T));
        
        to_return->error = error;
        to_return->errstate = errstate;

        to_return->table = Table_new(5, NULL, NULL);
        Table_put(to_return->table, section, NULL);
        to_return->current = NULL;

        return to_return;
}

void Umsections_free(Umsections_T *asmp)
{
        FREE(asmp);
}

// call the assembler's error function, using the error state
//   passed in at creation time 
int Umsections_error(Umsections_T asm, const char *msg)
{
        return asm->(*error)(asm->errstate, msg);
}


void Umsections_section(Umsections_T asm, const char *section)
{
        if (!Table_get(asm->table, section))
                *asm->current = *section;
        else {
                Table_put(asm->table, section, NULL);
                *asm->current = *section;
        }
                
}

void Umsections_emit_word(Umsections_T asm, Umsections_word data)
{
        UArray_T sections = Table_get(asm->table, asm->current);
        *(Umsections_word*)UArray_at(sections, UArray_length(sections) + 1) 
        = data;
}

void Umsections_map(Umsections_T asm, void apply(const char *name, void *cl),
                        void *cl)
{
        struct table_cl mycl = { apply, cl };
        Table_map(asm->table, table_apply, &mycl);
}

int Umsections_length(Umsections_T asm, const char *name)
{
        int to_return = 0;
        if (!section_exists(asm, name))
                Umsections_error(asm, "ERROR!\n");
        else
                to_return = UArray_length(Table_get(asm->table, name));

        return to_return;
}

Umsections_word Umsections_getword(Umsections_T asm, const char *name, int i)
{
        if (!section_exists(asm, name) || i >= Umsections_length(asm, name))
                Umsections_error(asm, "ERROR!\n");
        
        return *(Umsections_word*)UArray_at(Table_get(asm->table, name), i);
}

void Umsections_putword(Umsections_T asm, const char *name, int i, Umsections_word w)
{
        if (!section_exists(asm, name) || i >= Umsections_length(asm, name))
                Umsections_error(asm, "ERROR!\n");

        *(Umsections_word*)UArray_at(Table_get(asm->table, name), i) = w;
}

void Umsections_write(Umsections_T asm, FILE *output)
{
        Table_map(asm->table, write_section, NULL);
}