#include "ummacros.h"
#include "um-opcode.h"
#include "bitpack.h"
#include "umsections.h"

/* STATIC HELPERS */

static Umsections_word um_op(Um_Opcode op, Ummacros_Reg ra, 
		Ummacros_Reg rb, Ummacros_Reg rc) 
{     
	Umsections_word inst = 0;

	inst = Bitpack_newu(inst, 4, 28, (unsigned)op);
	inst = Bitpack_newu(inst, 3, 6, (unsigned)ra);
	inst = Bitpack_newu(inst, 3, 3, (unsigned)rb);
	inst = Bitpack_newu(inst, 3, 0, (unsigned)rc);

	return inst;
}

static Umsections_word loadval(Ummacros_Reg ra, Umsections_word val) 
{
        Umsections_word inst = 0;

        inst = Bitpack_newu(inst, 4, 28, (unsigned)LV);
        inst = Bitpack_newu(inst, 3, 25, (unsigned)ra);
        inst = Bitpack_newu(inst, 25, 0, (unsigned)val);
 
        return inst;
}

void mov(Umsections_T asm, Ummacros_Reg A, Ummacros_Reg B)
{
        Umsections_emit_word(asm, um_op(CMOV, A, B, 1));
}

void com(Umsections_T asm, Ummacros_Reg A, Ummacros_Reg B)
{
        Umsections_emit_word(asm, um_op(NAND, A, B, B));
}

void neg(Umsections_T asm, Ummacros_Reg A, Ummacros_Reg B, int temporary)
{
        com(asm, A, B);
        mov(asm, temporary, 1);
        Umsections_emit_word(asm, um_op(ADD, A, A, temporary));
}

void sub(Umsections_T asm, Ummacros_Reg A, Ummacros_Reg B, 
        Ummacros_Reg C, int temporary)
{
        neg(asm, A, B, temporary);
        Umsections_emit_word(asm, um_op(ADD, C, C, A));
}

void and(Umsections_T asm, Ummacros_Reg A, Ummacros_Reg B, Ummacros_Reg C)
{
        Umsections_emit_word(asm, um_op(NAND, A, B, C));
        com(asm, B, A); 
}

void or(Umsections_T asm, Ummacros_Reg A, Ummacros_Reg B, 
        Ummacros_Reg C, int temporary)
{
        Umsections_emit_word(asm, um_op(NAND, A, B, B));
        Umsections_emit_word(asm, um_op(NAND, temporary, C, C));
        Umsections_emit_word(asm, um_op(NAND, B, A, temporary));
}

void loadv(Umsections_T asm, Ummacros_Reg A, Umsections_word val)
{
        Umsections_emit_word(asm, loadval(A, val));
}


/* Emit a macro instruction into 'asm', possibly overwriting temporary
	 register. Argument of -1 means no temporary is available.
	 Macro instructions include MOV, COM, NEG, SUB, AND, and OR.
	 If a temporary is needed but none is available, Umsections_error(). */
void Ummacros_op(Umsections_T asm, Ummacros_Op operator, int temporary,
				Ummacros_Reg A, Ummacros_Reg B, Ummacros_Reg C)
{
	switch (operator)
	{
		case MOV :
			mov(asm, A, B);

		case COM :
			com(asm, A, B);
		
                case NEG :
                        if (temporary == -1)
                                fprintf(stderr, "ERROR!\n");
                        
                        neg(asm, A, B, temporary);
		
                case SUB :
			if (temporary == -1)
				fprintf(stderr, "ERROR!\n");

                        sub(asm, A, B, C, temporary);
		
		case AND :
                        and(asm, A, B, C);
		
		case OR :
			if (temporary == -1)
				fprintf(stderr, "ERROR!\n");

			or(asm, A, B, C, temporary);
		}
}

  /* Emit code to load literal k into register A. 
         Must work even if k and ~k do not fit in 25 bits---in which
         case temporary register may be overwritten.  Call the
         error function if temporary is needed but is supplied as -1 */
void Ummacros_load_literal(Umsections_T asm, int temporary, Ummacros_Reg A, 
                                uint32_t k)
{
        if (!(k >> 25 & ~0)) //DOES FIT IN 25 BITS
                loadv(asm, A, k);
        else if (!(~k >> 25 & ~0)) //K's COM DOES FIT IN 25 BITS
                {
                        loadval(A, ~k);
                        com(asm, A, A);
                        
                }
                
        else if (temporary == -1)
                        fprintf(stderr, "ERROR!\n");
        
        else {
                uint32_t upper = Bitpack_getu(k, 7, 25);
                uint32_t lower = Bitpack_getu(k, 25, 0);
                upper = upper << 25;
                loadval(A, upper);
                loadval(temporary, lower);
                Umsections_emit_word(asm, um_op(ADD, A, A, temporary));
        }

}

static void add_label(Seq_T stream, int location_to_patch, int label_value)
{
        Um_instruction inst = get_inst(stream, location_to_patch);
        unsigned k = Bitpack_getu(inst, 25, 0);
        inst = Bitpack_newu(inst, 25, 0, label_value + k);
        put_inst(stream, location_to_patch, inst);
}