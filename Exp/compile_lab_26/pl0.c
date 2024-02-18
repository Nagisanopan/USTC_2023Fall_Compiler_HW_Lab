#pragma warning(disable:4996)


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "pl0.h"
#include "set.h"

void print_error(int error_type)
{
	int i;

	printf("      ");
	for (i = 1; i <= character_count - 1; i++)
		printf(" ");
	printf("^\n");
	printf("print_error %3d: %s\n", error_type, err_msg[error_type]);
	err_count++;
} 

void getch(void)
{
	if (character_count == line_length)
	{
		if (feof(infile))
		{
			printf("\nPROGRAM INCOMPLETE\n");
			exit(1);
		}
		line_length = character_count = 0;
		printf("%5d  ", current_inst_index);
		while ( (!feof(infile)) && ((next_char = (char)getc(infile)) != '\n'))
		{
			printf("%c", next_char);
			line[++line_length] = next_char;
		}
		printf("\n");
		line[++line_length] = ' ';
	}
	next_char = line[++character_count];
}

void get_symbol(void)
{
	int i, k;
	char id[IDENTIFIER_MAX_LENGTH + 1];

	if (roll_back_flag)
	{
		next_symbol = last_symbol;
		next_num = last_num;
		strcpy(next_id, last_id);
		roll_back_flag = 0;
		return;
	}

	while (next_char == ' ' || next_char == '\t')
		getch();

	if (isalpha(next_char))  /* symbol is a reserved word or an identifier. */
	{
		k = 0;
		do
		{
			if (k < IDENTIFIER_MAX_LENGTH)
				id[k++] = next_char;
			getch();
		} while (isalpha(next_char) || isdigit(next_char));
		id[k] = '\0';
		strcpy(next_id, id);
		reserve_word[0] = next_id;
		i = RESERVE_WORD_TABLE_MAX_LENGTH;
		while (strcmp(next_id, reserve_word[i--]) != 0);
		if (++i)
			next_symbol = reserve_word_symbol[i];  /* symbol is a reserve_word. */
		else
			next_symbol = SYM_IDENTIFIER;  /* symbol is an identifier. */
	}
	else if (isdigit(next_char))  /* symbol is a number. */
	{
		k = next_num = 0;
		next_symbol = SYM_NUMBER;
		do
		{
			next_num = next_num * 10 + next_char - '0';
			k++;
			getch();
		}
		while (isdigit(next_char));
		if (k > DIGIT_MAX_LENGTH)
			print_error(25);  /* The number is too great. */
	}
	else if (next_char == ':')
	{
		getch();
		if (next_char == '=')
		{
			next_symbol = SYM_BECOMES;
			getch();
		}
		else if (next_char == ':')
		{
			next_symbol = SYM_QUOTE;
			getch();
		}
		else
		{
			next_symbol = SYM_NULL;
		}
	}
	else if (next_char == '>')
	{
		getch();
		if (next_char == '=')
		{
			next_symbol = SYM_GEQ;
			getch();
		}
		else
		{
			next_symbol = SYM_GTR;
		}
	}
	else if (next_char == '<')
	{
		getch();
		if (next_char == '=')
		{
			next_symbol = SYM_LEQ;
			getch();
		}
		else if (next_char == '>')
		{
			next_symbol = SYM_NEQ;
			getch();
		}
		else
		{
			next_symbol = SYM_LES;
		}
	}
	else
	{
		i = REVERSE_CHAR_TABLE_MAX_LENGTH;
		reserve_char[0] = next_char;
		while (reserve_char[i--] != next_char);
		if (++i)
		{
			next_symbol = reserve_char_symbol[i];
			getch();
		}
		else
		{
			printf("Fatal print_error: Unknown character.\n");
			exit(1);
		}
	}
}

void look_ahead(void)
{
	last_symbol = next_symbol;
	last_num = next_num;
	strcpy(last_id, next_id);
	get_symbol();
	roll_back_flag = 1;
}

void accept_look_ahead(void)
{
	roll_back_flag = 0;
	get_symbol();
}

void roll_back(void)
{
	char id[IDENTIFIER_MAX_LENGTH + 1];
	int temp = next_symbol;
	next_symbol = last_symbol;
	last_symbol = temp;

	temp = next_num;
	next_num = last_num;
	last_num = temp;

	strcpy(id, last_id);
	strcpy(last_id, next_id);
	strcpy(next_id, id);
}

void gen_inst(int inst_op_code, int inst_level, int inst_address)
{
	if (current_inst_index > INST_MAX_COUNT)
	{
		printf("Fatal print_error: Program too long.\n");
		exit(1);
	}
	code[current_inst_index].func_code = inst_op_code;
	code[current_inst_index].level = inst_level;
	code[current_inst_index].address = inst_address;
	current_inst_index++;
}

void test(symbol_set s1, symbol_set s2, int n)
{
	symbol_set s;

	if (!in_set(next_symbol, s1))
	{
		print_error(n);
		s = unite_set(s1, s2);
		while(!in_set(next_symbol, s))
			get_symbol();
		destroy_set(s);
	}
}

void enter(int kind)
{
	id_mask* mask;

	current_table_index++;
	strcpy(id_table[current_table_index].name, next_id);
	id_table[current_table_index].kind = kind;
	switch (kind)
	{
		case ID_CONSTANT:
			if (next_num > MAX_ADDRESS)
			{
				print_error(25); // The number is too great.
				next_num = 0;
			}
			id_table[current_table_index].value = next_num;
			break;
		case ID_VARIABLE:
			mask = (id_mask*) &id_table[current_table_index];
			mask->level = (short)current_level;
			mask->address = (short)data_alloc_index++;
			break;
		case ID_PROCEDURE:
			mask = (id_mask*) &id_table[current_table_index];
			mask->level = (short)current_level;
			break;
		case ID_ARRAY:
			mask = (id_mask*) &id_table[current_table_index];
			mask->level = (short)current_level;
			mask->address = (short)data_alloc_index++;
			break;
		default:
			break;
	}
}

int position(char* id)
{
	int i;
	strcpy(id_table[0].name, id);
	i = current_table_index + 1;
	while (strcmp(id_table[--i].name, id) != 0);
	return i;
}

void const_declaration(void)
{
	if (next_symbol == SYM_IDENTIFIER)
	{
		get_symbol();
		if (next_symbol == SYM_EQU || next_symbol == SYM_BECOMES)
		{
			if (next_symbol == SYM_BECOMES)
				print_error(1); // Found ':=' when expecting '='.
			get_symbol();
			if (next_symbol == SYM_NUMBER)
			{
				enter(ID_CONSTANT);
				get_symbol();
			}
			else
			{
				print_error(2); // There must be address number to follow '='.
			}
		}
		else
		{
			print_error(3); // There must be an '=' to follow the identifier.
		}
	} else print_error(4);
	 // There must be an identifier to follow 'const', 'var', or 'procedure'.
}

void dim_declaration(int dimension){
	if (dimension > MAX_ARRAY_DIM){
		print_error(0); // Array dimension exceeded.
	}
	get_symbol();
	if (next_symbol == SYM_NUMBER){
		if(next_num >= MAX_ARRAY_VOLUME) print_error(0);
		else{
			id_table[current_table_index].dimension[dimension] = next_num;
		}
	}
	else print_error(0);

	get_symbol();
	if (next_symbol == SYM_RBRACKET) {
		get_symbol();
		if (next_symbol == SYM_LBRACKET){
			id_table[current_table_index].dimension_num++;
			dim_declaration(dimension + 1);
		}
		else if (next_symbol == SYM_COMMA || next_symbol == SYM_SEMICOLON) {
			id_table[current_table_index].dimension[dimension + 1] = 0;
			int array_size = 1;
			for (int i = 0; id_table[current_table_index].dimension[i]; ++i)
				array_size *= id_table[current_table_index].dimension[i];
			data_alloc_index += array_size;
		}
	} 
}

void var_declaration(void)
{	
	int var_pointer_degree = 0;
	while(next_symbol == SYM_TIMES){
		var_pointer_degree++;
		get_symbol();
	} // 获取指针级数
	//  debug: printf("degree:%d\n",var_pointer_degree);
	if (next_symbol == SYM_IDENTIFIER) {
		get_symbol();
		if (next_symbol == SYM_COMMA || next_symbol == SYM_SEMICOLON){
			enter(ID_VARIABLE);
			id_table[current_table_index].pointer_degree = var_pointer_degree;
		}
		else if (next_symbol == SYM_LBRACKET){
			enter(ID_ARRAY);
			id_table[current_table_index].pointer_degree = var_pointer_degree;
			id_table[current_table_index].dimension_num = 1;
			dim_declaration(0);
		}
		else print_error(5);
	}
	else {
		print_error(4); 
	}
}

void list_code(int from, int to)
{
	int i;
	
	printf("\n");
	for (i = from; i < to; i++)
	{
		printf("%5d %s\t%d\t%d\n", i, mnemonic[code[i].func_code], code[i].level, code[i].address);
	}
	printf("\n");
}

void dim_position(symbol_set sym_set, int i, int dimension) {
	if (id_table[i].dimension[dimension] == 0)
		print_error(0);
	if (roll_back_flag == 1) accept_look_ahead();
	else get_symbol();
	symbol_set set = unite_set(sym_set, create_set(SYM_RBRACKET, SYM_NULL));
	expression(set);
	destroy_set(set);
	if (next_symbol == SYM_RBRACKET) {
		gen_inst(OPR, 0, OPR_ADD);
		if (id_table[i].dimension[dimension + 1] == 0)
			gen_inst(LIT, 0, 1);
		else gen_inst(LIT, 0, id_table[i].dimension[dimension + 1]);
		gen_inst(OPR, 0, OPR_MUL);
		
		get_symbol();
		if(next_symbol == SYM_LBRACKET){
			dim_position(sym_set, i, dimension + 1);
		}
	}
}

void expression_calc_offset(void)
{	
	int i = expression_array_mk->dimension_num - e_current_array_visit_layer + 1;
	int result = 1;
	for(i ; i <  expression_array_mk->dimension_num; i++)
	{
		result = result * expression_array_mk->dimension[i];
	}
	gen_inst(LIT, 0 ,result);
}

void factor(symbol_set sym_set)
{
	int i;
	int pointer_degree;
	void term(symbol_set sym_set); //bug_fixed
	symbol_set set, set1;
	id_mask* mk;

	// test(factor_begin_symbol_set, sym_set, 24);

	if (1)
	{
		if (next_symbol == SYM_IDENTIFIER)
		{
			if ((i = position(next_id)) == 0)
			{
				print_error(11); // Undeclared identifier.
				get_symbol();
			}
			else
			{	
				flag_factor:
				switch (id_table[i].kind)
				{	
					id_mask *mk;
					case ID_CONSTANT:
						gen_inst(LIT, 0, id_table[i].value); 
						get_symbol();	// 生成指令：把常数放到栈顶
						break;
					case ID_VARIABLE:
						mk = (id_mask*) &id_table[i];
						gen_inst(LOD, current_level - mk->level, mk->address);
						get_symbol(); // 生成指令：把变量放到栈顶
						break;
					case ID_ARRAY:
						look_ahead();
						if (next_symbol == SYM_LBRACKET) {
							mk = (id_mask*)&id_table[i];
							gen_inst(LIT, 0, 0);
							dim_position(sym_set, i, 0);
							gen_inst(LEA, current_level - mk->level, mk->address);
							gen_inst(OPR, 0, OPR_ADD);
							gen_inst(LODA, 0, 0);
						}
						else 
						{	
							roll_back();
							expression_is_array = 1;
							expression_array_mk = (id_mask*)&id_table[i];
							e_current_array_visit_layer = expression_array_mk->dimension_num;
							gen_inst(LEA, current_level - expression_array_mk->level, expression_array_mk->address);
							get_symbol();
						}
						// else print_error(0); // You cannot access an array directly.
						break;
					case ID_PROCEDURE:
						get_symbol();
						if (next_symbol == SYM_QUOTE){
							get_symbol();
							if (next_symbol != SYM_IDENTIFIER){
								print_error(0);
							}
							else{
								mk = (id_mask*)&id_table[++i];
								int level = mk->level;
 
                                while((strcmp(id_table[i].name,next_id) != 0 || level - mk->level) && i <= current_table_index) {
                                    mk = (id_mask*)&id_table[++i];
                                }
                                if(i > current_table_index){
                                    print_error(11); // Undeclared identifier.
                                }
                                goto flag_factor;
							}
						}
						else
							print_error(21); // Procedure identifier can not be in an expression.
						break;
				}
			}
		}
		else if (next_symbol == SYM_AMPERSAND) 
		{
			get_symbol();
			if (next_symbol == SYM_IDENTIFIER)
			{
 				if ((i = position(next_id)) == 0)
				{
					print_error(11); // Undeclared identifier.
					get_symbol();
				}
				else
				{
					flag_factor2:
					switch (id_table[i].kind)
					{	
						case ID_CONSTANT:
							print_error(0);
							get_symbol();
							break;
						case ID_VARIABLE:
							mk = (id_mask*) &id_table[i];
							gen_inst(LEA, current_level - mk->level, mk->address); // 生成指令：把变量地址放到栈顶
							get_symbol();
							break;
						case ID_ARRAY:
							get_symbol(); 
							if (next_symbol == SYM_LBRACKET) {
								mk = (id_mask*)&id_table[i];
								gen_inst(LIT, 0, 0);
								dim_position(sym_set, i, 0);
								gen_inst(LEA, current_level - mk->level, mk->address);
								gen_inst(OPR, 0, OPR_ADD);
							}
							else print_error(0); 
							break;
						case ID_PROCEDURE:
							get_symbol();
							if(next_symbol == SYM_QUOTE){
								get_symbol();
								if(next_symbol != SYM_IDENTIFIER){
									print_error(0);
								}
								else{
									mk = (id_mask*)&id_table[i];
									int level = mk->level;

								while(((strcmp(id_table[i].name,next_id)!=0 || level-mk->level) && i <= current_table_index)){
									mk = (id_mask*)&id_table[++i];
								}
								if(i > current_table_index)
									print_error(0);
								goto flag_factor2;
								}

							}
							else{
								print_error(0);
							}
							break;
					}
				}
			}
			else if (next_symbol == SYM_QUOTE)
			{
				i = 0;
				id_mask* mk;
				get_symbol();
				if(next_symbol != SYM_IDENTIFIER){
					print_error(26);
				}
				else{
					mk = (id_mask*)&id_table[++i];
					int level=mk->level;

					while((strcmp(id_table[i].name,next_id) != 0 || level-mk->level ) && i<= current_table_index) {
						mk = (id_mask*)&id_table[++i];
					}
					if(i > current_table_index){
						print_error(11); // Undeclared identifier.
					}
					goto flag_factor2;
				}
			}
			else print_error(0);
		}

        else if(next_symbol == SYM_QUOTE){
            i = 0;
            id_mask* mk;
            get_symbol();
            if(next_symbol != SYM_IDENTIFIER){
                print_error(26);
            }
            else{
                mk = (id_mask*)&id_table[++i];
                int level=mk->level;

                while((strcmp(id_table[i].name,next_id) != 0 || level-mk->level) && i <= current_table_index) {
                    mk = (id_mask*)&id_table[i++];
                }
                if(i > current_table_index){
                    print_error(11); // Undeclared identifier.
                }
                goto flag_factor;
            }
        }

		else if (next_symbol == SYM_NUMBER)
		{
			if (next_num > MAX_ADDRESS)
			{
				print_error(25); // The number is too great.
				next_num = 0;
			}
			gen_inst(LIT, 0, next_num); //// 生成指令：把常数放到栈顶
			get_symbol();
		}
		else if (next_symbol == SYM_LPAREN)
		{
			get_symbol();
			set1 = create_set(SYM_RPAREN, SYM_NULL);
			set = unite_set(set1, sym_set);
			expression(set);
			destroy_set(set1);
			destroy_set(set);
			if (next_symbol == SYM_RPAREN)
			{	
				get_symbol();
			}
			else
			{
				print_error(22);
			}
		}
		else if (next_symbol == SYM_MINUS)
		{
			get_symbol();
			factor(sym_set);
			gen_inst(OPR, 0, OPR_NEG);
		}
		else if (next_symbol == SYM_TIMES) // new
		{	
			get_symbol();
			factor(sym_set);
			if(!expression_is_array) 
				gen_inst(LODA, 0, 0);
			else
			{	
				e_current_array_visit_layer--;
				if(e_current_array_visit_layer ==  0)
				{
					gen_inst(LODA, 0, 0);
				}
			}

		}		
		set = create_set(SYM_LPAREN, SYM_NULL);
		// test(sym_set, set, 23);
		destroy_set(set);
		}	
}

void term(symbol_set sym_set)
{
	int mul_op;
	symbol_set set, set1;

	set1 = create_set(SYM_TIMES, SYM_SLASH, SYM_NULL);
	set = unite_set(sym_set, set1);
	factor(set);
	while (next_symbol == SYM_TIMES || next_symbol == SYM_SLASH)
	{
		mul_op = next_symbol;
		get_symbol();
		factor(set);
		if (mul_op == SYM_TIMES)
		{
			gen_inst(OPR, 0, OPR_MUL);
		}
		else
		{
			gen_inst(OPR, 0, OPR_DIV);
		}
	}
	destroy_set(set1);
	destroy_set(set);
}

void expression(symbol_set sym_set)
{
	int add_op;
	symbol_set set, set1;

	set1 = create_set(SYM_PLUS, SYM_MINUS, SYM_NULL);
	set = unite_set(sym_set, set1);
	
	term(set);
	while (next_symbol == SYM_PLUS || next_symbol == SYM_MINUS)
	{	
		if (!expression_is_array)
		{
			add_op = next_symbol;
			get_symbol();
			term(set);
			if (add_op == SYM_PLUS)
			{
				gen_inst(OPR, 0, OPR_ADD);
			}
			else
			{
				gen_inst(OPR, 0, OPR_MIN);
			}	
		}
		else
		{
			add_op = next_symbol;
			get_symbol();
			term(set);
			expression_calc_offset();
			gen_inst(OPR, 0, OPR_MUL);
			gen_inst(OPR, 0, OPR_ADD);
		}
	}
	destroy_set(set1);
	destroy_set(set);
}

void condition(symbol_set sym_set)
{
	int relation_op;
	symbol_set set;

	if (next_symbol == SYM_ODD)
	{
		get_symbol();
		expression(sym_set);
		gen_inst(OPR, 0, 6);
	}
	else
	{
		set = unite_set(relation_symbol_set, sym_set);
		expression(set);
		destroy_set(set);
		if (!in_set(next_symbol, relation_symbol_set))
		{
			print_error(20);
		}
		else
		{
			relation_op = next_symbol;
			get_symbol();
			expression(sym_set);
			switch (relation_op)
			{
				case SYM_EQU:
					gen_inst(OPR, 0, OPR_EQU);
					break;
				case SYM_NEQ:
					gen_inst(OPR, 0, OPR_NEQ);
					break;
				case SYM_LES:
					gen_inst(OPR, 0, OPR_LES);
					break;
				case SYM_GEQ:
					gen_inst(OPR, 0, OPR_GEQ);
					break;
				case SYM_GTR:
					gen_inst(OPR, 0, OPR_GTR);
					break;
				case SYM_LEQ:
					gen_inst(OPR, 0, OPR_LEQ);
					break;
				default:
					break;
			}
		}
	}
}

void statement_calc_offset(void)
{
	int i = statement_array_mk->dimension_num - s_current_array_visit_layer + 1;
	int result = 1;
	for(i ; i <  statement_array_mk->dimension_num; i++)
	{
		result = result * statement_array_mk->dimension[i];
	}
	gen_inst(LIT, 0 ,result);
}

void statement(symbol_set sym_set)
{
	int i, cx1, cx2;
	symbol_set set1, set;

	if (next_symbol == SYM_TIMES) {
		statement_pointer_degree++;
		get_symbol();
		statement(sym_set);
		statement_pointer_degree--;
		if(statement_pointer_degree == 0)
		{
			if (next_symbol == SYM_BECOMES){
				get_symbol();
			}
			expression(sym_set);
			gen_inst(STOA, 0, 0);
		}
		else 
		{
			if (statement_is_array == 0)
			{
				gen_inst(LODA, 0, 0);
			}
			else{
				s_current_array_visit_layer--;
			}
		}		
	}	

	else if (next_symbol == SYM_LPAREN)
	{
		get_symbol();
		set1 = create_set(SYM_RPAREN, SYM_NULL);
		set = unite_set(set1, sym_set);
		statement(set);
		destroy_set(set1);
		destroy_set(set);
		while (next_symbol == SYM_PLUS)
		{
			get_symbol();
			statement_calc_offset();
			if(next_symbol == SYM_NUMBER)
				gen_inst(LIT, 0, next_num);
			gen_inst(OPR, 0, OPR_MUL);
			gen_inst(OPR, 0, OPR_ADD);	
			get_symbol();
		}
		if (next_symbol == SYM_RPAREN)
		{	
			get_symbol();
		}
	}


	else if (next_symbol == SYM_IDENTIFIER)
	{
		// variable assignment
		id_mask* mk;

		if (!(i = position(next_id)))
		{
			print_error(11); 
		}
		else
		{
			flag_statement:
				if (id_table[i].kind == ID_CONSTANT)
				{
					print_error(12); // Illegal assignment.
					i = 0;
				}
				else if (id_table[i].kind == ID_VARIABLE)
				{
					get_symbol();
					if (next_symbol == SYM_BECOMES)
					{
						get_symbol();
					}
					else
					{
						print_error(13); // ':=' expected.
					}

					if (statement_pointer_degree == 0)
					{
						expression(sym_set);
						mk = (id_mask*)&id_table[i];
						if (i)
						{
							gen_inst(STO, current_level - mk->level, mk->address);
						}
					}
					else
					{
						statement_is_array = 0;
						mk = (id_mask*)&id_table[i];
						gen_inst(LOD, current_level - mk->level, mk->address);
					}
				}
				else if (id_table[i].kind == ID_ARRAY)
				{
					get_symbol();
					if (next_symbol == SYM_LBRACKET) 
					{
						mk = (id_mask*)&id_table[i];
						gen_inst(LIT,0,0);
						dim_position(sym_set, i, 0);
						gen_inst(LEA, current_level - mk->level, mk->address);
						gen_inst(OPR, 0, OPR_ADD);
						if (next_symbol == SYM_BECOMES)
						{
							get_symbol();
						}
						else
						{
							print_error(13);	
						}
						expression(sym_set);	
						if (i)
						{
							gen_inst(STOA, 0, 0);
						}
					}
					else 
					{
						statement_array_mk = (id_mask*)&id_table[i];
						s_current_array_visit_layer = statement_array_mk->dimension_num;
						statement_is_array = 1;
						gen_inst(LEA, current_level - statement_array_mk->level, statement_array_mk->address);
					}
				}
				else if (id_table[i].kind == ID_PROCEDURE)
				{
					get_symbol();
					if (next_symbol == SYM_QUOTE)
					{
						get_symbol();
						if (next_symbol != SYM_IDENTIFIER)
						{
							print_error(26);
						}
						else
						{
							i++;
							mk = (id_mask*)&id_table[i];
							int level = mk->level;

							while ((strcmp(id_table[i].name, next_id) != 0 || level - mk->level) && i <= current_table_index)
							{
								i++;
								mk = (id_mask*)&id_table[i];
							}
							if (i > current_table_index)
							{
								print_error(11); // Undeclared identifier.
							}
							goto flag_statement;
						}
					}
					else
					{
						print_error(12); // Procedure identifier can not be in an expression.
					}
			    }
		}
	}
	
	else if (next_symbol == SYM_QUOTE)
	{
		i = 0;
		id_mask* mk;
		get_symbol();
		if (next_symbol != SYM_IDENTIFIER){
			print_error(0);
		}
		else
		{
			mk = (id_mask*)&id_table[++i];
			int level = mk->level;

			while ((strcmp(id_table[i].name,next_id) != 0 || level-mk->level) && i <= current_table_index)
			{
				mk = (id_mask*)&id_table[++i];
			}
			if  (i > current_table_index){
				print_error(0);
			}
			goto flag_statement;
		}
		
	}
	
	else if (next_symbol == SYM_CALL)
	{ // call instruction
		get_symbol();
		if (next_symbol != SYM_IDENTIFIER)
		{
			print_error(14);
		}
		else
		{
			if (! (i = position(next_id)))
			{
				print_error(11);
			}
			else if (id_table[i].kind == ID_PROCEDURE)
			{
				id_mask* mk;
				mk = (id_mask*) &id_table[i];
				gen_inst(CAL, current_level - mk->level, mk->address);
			}
			else
			{
				print_error(15);
			}
			get_symbol();
		}
	} 

	else if (next_symbol == SYM_IF)
	{ // if instruction
		get_symbol();
		set1 = create_set(SYM_THEN, SYM_DO, SYM_NULL);
		set = unite_set(set1, sym_set);
		condition(set);
		destroy_set(set1);
		destroy_set(set);
		if (next_symbol == SYM_THEN)
		{
			get_symbol();
		}
		else
		{
			print_error(16);
		}
		cx1 = current_inst_index;
		gen_inst(JPC, 0, 0);
		statement(sym_set);
		code[cx1].address = current_inst_index;
	}

	else if (next_symbol == SYM_BEGIN)
	{ // begin(顺序) instruction
		get_symbol();
		set1 = create_set(SYM_SEMICOLON, SYM_END, SYM_NULL);
		set = unite_set(set1, sym_set);
		statement(set);
		while (next_symbol == SYM_SEMICOLON || in_set(next_symbol, state_begin_symbol_set))
		{	
			if (next_symbol == SYM_SEMICOLON)
			{	
				expression_is_array = 0;
				get_symbol();
			}
			else
			{
				print_error(10);
			}
			statement(set);
		} // while
		destroy_set(set1);
		destroy_set(set);
		if (next_symbol == SYM_END)
		{
			get_symbol();
		}
		else
		{
			print_error(17);
		}
	}

	else if (next_symbol == SYM_WHILE)
	{ // while instruction
		cx1 = current_inst_index;
		get_symbol();
		set1 = create_set(SYM_DO, SYM_NULL);
		set = unite_set(set1, sym_set);
		condition(set);
		destroy_set(set1);
		destroy_set(set);
		cx2 = current_inst_index;
		gen_inst(JPC, 0, 0);
		if (next_symbol == SYM_DO)
		{
			get_symbol();
		}
		else
		{
			print_error(18);
		}
		statement(sym_set);
		gen_inst(JMP, 0, cx1);
		code[cx2].address = current_inst_index;
	}

	else if (next_symbol == SYM_PRINT) {
		get_symbol();
		if (next_symbol == SYM_LPAREN) {
			get_symbol();
			if (next_symbol == SYM_RPAREN) {
				gen_inst(PRT, 255, 0);
			}
			else {
				set1 = create_set(SYM_RPAREN, SYM_COMMA, SYM_NULL);
				set = unite_set(set1, sym_set);
				expression(set);
				gen_inst(PRT, 0, 0);

				while (next_symbol == SYM_COMMA) {
					get_symbol();
					expression(set);
					gen_inst(PRT, 0, 0);
				}
				destroy_set(set1);
				destroy_set(set);
				// printf("next_symbol_num = %d \n",next_symbol);
				if (next_symbol == SYM_RPAREN) 
					gen_inst(PRT, 255, 0);
				else print_error(22); // Missing ')'.
			}
			get_symbol();
		}
		else print_error(33); // Missing '('.
	}	
}

void block(symbol_set sym_set)
{
	int code_index;
	id_mask* mk;
	int block_data_alloc_index;
	int saved_table_index;
	symbol_set set1, set;

	data_alloc_index = 3;
	block_data_alloc_index = data_alloc_index;
	mk = (id_mask*) &id_table[current_table_index];
	mk->address = (short)current_inst_index;
	gen_inst(JMP, 0, 0);
	if (current_level > MAX_LEVEL)
	{
		print_error(32); // There are too many levels.
	}
	// 以下为主循环，上面都是和代码生成相关的变量
	do
	{
		if (next_symbol == SYM_CONST)
		{ // constant declarations
			get_symbol();
			do
			{
				const_declaration();
				while (next_symbol == SYM_COMMA)
				{
					get_symbol();
					const_declaration();
				}
				if (next_symbol == SYM_SEMICOLON)
				{
					get_symbol();
				}
				else
				{
					print_error(5); // Missing ',' or ';'.
				}
			}
			while (next_symbol == SYM_IDENTIFIER);
		}

		if (next_symbol == SYM_VAR)
		{ // variable declarations
			get_symbol();
			do
			{
				var_declaration();
				while (next_symbol == SYM_COMMA)
				{
					get_symbol();
					var_declaration();
				}
				if (next_symbol == SYM_SEMICOLON)
				{
					get_symbol();
				}
				else
				{
					print_error(5); // Missing ',' or ';'.
				}
			}
			while (next_symbol == SYM_IDENTIFIER);
		}
		block_data_alloc_index = data_alloc_index; // Save data_alloc_index before handling procedure call!
		while (next_symbol == SYM_PROCEDURE)
		{ // procedure declarations
			get_symbol();
			if (next_symbol == SYM_IDENTIFIER)
			{
				enter(ID_PROCEDURE);
				get_symbol();
			}
			else
			{
				print_error(4); // There must be an identifier to follow 'const', 'var', or 'procedure'.
			}


			if (next_symbol == SYM_SEMICOLON)
			{
				get_symbol();
			}
			else
			{
				print_error(5); // Missing ',' or ';'.
			}

			current_level++;
			saved_table_index = current_table_index;
			set1 = create_set(SYM_SEMICOLON, SYM_NULL);
			set = unite_set(set1, sym_set);
			block(set); // 递归调用block，因为每层procedure都是分程序
			destroy_set(set1);
			destroy_set(set);
			current_table_index = saved_table_index;
			current_level--;

			if (next_symbol == SYM_SEMICOLON)
			{
				get_symbol();
				set1 = create_set(SYM_IDENTIFIER, SYM_PROCEDURE, SYM_NULL);
				set = unite_set(state_begin_symbol_set, set1);
				// test(set, sym_set, 6);
				destroy_set(set1);
				destroy_set(set);
			}
			else
			{
				print_error(5); // Missing ',' or ';'.
			}
		}
		data_alloc_index = block_data_alloc_index; // Restore data_alloc_index after handling procedure call!
		set1 = create_set(SYM_IDENTIFIER, SYM_NULL);
		set = unite_set(state_begin_symbol_set, set1);
		// test(set, declare_begin_symbol_set, 7);
		destroy_set(set1);
		destroy_set(set);
	}
	while (in_set(next_symbol, declare_begin_symbol_set));

	code[mk->address].address = current_inst_index;
	mk->address = (short)current_inst_index;
	code_index = current_inst_index;
	gen_inst(INT, 0, block_data_alloc_index);
	set1 = create_set(SYM_SEMICOLON, SYM_END, SYM_NULL);
	set = unite_set(set1, sym_set);
	statement(set);
	destroy_set(set1);
	destroy_set(set);
	gen_inst(OPR, 0, OPR_RET); // return
	// test(sym_set, phi, 8); // test for print_error: Follow the statement is an incorrect symbol.
	list_code(code_index, current_inst_index);
}

int base(const int stack[], int current_level, int level_diff)
{
	int b = current_level;
	
	while (level_diff--)
		b = stack[b];
	return b;
}

void interpret()
{
	int pc;        /* program counter */
	int stack[STACK_SIZE];
	int top;       /* top of stack */
	int b;         /* program, base, and top-stack register */
	instruction i; /* instruction register */

	printf("Begin executing PL/0 program.\n");

	pc = 0;
	b = 1;
	top = 3;
	stack[1] = stack[2] = stack[3] = 0;
	do
	{	
		// printf("pc = %d\ntop = %d\nstack[top] = %d stack[top-1] = %d",pc,top,stack[top],stack[top-1]);
		i = code[pc++];
		switch (i.func_code)
		{
			case LIT:
				stack[++top] = i.address;
				break;
			case OPR:
				switch (i.address) // operator
				{
				case OPR_RET:
					top = b - 1;
					pc = stack[top + 3];
					b = stack[top + 2];
					break;
				case OPR_NEG:
					stack[top] = -stack[top];
					break;
				case OPR_ADD:
					top--;
					stack[top] += stack[top + 1];
					break;
				case OPR_MIN:
					top--;
					stack[top] -= stack[top + 1];
					break;
				case OPR_MUL:
					top--;
					stack[top] *= stack[top + 1];
					break;
				case OPR_DIV:
					top--;
					if (stack[top + 1] == 0)
					{
						fprintf(stderr, "Runtime print_error: Divided by zero.\n");
						fprintf(stderr, "Program terminated.\n");
						continue;
					}
					stack[top] /= stack[top + 1];
					break;
				case OPR_ODD:
					stack[top] %= 2;
					break;
				case OPR_EQU:
					top--;
					stack[top] = stack[top] == stack[top + 1];
					break;
				case OPR_NEQ:
					top--;
					stack[top] = stack[top] != stack[top + 1];
					break;
				case OPR_LES:
					top--;
					stack[top] = stack[top] < stack[top + 1];
					break;
				case OPR_GEQ:
					top--;
					stack[top] = stack[top] >= stack[top + 1];
					break;
				case OPR_GTR:
					top--;
					stack[top] = stack[top] > stack[top + 1];
					break;
				case OPR_LEQ:
					top--;
					stack[top] = stack[top] <= stack[top + 1];
					break;
				} // switch
				break;
			case LOD:
				stack[++top] = stack[base(stack, b, i.level) + i.address];
				break;
			case LEA: // NEW
				stack[++top] = base(stack, b, i.level) + i.address;
				break;
			case LODA: // NEW
				stack[top] = stack[stack[top]];
				break;
			case STOA: // NEW
				stack[stack[top - 1]] = stack[top];
				top -= 2;
				break;
			case PRT: // NEW
				if (i.level == 255) putchar('\n');
				else printf("%d ", stack[top--]);
				break;
			case STO:
				stack[base(stack, b, i.level) + i.address] = stack[top];
				// printf("%d\n", stack[top]);
				top--;
				break;
			case CAL:
				stack[top + 1] = base(stack, b, i.level);
				// generate new block mark
				stack[top + 2] = b;
				stack[top + 3] = pc;
				b = top + 1;
				pc = i.address;
				break;
			case INT:
				top += i.address;
				break;
			case JMP:
				pc = i.address;
				break;
			case JPC:
				if (stack[top] == 0)
					pc = i.address;
				top--;
				break;
	
		}
	}
	while (pc);

	printf("End executing PL/0 program.\n");
}

int main()
{
	FILE* bin_output;
	char s[80];
	int i;
	symbol_set set, set1, set2;

	printf("Please input source file name:\n");
	scanf("%s", s);
	if ((infile = fopen(s, "r")) == NULL)
	{
		printf("File %s can't be opened.\n", s);
		exit(1);
	}

	phi = create_set(SYM_NULL);
	relation_symbol_set = create_set(SYM_EQU, SYM_NEQ, SYM_LES, SYM_LEQ, SYM_GTR, SYM_GEQ, SYM_NULL);
	
	// create begin symbol sets
	declare_begin_symbol_set = create_set(SYM_CONST, SYM_VAR, SYM_PROCEDURE, SYM_NULL);
	state_begin_symbol_set = create_set(SYM_BEGIN, SYM_CALL, SYM_IF, SYM_WHILE, SYM_NULL);
	factor_begin_symbol_set = create_set(SYM_IDENTIFIER, SYM_NUMBER, SYM_LPAREN, SYM_MINUS, SYM_NULL);

	err_count = character_count = current_inst_index = line_length = 0;
	next_char = ' ';

	get_symbol();

	set1 = create_set(SYM_PERIOD, SYM_NULL);
	set2 = unite_set(declare_begin_symbol_set, state_begin_symbol_set);
	set = unite_set(set1, set2);
	block(set);
	destroy_set(set1);
	destroy_set(set2);
	destroy_set(set);
	destroy_set(phi);
	destroy_set(relation_symbol_set);
	destroy_set(declare_begin_symbol_set);
	destroy_set(state_begin_symbol_set);
	destroy_set(factor_begin_symbol_set);

	if (next_symbol != SYM_PERIOD)
		print_error(9);
	if (err_count == 0)
	{
		bin_output = fopen("bin.txt", "w");
		for (i = 0; i < current_inst_index; i++)
			fwrite(&code[i], sizeof(instruction), 1, bin_output);
		fclose(bin_output);
	}
	if (err_count == 0)
		interpret();
	else
		printf("There are %d print_error(s) in PL/0 program.\n", err_count);
	list_code(0, current_inst_index);
	return 0;
}