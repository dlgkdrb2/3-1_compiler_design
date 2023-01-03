#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <Windows.h> // gotoxy() 함수를 사용하기 위한 헤더

#define Max_symbols 200 // Number of terminals and nonterminals must be smaller than this number.
#define Max_size_rule_table 100  // actual number of rules must be smaller than this number.
#define Max_string_of_RHS 20 // 룰 우측편의 최대 길이
#define MaxNumberOfStates 200 // 최대 가능한 스테이트 수

typedef struct tkt { // 하나의 토큰을 나타내는 구조체.
	int index;  // index : 토큰의 종류를 나타냄, 즉 토큰 번호를 말함.
	char typ[16];
	char sub_kind[3];  // relop 토큰인 경우 다시 구분하는데 이용됨.
						 // NUM 토큰의 양의 정수인지("in") 실수인지를("do") 구분.
	int sub_data; // ID 토큰의 심볼테이블엔트리 번호, 양의 정수 값을 저장함.
	double rnum;  // 실수인 경우 
	char str[20]; // 토큰을 구성하는 스트링.  // 아마도 이 필드는 이용되지 않는 것 같음.
} tokentype;

typedef struct ssym {
	int kind; // 0 if terminal; 1 if nontermial; -1 if dummy symbol indicating the end mark.
	int no; // 심볼 고유번호
	char str[30]; // 심볼의 이름 (주: grammar 정의 화일에서 읽어 온 이름임)
} sym; // 문법 심볼 정의

// Rules are represented as follows
typedef struct struc_rule { // rule 한 개의 정의
	sym leftside;
	sym rightside[Max_string_of_RHS];
	int rleng;  // RHS 의 심볼의 수. 0 is given if right hand side is epsilon
}ty_rule;

// Item list 에서 사용할 구조체
typedef struct struc_itemnode* ty_ptr_item_node;
typedef struct struc_itemnode {
	int RuleNum;
	int DotNum;
	ty_ptr_item_node link;
}ty_item_node;

// state node : Goto graph에서 사용할 state node 구조체
typedef struct struc_state_node* ty_ptr_state_node;
typedef struct struc_state_node {
	int id;  // 자신의 ID(번호)
	int item_cnt;  // 보유하고 있는 Item 들의 수
	ty_ptr_item_node item_start;  // 보유하고 있는 Item_list의 시작주소
	ty_ptr_state_node next; // 다음 state로의 link
} ty_state_node;

typedef struct struc_arc_node* ty_ptr_arc_node;
typedef struct struc_arc_node {
	int from_state;
	int to_state;
	sym gram_sym;
	ty_ptr_arc_node link;
} ty_arc_node;

// types for goto graph
typedef struct struc_goto_graph* ty_ptr_goto_graph;
typedef struct struc_goto_graph {
	ty_ptr_arc_node Arc_list;
	ty_ptr_state_node State_Node_List;
}ty_goto_graph; 

// Rules are represented as follows
typedef struct orule {  // rule 한 개의 정의.
	sym leftside;
	sym rightside[Max_string_of_RHS];
	int  rleng;  // RHS 의 심볼의 수.  0 is given if righthand side is epsilon.
} onerule;
typedef struct twoints {
	int cr;
	int elm;
} element_stack;
typedef struct {
	int r, X, i, Yi;
} type_recompute;

typedef struct cell_action_tbl {
	char Kind; // s, r, a, e 중 한 글자
	int num; // s 이면 스테이트 번호, r 이면 룰번호를 나타냄
}ty_cell_action_table;

// 파스트리 노드 정의, 노드 문법심볼을 가짐. 그런데, 이 노드는 LR stack에도 넣음
// 그런데 LR stack 에는 state 번호도 들어가야 함. 그래서 node 가 문법 심볼과 state를 넣는 2가지 용도로 사용함
typedef struct struc_tree_node* ty_ptr_tree_node;
typedef struct strc_tree_node {
	sym nodesym; // 단말기호나 비단말기호를 저장
	int child_cnt; // number of children
	ty_ptr_tree_node children[10]; // 자식 노드들에 대한 포인터
	int rule_no_used; // 비단말기호 노드인 경우 이 노드를 만드는데 사용된 룰번호
}ty_tree_node;

// LR 파서가 사용하는 stack의 한 원소
// state 정보를 가진다면: state >= 0 이상(&& symbol_node 는 NULL); 문법심볼 정보라면 state== -1
typedef struct struc_stk_element {
	int state; // state 번호를 나타냄
	ty_ptr_tree_node symbol_node; // 문법 심볼을 가지는 트리노드에 대한 포인터
}ty_stk_element;

ty_ptr_state_node State_Node_List_Header;
ty_ptr_arc_node Arc_List_Header;

int Max_terminal; // 8 the actual number of terminals of the grammar including $
int Max_nonterminal; // 4 the actual number of nonterminals of the grammar
sym Nonterminals_list[Max_symbols]; // (문법화일에 정의된) 모든 비단말기호들
sym Terminals_list[Max_symbols]; // (문법화일에 정의된) 모든 단말기호들

int Max_rules; // 9 문법 화일에 정의된 규칙의 총 수
ty_rule rules[Max_size_rule_table]; // 규칙 저장 테이블

// goto_graph 구조체에 대한 포인터 (이 구조체는 arc list와 state list의 헤더들을 가짐)
ty_ptr_goto_graph Header_goto_graph = NULL; 
int total_num_states = 0; // the actual number of states of go-to graph
int total_num_arcs = 0; // the actual number of arcs

int First_table[Max_symbols][Max_symbols]; // actual region:  Max_nonterminal  X (MaxTerminals+2)
int Follow_table[Max_symbols][Max_symbols]; // actual region:  Max_nonterminal  X (MaxTerminals+1)
int done[Max_symbols];	// 비단말기호의 first 계산 완료 플래그
int done_follow[Max_symbols];  // 비단말기호의 follow 계산 완료 플래그

type_recompute recompute_list[500];    // recompute_list point list
int num_recompute = 0; // number of recompute_list points in recompute_list.
type_recompute a_recompute;

int ch_stack[100] = { -1, }; //call history stack. -1 은 더미 값. 초기화 안해도 상관없다.
int top_stack = -1;

ty_cell_action_table action_table[MaxNumberOfStates][Max_symbols]; // the number of columns actually used
int goto_table[MaxNumberOfStates][Max_symbols]; // the number of columns actually used is MaxNonterminal

ty_stk_element LR_stack[1000]; // declaration of LR parsing stack
int top_LR_stack = -1; // top of stack

ty_ptr_tree_node Root_parse_tree = NULL; // pointer to the root node of the parse tree
int last_y; // 파스트리 그리기에서 사용하는 전역변수 (바로 전에 출력한 노드의 y 좌표를 가짐)

FILE* fps; // file pointer to a source program file

// Is nonterminal Y in recompute list?
int is_nonterminal_in_stoplist(int Y) {
	int i;
	for (i = 0; i < num_recompute; i++) {
		if (recompute_list[i].X == Y)
			return 1;
	}
	return 0;
}  // end of is_nonterminal_in_stoplist

// is a nonterminal in ch_stack?
int nonterminal_is_in_stack(int Y) {
	int i;
	if (top_stack >= 0) {
		for (i = 0; i <= top_stack; i++)
			if (ch_stack[i] == Y)
				return 1;
	}
	return 0;
} // end of nonterminal_is_in_stack

//  first computation of one nonterminal with capability of dealing with case-2.
int first(sym X) {              // assume X is a nonterminal.
	int i, j, k, r, rleng;
	sym Yi;
	int Yi_has_epsilon;

	top_stack++;
	ch_stack[top_stack] = X.no;  // push to the stack.

	for (r = 0; r < Max_rules; r++) {
		if (rules[r].leftside.no != X.no)
			continue; // skip this rule since left side is not X.
		rleng = rules[r].rleng;
		if (rleng == 0) {
			First_table[X.no][Max_terminal] = 1; // 입실론을 first 에  추가.
			continue;  // 다음 룰로 간다.
		}

		for (i = 0; i < rleng; i++) {
			Yi = rules[r].rightside[i];	// Yi 는 룰의 우측편의 위치 i 의 심볼
			if (Yi.kind == 0) {	// Yi is terminal
				First_table[X.no][Yi.no] = 1;	// Yi를 X의 first 로 추가.
				break;	// exit this loop to go to next rule.
			}
			// Now, Yi is nonterminal.
			if (X.no == Yi.no) {
				if (First_table[X.no][Max_terminal] == 1)
					continue;  // Yi 우측편을 처리하게 함.
				else
					break; // Yi 의 우측은 무시하고 다음 룰로 간다.
			}

			if (done[Yi.no] == 1) {	// if done of Yi is 1
				if (is_nonterminal_in_stoplist(Yi.no)) {	// Yi 가 좌심볼인 재계산점이 존재하면,
					a_recompute.r = r; a_recompute.X = X.no; a_recompute.i = i; a_recompute.Yi = Yi.no;
					recompute_list[num_recompute] = a_recompute;	// 재계산점 [r,X,i,Yi] 를 넣는다.
					num_recompute++;
					// 그리고 아래 ||A|| 로 간다.
				}
			}
			else {	// done of Yi == 0이다.
				if (nonterminal_is_in_stack(Yi.no)) {	// Yi 가 ch_stack 에 있다면
					a_recompute.r = r; a_recompute.X = X.no; a_recompute.i = i; a_recompute.Yi = Yi.no;
					recompute_list[num_recompute] = a_recompute;	// 재계산점 [r,X,i,Yi] 를 넣는다.
					num_recompute++;
					// 그리고 아래 ||A|| 로 간다.
				}
				else {	// Yi 의 first 를 계산한 적이 없다. 
					first(Yi);	// Yi 의 first 를 계산하여 First_table 에 등록함.
					if (is_nonterminal_in_stoplist(Yi.no)) {	// Yi가 좌심볼인 재계산점이 있다면,
						a_recompute.r = r; a_recompute.X = X.no; a_recompute.i = i; a_recompute.Yi = Yi.no;
						recompute_list[num_recompute] = a_recompute;	// 재계산점 [r,X,i,Yi] 를 넣는다.
						num_recompute++;
					}
				}  // else
			} // else
			// ||A||: Yi 의 비-입실론 first 를 first_X 에 반영. 입실론이 있다면 다음 심볼 처리로 아니면 다음 룰 처리로 간다.
			int n;
			for (n = 0; n < Max_terminal; n++) {
				if (First_table[Yi.no][n] == 1)
					First_table[X.no][n] = 1;
			}
			if (First_table[Yi.no][Max_terminal] == 0)	// 입실론이 Yi 의 first 에 없으므로.
				break; // 다음 룰로 간다.
		} // for (i=0
		// break 의 수행으로 여기로 온다면 i != rleng 이다. 그렇지 않다면 i==rleng 이고,
		// 이 말은 r 규칙의 우측편 모두가 입실론을 가지고 있다. 따라서 X 도 입실론을 가져야 한다.
		if (i == rleng)
			First_table[X.no][Max_terminal] = 1;  // X 의 first 가 입실론을 가지게 한다.
	} // for (r=0

	done[X.no] = 1;  // X 의 done (first 계산완료)을 1 로 한다.

	// pop stack.
	ch_stack[top_stack] = -1;  // dummy 값을 넣는다.
	top_stack--;
} // end of first

// 모든 비단말기호의 first 를 구하는 함수. 함수 first 호출하여 계산한다.
void first_all() {
	int i, j, r, m, A, k, n, Xno;
	sym X, Y;

	// 먼저 모든 비단말기호들의 first 를 구하여 First_table 에 기록한다.
	for (i = 0; i < Max_nonterminal; i++) {
		X = Nonterminals_list[i];
		if (done[i] == 0) { // 아직 이 비단말기호를 처리하지 않음.
			top_stack = -1;
			first(X);
		}
		if (top_stack != -1) {
			printf("Logic error. stack top should be -1.\n");
			getchar();  // 일단 멈추게 함.
		}
	} // for

	// 재계산점 처리
	int change_occurred;
	type_recompute recom;

	while (1) {
		change_occurred = 0;
		for (m = 0; m < num_recompute; m++) {
			recom = recompute_list[m];
			r = recom.r; Xno = recom.X; i = recom.i; A = recom.Yi;
			k = rules[r].rleng;
			for (j = i; j < k; j++) {
				Y = rules[r].rightside[j];
				if (Y.kind == 0) {  // 단말기호이면,
					First_table[Xno][Y.no] = 1;
					break;  // 규칙 r 의 처리 종료하고, 다음 재계산점으로 간다.
				}
				// 여기 오면 Y는 비단말기호임. Y 의 first 를 모두 X 의 first로 등록한다(입실론 제외).
				for (n = 0; n < Max_terminal; n++) {
					if (First_table[Y.no][n] == 1) {
						if (First_table[Xno][n] == 0)
							change_occurred = 1; // 0 인 것이 1 로 변경됨. 즉 first 가 추가가 됨.
						First_table[Xno][n] = 1;
					}  // if
				}  // for(n=0
				if (First_table[Y.no][Max_terminal] == 0)	// Y 의 first 에 입실론이 없다면,
					break;
			} // for (j=i
			if (j == k) {  // 이것이 참이면, 룰의 우측의 모든 심볼이 입실론을 가짐.
				if (First_table[Xno][Max_terminal] == 0)
					change_occurred = 1;
				First_table[Xno][Max_terminal] = 1;  // 입실론을 넣어 줌.
			} // if
		} // for (m=0
		if (change_occurred == 0)
			break;	// 재계산점 처리에서 변화를 주지 못함.
	} // while
} // end of first_all


// alpha is an arbitrary string of terminals or nonterminals. 
// A dummy symbol with kind = -1 must be place at the end as the end marker.
// length: number of symbols in alpha.
void first_of_string(sym alpha[], int length, int first_result[]) {
	int i, k;
	sym Yi;
	for (i = 0; i < Max_terminal + 1; i++)
		first_result[i] = 0; // initialize the first result of alpha

	// Let alpha be Y0 Y1 ... Y(length-1)

	for (i = 0; i < length; i++) {
		Yi = alpha[i];
		if (Yi.kind == 0) {  // Yi is terminal
			first_result[Yi.no] = 1;
			break;
		}
		else {  // 여기 오면 Yi 는 비단말기호이다.
			for (k = 0; k < Max_terminal; k++)	 // copy first of Yi to first of alpha
				if (First_table[Yi.no][k] == 1) first_result[k] = 1;
			if (First_table[Yi.no][Max_terminal] == 0)
				break; // first of Yi does not have epsilon. So forget remaining part.	
		}
	} // for 
	if (i == length)
		first_result[Max_terminal] = 1;  // epsilon 을 넣는다.
} // end of function first_of_string

// This function computes the follow of a nonterminal with index X.
int follow_nonterminal(int X) {
	int i, j, k, m;
	int first_result[Max_symbols]; // one row of First table.
	int leftsym;
	sym SymString[Max_string_of_RHS];

	for (i = 0; i < Max_terminal + 1; i++)
		first_result[i] = 0; // initialization.

	for (i = 0; i < Max_rules; i++) {
		leftsym = rules[i].leftside.no;
		for (j = 0; j < rules[i].rleng; j++)
		{    //  the symbol of index j of the RHS of rule i is to be processed in this iteration
			if (rules[i].rightside[j].kind == 0 || rules[i].rightside[j].no != X)
				continue; // not X. so skip this symbol j.
			// Now, position j has a nonterminal which is equal to X.
			if (j < rules[i].rleng - 1) {  // there are symbols after position j in RHS of rule i.
				m = 0;
				for (k = j + 1; k < rules[i].rleng; k++, m++) SymString[m] = rules[i].rightside[k];
				SymString[m].kind = -1;  // end of string marker.
				first_of_string(SymString, m, first_result); // Compute the first of the string after position j of rule i.
																		   // the process result is received via first_result.
				for (k = 0; k < Max_terminal; k++) // Copy the first symbols of the remaining string(except eps) to the Follow of X.
					if (first_result[k] == 1)
						Follow_table[X][k] = 1;
			} // if

			if (j == rules[i].rleng - 1 || first_result[Max_terminal] == 1) // j is last symbol or first result has epsilon
			{
				if (rules[i].leftside.no == X)
					continue; // left symbol of this rule == X. So no need of addition.
				if (Follow_table[leftsym][Max_terminal] == 0) // the follow set of the left sym of rule i was not computed.
					follow_nonterminal(leftsym);		// compute it.
				for (k = 0; k < Max_terminal; k++) // add follow terminals of left side symbol to follow of X (without epsil).
					if (Follow_table[leftsym][k] == 1)
						Follow_table[X][k] = 1;
			}
		} // end of for j=0.
	} // end of for i
	Follow_table[X][Max_terminal] = 1;  // put the completion mark for this nonterminal.
	return 1;
} // end of function follow_nonterminal.



void read_grammar(char grammar_file_name[50]) {
	FILE* fs = fopen("G_arith_no_LR.txt", "r");
	if (fs == NULL) {
		printf("file open fail");
		return;
	}
	char line[100];
	char temp[100];
	fgets(line, 100, fs);
	fgets(line, 100, fs);
	Max_nonterminal = 0;
	Max_terminal = 0;
	Max_rules = 0;
	// nonterminal 읽어오기
	fgets(line, 100, fs);
	int i = 0;
	while (1) {
		while (line[i] == ' ' || line[i] == '\t') i++;
		if (line[i] == '\n') break;
		int k = 0;
		while (line[i] != ' ' && line[i] != '\t' && line[i] != '\n') {
			temp[k] = line[i]; i++; k++;
		} 
		temp[k] = '\0';
		strcpy(Nonterminals_list[Max_nonterminal].str, temp);
		Nonterminals_list[Max_nonterminal].kind = 1;
		Nonterminals_list[Max_nonterminal].no = Max_nonterminal;
		Max_nonterminal++;
	}
	// terminal 읽어오기
	fgets(line, 100, fs);
	i = 0;
	while (1) {
		while (line[i] == ' ' || line[i] == '\t') i++;
		if (line[i] == '\n') break;
		int k = 0;
		while (line[i] != ' ' && line[i] != '\t' && line[i] != '\n') {
			temp[k] = line[i]; i++; k++;
		}
		temp[k] = '\0';
		strcpy(Terminals_list[Max_terminal].str, temp);
		Terminals_list[Max_terminal].kind = 0;
		Terminals_list[Max_terminal].no = Max_terminal;
		Max_terminal++;
	}
	// rule 읽어오기
	fgets(line, 100, fs);
	int nRule = 0, location, num = 0, nSym;
	i = 0;
	while (1) {
		if (fgets(line, 100, fs) == NULL)
			break;
		location = 0, i = 0, nSym = 0;
		while (1) {
			if (line[i] == '\0') break;
			while (line[i] == ' ' || line[i] == '\t' || line[i] == '-') {
				if (line[i] == ' ' || line[i] == '\t') i++;
				else if (line[i] == '-' && line[i + 1] == '>') i = i + 2;
				else break;
			}
			if (line[i] == '\n') break;
			int k = 0;
			while (line[i] != ' ' && line[i] != '\t' && line[i] != '\n') {
				temp[k] = line[i]; i++; k++;
				if (line[i] == '\0') break;
			}
			temp[k] = '\0';
			if (location == 1) { // rightside
				num = 0;
				while (1) {
					if (!strcmp(temp, Nonterminals_list[num].str)) { // 비단말인지 
						strcpy(rules[nRule].rightside[nSym].str, temp);
						rules[nRule].rightside[nSym].kind = 1;
						rules[nRule].rightside[nSym].no = Nonterminals_list[num].no;
						nSym++;
						break;
					}
					else if (!strcmp(temp, Terminals_list[num].str)) { // 단말인지
						strcpy(rules[nRule].rightside[nSym].str, temp);
						rules[nRule].rightside[nSym].kind = 0;
						rules[nRule].rightside[nSym].no = Terminals_list[num].no;
						nSym++;
						break;
					}
					else if (!strcmp(temp, "epsilon")) { // 입실론인지
						strcpy(rules[nRule].rightside[nSym].str, temp);
						rules[nRule].rightside[nSym].kind = -1;
						rules[nRule].rightside[nSym].no = Terminals_list[num].no;
						nSym++;
						break;
					}
					else num++;
				}
			}
			if (location == 0) { // leftside
				num = 0;
				while (1) {
					if (!strcmp(temp, Nonterminals_list[num].str)) {
						strcpy(rules[nRule].leftside.str, temp);
						rules[nRule].leftside.kind = 1;
						rules[nRule].leftside.no = Nonterminals_list[num].no;
						break;
					}
					else num++;
				}
				location++;
			}
		}
		rules[nRule].rleng = nSym;
		nRule++;
		Max_rules++;
	}
}


int firstCheck = 0;
ty_ptr_item_node closure(ty_ptr_item_node IS) {
	if (IS->DotNum >= rules[IS->RuleNum].rleng) 
			return NULL;
	//if (rules[IS->RuleNum].rightside[IS->DotNum].kind == 0 && IS->DotNum == rules[IS->RuleNum].rleng - 1)
		//return NULL;
	ty_ptr_item_node node = (ty_ptr_item_node)malloc(sizeof(ty_item_node));
	node->link = NULL;
	int exist = 0; // leftside에 같은 nonterminal을 위한 변수
	for (int i = 0; i < rules[IS->RuleNum].rleng; i++) { // IS의 rulNum의 rleng 길이만큼 반복
		if (IS->DotNum == i && rules[IS->RuleNum].rightside[i].kind == 1) { // dot위치랑 같고, 비단말이면
			for (int j = 0; j < Max_rules; j++) { // 룰의 개수만큼 돌려
				if (firstCheck == 0) { // first == closure의 맨처음 넣어주는 거
					node->DotNum = IS->DotNum;
					node->RuleNum = IS->RuleNum;
					node->link = NULL;
					firstCheck = 1;
					node->link = closure(node);
					break;
				}
				if (!strcmp(rules[IS->RuleNum].rightside[i].str, rules[j].leftside.str)) { // 그 비단말이 왼쪽에 있으면
					if (rules[j].rightside[0].kind == -1) { // 입실론이면
						j++;
					}
					//if (CheckExistItem(IS, node) == 0) {
					if (exist) {
						ty_ptr_item_node sub_node = (ty_ptr_item_node)malloc(sizeof(ty_item_node));
						sub_node->link = NULL;
						sub_node->DotNum = 0;
						sub_node->RuleNum = j;

						node->link = sub_node;
						sub_node->link = closure(sub_node);
						//if (sub_node->link->DotNum != 0 && sub_node->link != NULL) sub_node->link = NULL;
					}
					else {
						node->DotNum = 0;
						node->RuleNum = j;
						node->link = NULL;
						exist = 1;
						node->link = closure(node);
						//if (node->link->DotNum != 0 && node->link != NULL) node->link = NULL;
					}
					//}
				}
			}
		}
		else if (IS->DotNum == i && rules[IS->RuleNum].rightside[i].kind == 0) return NULL; // ??????
	}

	return node;
}


// Goto function. Compute the item list which can be reached from IS by sym_val
ty_ptr_item_node GoTo(ty_ptr_item_node IS, sym sym_val) {
	
	if (rules[IS->RuleNum].rleng <= IS->DotNum) return IS;
	if (!IS) return NULL;
	ty_ptr_item_node node = (ty_ptr_item_node)malloc(sizeof(ty_item_node));
	// 반복문만 처리해주면 됨
	while (IS) {  
		int i = 0;
		for (i; i < rules[IS->RuleNum].rleng; i) {
			// IS에서 sym_val이 들어가는 것을 찾고
			if (!strcmp(rules[IS->RuleNum].rightside[i].str, sym_val.str)) {
				// IS.DotNum 과 sym_val의 인덱스가 같으면 
				if (IS->DotNum == i) {
					// IS.DotNum + 1 하고, 이 노드를 closure
					node->DotNum = (IS->DotNum) + 1;
					node->RuleNum = IS->RuleNum;
					 //if (rules[node->RuleNum].rightside[IS->DotNum].kind == 0)	return node->link = NULL;  // 여기서부터 다시 시작해볼것
					
					node->link = closure(node);
					return node;
				}
			}
			else i++;
		}
		IS = IS->link;
	}

	return node;
}

// 두 아이템리스트를 비교한다. 동일하면 1, 다르면 0 을 반환한다.
int is_same_two_itemlists(ty_ptr_item_node list1, ty_ptr_item_node list2) {

	if (list1 == NULL || list2 == NULL) return 0;
	while (list1 != NULL || list2 != NULL) {
		if (list1->DotNum == list2->DotNum) {
			if (list1->RuleNum == list2->RuleNum) {
				list1 = list1->link;
				list2 = list2->link;
			}
			else return 0;
		}
		else return 0;
	}

	return 1;
}

// Goto_graph 를 만드는 함수. 결과를 전역변수 Header_goto_graph  가 가리키게 한다.
void makeGotoGraph(ty_ptr_item_node IS_0) {
	//////
	ty_ptr_state_node start_state = (ty_ptr_state_node)malloc(sizeof(ty_state_node));

	ty_ptr_state_node state_cursur, state_first_cursur;
	ty_ptr_arc_node arc_cursur, arc_first_cursur;
	ty_ptr_item_node item_cursur;

	arc_first_cursur = Arc_List_Header;
	arc_cursur = Arc_List_Header;
	arc_cursur->link = NULL;

	start_state = State_Node_List_Header;
	state_first_cursur = State_Node_List_Header;
	state_cursur = State_Node_List_Header;
	
	// start_state->item_start = GoTo(IS_0, rules[IS_0->RuleNum].rightside[IS_0->DotNum]);
	start_state->item_start = IS_0;
	start_state->id = total_num_states++;
	start_state->item_cnt = 0;
	start_state->next = NULL;

	item_cursur = start_state->item_start;
	arc_first_cursur->link = NULL;

	int from_state_cursur = 1;


	while (1) { // state 변경
		start_state->item_cnt = 0;
		while (item_cursur) { // state->item_start 변경
			ty_ptr_state_node node = (ty_ptr_state_node)malloc(sizeof(ty_state_node));
			ty_ptr_arc_node arc = (ty_ptr_arc_node)malloc(sizeof(ty_arc_node));
			node->item_start = GoTo(item_cursur, rules[item_cursur->RuleNum].rightside[item_cursur->DotNum]);
			if (node->item_start == item_cursur) {
				start_state->item_cnt++;
				break;
			}
			if (node->item_start == NULL) {}
			else {
				arc->from_state = start_state->id;
				int check = 0;
				// node : 새로만든 goto
				// state_cursur : 원래 있던 state 들을 가리키는 cursur

				// arc : 새로만든 arc
				// arc_cursur : 원래 있던 arc 들을 가리키는 cursur 
 				while (state_first_cursur) { // 같은 state가 존재하는지 확인
					check = is_same_two_itemlists(node->item_start, state_first_cursur->item_start); // 같으면 1 
					if (check) {
						arc->to_state = state_first_cursur->id;
						arc->gram_sym = rules[state_first_cursur->item_start->RuleNum].rightside[(state_first_cursur->item_start->DotNum) - 1]; // -1을 하는 이유 : goto를 하면서 dotNum + 1이 되어서 지나온 기호를 넣어야 하므로
						
						arc_cursur->link = (ty_ptr_arc_node)malloc(sizeof(ty_arc_node));
						arc_cursur->link->from_state = arc->from_state;
						arc_cursur->link->to_state = arc->to_state;
						arc_cursur->link->gram_sym = arc->gram_sym;
						arc->link = NULL;
						arc_cursur->link->link = arc->link;
						arc_cursur = arc_cursur->link;
						total_num_arcs++;
						break;


					}
					state_first_cursur = state_first_cursur->next;
				}
				// 여기오면 state_cursur->next 가 null 임

				if (!check) { // 같은 state가 없어서 state를 붙여주는 작업
					
					
					state_cursur->next = node;
					state_cursur = state_cursur->next;

					 
					node->next = NULL; // 초기화
					node->id = total_num_states++; // 초기화

					arc->to_state = node->id;
					arc->gram_sym = rules[node->item_start->RuleNum].rightside[(node->item_start->DotNum) - 1];
					

					// 같은 아크가 존재하는지 확인하기 위함
					int arc_exist = 0;
					arc_first_cursur = Arc_List_Header;
					while (arc_first_cursur->link) {
						// from_state가 같고                              // to_state 가 같고                             // arc_str이 같다면
						if (arc_first_cursur->from_state == arc->from_state && arc_first_cursur->to_state == arc->to_state && strcmp(arc_first_cursur->gram_sym.str, arc->gram_sym.str) == 0) {
							arc_exist = 1; // 같은 아크가 존재한다.
							break;
						}
						else arc_first_cursur = arc_first_cursur->link;
					}
					// 여기까지 오면 arc_cursur->link 가 NULL 임
				
						// 같은 아크가 존재하지 않고,
					if (arc_exist == 0) {
						arc_cursur->link = (ty_ptr_arc_node)malloc(sizeof(ty_arc_node));
						arc_cursur->link->from_state = arc->from_state;
						arc_cursur->link->to_state = arc->to_state;
						arc_cursur->link->gram_sym = arc->gram_sym;
						arc->link = NULL;
						arc_cursur->link->link = arc->link;
						arc_cursur = arc_cursur->link;
						total_num_arcs++;
					}
				}
			}
			state_first_cursur = State_Node_List_Header;
			item_cursur = item_cursur->link;
			start_state->item_cnt++;
		}

		start_state = start_state->next;
		//from_state_cursur++;
		if (start_state == NULL) break;
		item_cursur = start_state->item_start;
	}
	start_state = state_first_cursur;
	

	// goto_graph 의 전역 헤더변수 (Header_goto_graph) 에 결과를 가리키게 함.
	Header_goto_graph = (ty_ptr_goto_graph)malloc(sizeof(ty_goto_graph));
	Header_goto_graph->State_Node_List = State_Node_List_Header;
	Header_goto_graph->Arc_list = Arc_List_Header;
}

//  goto_graph 를 파일에 출력한다. 주: 파일 "goto_graph.txt" 에 저장함.
void printGotoGraph(ty_ptr_goto_graph gsp) {
	FILE* fp = fopen("output.txt", "w");
	int isPrint;
	for (int i = 0; i <total_num_states; i++) {

		printf("ID[%d] (%d): ", i, gsp->State_Node_List->item_cnt);
		fprintf(fp, "ID[%d] (%d): ", i, gsp->State_Node_List->item_cnt);
		while(gsp->State_Node_List->item_start){
			int j = 0;
			isPrint = 0;
			printf("%s -> ", rules[gsp->State_Node_List->item_start->RuleNum].leftside.str);
			fprintf(fp, "%s -> ", rules[gsp->State_Node_List->item_start->RuleNum].leftside.str);
			for (int k = 0; k < rules[gsp->State_Node_List->item_start->RuleNum].rleng; k++) {
				if (j == gsp->State_Node_List->item_start->DotNum) {
					if (isPrint == 0) {
						printf(".");
						fprintf(fp, ".");
						isPrint = 1;
					}
				}
				printf("%s ", rules[gsp->State_Node_List->item_start->RuleNum].rightside[k].str);
				fprintf(fp, "%s ", rules[gsp->State_Node_List->item_start->RuleNum].rightside[k].str);
			}
			if (isPrint == 0) {
				printf("\b");
				ungetc(' ', fp);
				printf(".");
				fprintf(fp, ".");
			}
			printf("    ");
			fprintf(fp, "    ");
			j++;
			gsp->State_Node_List->item_start = gsp->State_Node_List->item_start->link;
		}
		printf("\n");
		fprintf(fp, "\n");
		gsp->State_Node_List = gsp->State_Node_List->next;
	}
	printf("Total number of states = %d\n\n", total_num_states);
	fprintf(fp ,"Total number of states = %d\n\n", total_num_states);
	printf("Goto arcs:\n");
	fprintf(fp, "Goto arcs:\n");
	printf("From   To   Symbol\n");
	fprintf(fp, "From   To   Symbol\n");
	for (int i = 0; i < total_num_arcs; i++) {
		printf("  %d   %d   %s\n", gsp->Arc_list->link->from_state, gsp->Arc_list->link->to_state, gsp->Arc_list->link->gram_sym.str);
		fprintf(fp, "  %d   %d   %s\n", gsp->Arc_list->link->from_state, gsp->Arc_list->link->to_state, gsp->Arc_list->link->gram_sym.str);
		gsp->Arc_list = gsp->Arc_list->link;
	}
	printf("Total number of arcs = %d\n", total_num_arcs);
	fprintf(fp, "Total number of arcs = %d\n", total_num_arcs);
}

// return 1 if an item (2nd parameter) is in the item list (1st parameter). Otherwise, return 0.
int CheckExistItem(ty_ptr_item_node cs_list, ty_ptr_item_node s_val) {
	ty_ptr_item_node cursor = cs_list;

	while (cursor) {
		if (cursor->RuleNum == s_val->RuleNum && cursor->DotNum == s_val->DotNum)
			return 1;
		cursor = cursor->link;
	} // while : cursor

	return 0;
} // CheckExistItem ()

sym get_next_token(FILE* fps) {
	sym a_terminal_sym;
	tokentype a_tok; // the token produced by the lexical analyer we developed before.
	a_tok = lexan_mini_grammar(fps);
	a_terminal_sym.kind = 0; // this is a terminal symbol
	a_terminal_sym.no = a_tok.Kind;// 단말기호 고유번호
	// small grammar 의 경우 source program 이 단말기호의 이름을 그대로 이용함.
	strcpy(a_terminal_sym.str, a_tok.str);
	return a_terminal_sym;
} // get_next_token

tokentype lexan_mini_grammar() {
	tokentype token;
	int i = 0, j;
	char ch, str[50];

	// tack out one string
	ch = fgetc(fps);
	while (ch == ' ' || ch == '\n' || ch == '\t')
		ch == fgetc(fps);

	while (ch != EOF && ch != ' ' && ch != '\n' && ch != '\t') {
		str[i] = ch;
		i++;
		ch = fgetc(fps);
	}
	str[i] = '\0';

	if (i == 0) { // 한 글자도 못 읽음. 이 경우는 eof가 도달한 경우만 가능
		token.Kind = Max_terminal - 1; // $ 토큰 (맨 마지막 단말기호)을 반환.
		strcpy(token.str, Terminals_list[Max_terminal - 1].str);
		return token;
	}
	else {
		// look up terminals list to decide terminal id number
		for (j = 0; j < Max_terminal; j++) {
			if (strcmp(str, Terminals_list[j].str) == 0) {
				// j가 단말기호의 고유번호임
				token.Kind = j;
				strcpy(token.str, Terminals_list[j].str);
				return token;
			}
		}
		// 없는 토큰. 이경우는 파일 종료로 간주함!
		printf("입력문장에 불법 토큰이 나옴: %s", str);
		ch = getchar();
		token.Kind = -1;
		return token;
	}
}

void make_action_table() {
	ty_cell_action_table a;
	a.Kind;
	a.num;
}

void print_Action_Table() {

}

void make_goto_table() {

}

void print_Goto_Table() {

}

ty_ptr_tree_node parsing_a_sentence(FILE* fps) {

}

void display_tree(ty_ptr_tree_node nptr, int px, int py, int first_child_flag, int last_child_flag, int root_flag) {


}

int main()
{
	int i, j, base_x, base_y;
	sym a_nonterminal = { 1, 0 };
	char grammar_file_name[50];

	strcpy(grammar_file_name, "G_arith_no_LR.txt");
	read_grammar(grammar_file_name);

	State_Node_List_Header = (ty_ptr_state_node)malloc(sizeof(ty_state_node));
	Arc_List_Header = (ty_ptr_arc_node)malloc(sizeof(ty_arc_node));
	ty_ptr_item_node ItemSet0, tptr;

	tptr = (ty_ptr_item_node)malloc(sizeof(ty_item_node));
	tptr->RuleNum = 0;
	tptr->DotNum = 0;
	tptr->link = NULL;

	ItemSet0 = closure(tptr);  // This is state 0.

	makeGotoGraph(ItemSet0);
	printGotoGraph(Header_goto_graph);

	make_action_table();
	print_Action_Table();
	make_goto_table();
	print_Goto_Table();
	fps = fopen("source_arith.txt", "r"); // 소스프로그램을 가진 파일을 연다.
	// 파싱알고리즘을 호출한다.
	Root_parse_tree = parsing_a_sentence(fps); // 소스프로그램에 대한 파싱을 수행.
	fclose(fps);
	// 현재 커서의 좌표를 알아 내어 (base_x, base_y) 에 저장한다.
	CONSOLE_SCREEN_BUFFER_INFO presentCur;
	GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &presentCur);
	base_x = presentCur.dwCursorPosition.X; // 베이스 좌표의 x.
	base_y = presentCur.dwCursorPosition.Y; // 베이스 좌표의 y.
	last_y = base_y;
	// 파싱 결과로 나온 전체 파스트리를 화면에 그려 본다.
	display_tree(Root_parse_tree, base_x, base_y, 0, 0, 1);


	printf("Program terminates.\n");
}