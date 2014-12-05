#include "umsections.h"
#include "table.h"
#include "seq.h"
#include "mem.h"
#include "string.h"
#include "assert.h"
#include "atom.h"
#include "bitpack.h"
#include "fmt.h"

#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>

struct Umsections_T 
{
        Table_T table;       
        const char* current;
        int (*error)(void*, const char*);
	void* errstate;
};


/*------------helpers---------*/
bool section_exists(Umsections_T asm, const char* name)
{
        return (Table_get(asm->table, name) != NULL);
}

typedef void Apply(const char *name, void *cl);

struct table_cl {
        Apply *apply;
        void *cl;
};

void table_apply(const void* key, void ** val, void * cl)
{
        struct table_cl *tcl = cl;
        (void)val;
        tcl->apply((const char*)key, tcl->cl);
}

void table_free(const void *key, void **value, void *cl)
{
        (void)key; 
        (void)cl;

        Seq_T seq = *(Seq_T*)value;
        for (int i = 0; i < Seq_length(seq); i++)
                free(Seq_get(seq, i));

        Seq_free(&seq);
}


void write_section(const void* key, void ** val, void * cl)
{
        (void)key;

        Seq_T sections = *(Seq_T*)val;
        FILE* output = cl;

        for (int i = 0; i < Seq_length(sections); i++) {
                uint32_t word = *(uint32_t*)Seq_get(sections, i);
		for (int j = 4; j > 0; j--) {
			fputc(Bitpack_getu(word, 8, (j - 1) * 8), output); 
		}
        }
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

        const char* section_atom = Atom_string(section);

        Seq_T value = Seq_new(10);
        Table_put(to_return->table, section_atom, value);
        to_return->current = NULL;

        return to_return;
}

void Umsections_free(Umsections_T *asmp)
{
        Table_map((*asmp)->table, table_free, NULL);
        Table_free(&(*asmp)->table);
        free(*asmp);
}

// call the assembler's error function, using the error state
//   passed in at creation time 
int Umsections_error(Umsections_T asm, const char *msg)
{
        return asm->error(asm->errstate, msg);
}


void Umsections_section(Umsections_T asm, const char *section)
{
        if (Table_get(asm->table, section) == NULL) {
                const char* section_atom = Atom_string(section);
                Table_put(asm->table, section_atom, NULL);
        }

        const char* current = Atom_string(section);
        asm->current = current;        
}

void Umsections_emit_word(Umsections_T asm, Umsections_word data)
{
        Seq_T sections = Table_get(asm->table, asm->current);
        Umsections_word *word = malloc(sizeof(Umsections_word));
        *word = data;
        Seq_addhi(sections, word);
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
        if (!section_exists(asm, name)) {
                const char* msg = Fmt_string("Section %s does NOT exist!\n",
                        name);
                Umsections_error(asm, msg);
        }
        else {
                const char* section_atom = Atom_string(name);
                to_return = Seq_length(Table_get(asm->table, section_atom));
        }
        return to_return;
}

Umsections_word Umsections_getword(Umsections_T asm, const char *name, int i)
{
        if (!section_exists(asm, name)) {
                const char* msg = Fmt_string("Section %s does NOT exist!\n",
                        name);
                Umsections_error(asm, msg);
        }
        if (i >= Umsections_length(asm, name)) {
                const char* msg = Fmt_string("OUT OF BOUNDS!");
                Umsections_error(asm, msg);
        }
        const char* section_atom = Atom_string(name);
        return *(Umsections_word*)Seq_get(
                Table_get(asm->table, section_atom), i);
}

void Umsections_putword(Umsections_T asm, const char *name, int i,
                        Umsections_word w)
{
        if (!section_exists(asm, name)) {
                const char* msg = Fmt_string("Section %s does NOT exist!\n",
                        name);
                Umsections_error(asm, msg);
        }
        if (i >= Umsections_length(asm, name)) {
                const char* msg = Fmt_string("OUT OF BOUNDS!");
                Umsections_error(asm, msg);
        }

        const char* section_atom = Atom_string(name);
        *(Umsections_word*)Seq_get(
                Table_get(asm->table, section_atom), i) = w;
}

void Umsections_write(Umsections_T asm, FILE *output)
{
        
        Table_map(asm->table, write_section, output);
      
}
