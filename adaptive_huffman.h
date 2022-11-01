#include <iostream>
#include <vector>
#include <queue>
#include <string>
using namespace std;

struct node{
    node* parent;
    node* leftchild;
    node* rightchild;
    unsigned int frequency;
    unsigned int level;
    unsigned int number;
    string chracter;
    string code;
};

node* findcharacter(node* root, string target_character){ // chracter 있는 노드 찾아서 frequency update 후 주소 반환
    // DFS
    node* temp = root;
    node* target_node = NULL;
    node* target1 = NULL;
    node* target2 = NULL;
    if(temp == NULL){

    }
    else if(temp->leftchild == NULL && temp->rightchild == NULL) {
        if(temp->chracter == target_character) {
            target_node = temp;
        }
    }
    else {
        target1 = findcharacter(temp->leftchild, target_character);
        target2 = findcharacter(temp->rightchild, target_character);
    }
    if(target1 != NULL) target_node = target1;
    else if(target2 != NULL) target_node = target2;
    
    return target_node;
}

bool is_right(node* a){
    node* temp = a;
    bool is_r = false;
    while(1){
        if(temp->level == 1) break;
        else {
            if(temp->level != 2) {
                temp = temp->parent;
            }
            else {
                if(temp->parent->rightchild == temp) {
                    is_r = true;
                }
                else {
                    is_r = false;
                }
                break;
            }
        }
    }
    return is_r;
}

node* searchfarnode(node* temproot, node* a, unsigned int number_node) { // 현재 노드로부터 가장 멀리 떨어진 대상 노드를 찾음
    queue<node> q;
    node* target_node = NULL;
    bool* is_check = new bool[number_node+1];
    bool right = is_right(a);
    unsigned int target_level = a->level;
    q.push(*temproot);
    is_check[temproot->number] = true;
    while(!q.empty()) {
        node x = q.front();
        q.pop();
        if(x.frequency == a->frequency-1 && x.chracter != "NYT" && x.parent != NULL && x.level <= target_level) {
            if(a->parent->number != x.number) {
                target_level = x.level;
                if(x.parent->leftchild->number == x.number)
                    target_node = x.parent->leftchild;
                else 
                    target_node = x.parent->rightchild;    
            }
        }
        if(right) {
            if(x.rightchild != NULL) {
                node y = *x.rightchild;
                if(!is_check[y.number]) {
                    q.push(y);
                    is_check[y.number] = true;
                }
            }
            if(x.leftchild != NULL) {
                node y = *x.leftchild;
                if(!is_check[y.number]) {
                    q.push(y);
                    is_check[y.number] = true;
                }
            }
            if(x.parent != NULL) {
                node y = *x.parent;
                if(!is_check[y.number]) {
                    q.push(y);
                    is_check[y.number] = true;
                }
            }
        }
        else {
            if(x.leftchild != NULL) {
                node y = *x.leftchild;
                if(!is_check[y.number]) {
                    q.push(y);
                    is_check[y.number] = true;
                }
            }
            if(x.rightchild != NULL) {
                node y = *x.rightchild;
                if(!is_check[y.number]) {
                    q.push(y);
                    is_check[y.number] = true;
                }
            }
            if(x.parent != NULL) {
                node y = *x.parent;
                if(!is_check[y.number]) {
                    q.push(y);
                    is_check[y.number] = true;
                }
            }
        }
    }
    delete[] is_check;
    return target_node;
}

node* swapnode(node* a, node* b) {
    if(a->parent != b->parent) {
        if(a->parent->leftchild == a)
            a->parent->leftchild = b;
        else
            a->parent->rightchild = b;

        if(b->parent->leftchild == b)
            b->parent->leftchild = a;
        else
            b->parent->rightchild = a;

        node* temp = a->parent;
        a->parent = b->parent;
        b->parent = temp;

        unsigned int level = a->level;
        a->level = b->level;
        b->level = level;
    }
    else {
        node* temp = a->parent->leftchild;
        a->parent->leftchild = a->parent->rightchild;
        a->parent->rightchild = temp;
    }
    a->parent->frequency = a->parent->leftchild->frequency + a->parent->rightchild->frequency;

    return a->parent;
}

void updatecode(node* tempRoot, string s)
{
	node* root1 = tempRoot; 
	root1->code = s;

	if (root1 == NULL)
	{

	} 
	else if (root1->leftchild == NULL && root1->rightchild == NULL)
	{
		cout << "character:\t" << root1->chracter << "\tcode:\t" << root1->code << "\tfreq:\t" << root1->frequency;
        cout << "\tlevel:\t" << root1->level << "\tnumber:\t" << root1->number << endl; 
	} 
	else
	{
		root1->leftchild->code = s.append("0"); 
		s.erase(s.end() - 1); 
		root1->rightchild->code = s.append("1"); 
		s.erase(s.end() - 1); 

		updatecode(root1->leftchild, s.append("0")); 
		s.erase(s.end() - 1); 
		updatecode(root1->rightchild, s.append("1")); 
		s.erase(s.end() - 1);
	}
}

node* adaptive_huffman(node* temproot, node* search, unsigned int number_node) 
{
    node* object = searchfarnode(temproot, search, number_node);
    node* temp = NULL;
    if(search->parent != NULL) {
        if(object != NULL) {
            temp = swapnode(search, object);
        }
        else {
            temp = search->parent;
            temp->frequency = temp->leftchild->frequency + temp->rightchild->frequency;
        }
    }
    return temp;
}

void get_adaptive_huffman_code(){
    // Initial root node : NYT
    unsigned int number_node = 0;
    node* root = new node;
    root->parent = NULL;
    root->leftchild = NULL;
    root->rightchild = NULL;
    root->chracter = "NYT";
    root->code = "";
    root->level = 1;
    root->number = number_node;

    cout << "----------------------------- Adaptive huffman code -----------------------------" << endl;
    cout << "If you want to quit, type quit\n";
    cout << "Else then, type charcter or string\n";

    string input;

    while(1) {
        cout << "Input:\t";
        getline(cin, input);
        cout << "---------------------------------------------------------------------------------" << endl;

        if(input == "quit")
            break;
        else {
            for(int i=0;i<input.size();i++) { 
                string target_character(1,input.at(i)); 

                node* search = findcharacter(root, target_character);
                if(search != NULL)
                    search->frequency++;
                else {
                    search = findcharacter(root,"NYT");
                    search->leftchild = new node;
                    search->rightchild = new node;

                    // leftchild has to be NYT
                    search->leftchild->chracter = "NYT";
                    search->leftchild->frequency = 0;
                    search->leftchild->level = search->level + 1;
                    number_node++;
                    search->leftchild->number = number_node;
                    search->leftchild->parent = search;
                    search->leftchild->leftchild = NULL;
                    search->leftchild->rightchild = NULL;

                    // rightchild
                    search->rightchild->chracter = target_character;
                    search->rightchild->frequency = 1;
                    search->rightchild->level = search->level + 1;
                    number_node++;
                    search->rightchild->number = number_node;
                    search->rightchild->parent = search;
                    search->rightchild->leftchild = NULL;
                    search->rightchild->rightchild = NULL;

                    // parent update
                    search->chracter = "";
                    search->frequency = 1;
                }
                node* temp = adaptive_huffman(root, search,number_node);
                while(temp != NULL && temp->parent != NULL) {
                    temp = adaptive_huffman(root, temp,number_node);
                }
                updatecode(root,""); // update하고 update된 코드 보여줌
                cout << "---------------------------------------------------------------------------------" << endl;
            }
        }
    }
    delete[] root;
}